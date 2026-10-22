# manual_scpi_test.py — spaced-out manual SCPI test (not the rapid-fire pytest suite).
# Run: python manual_scpi_test.py [ip]
# Sends one command at a time with a short pause, since rapid back-to-back
# queries are what currently trigger the connection drop on this hardware.
#
# If no IP is given on the command line, this scans the local subnet for a
# device answering SCPI on port 5025 and confirms it with *IDN?, so you don't
# have to go dig the IP out of the UART log every time.
#
# NOTE: this used to match a hardcoded MAC address via ARP instead, but the
# MAC is now derived from each board's STM32 unique device ID (see
# w5500_get_mac() in L1_Driver/w5500_port.c) so that multiple boards on the
# same bench don't collide on one fixed MAC — which means there's no longer
# a single fixed MAC for this script to look for. Probing the SCPI port
# directly works regardless of how many distinct boards are on the network;
# if more than one answers, all of them are listed and the first is used.

import ipaddress
import re
import socket
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

PORT = 5025
DELAY = 0.3  # seconds between commands
IDN_PREFIX = "Company,CableTester"


def _local_ipv4_and_guess_subnet():
    """Best-effort: the local IPv4 this machine would use to reach the LAN,
    assumed to be on a /24 (true for both the DHCP and static-fallback
    ranges this project uses: 192.168.x.0/24)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
    finally:
        s.close()
    network = ipaddress.IPv4Network(f"{local_ip}/24", strict=False)
    return local_ip, network


def _read_line(s, deadline_s):
    """Accumulates recv() calls until a newline or the deadline — a single
    recv() can return a partial reply (SCPI_Input may flush its reply across
    more than one write() call), and this hardware also has a known
    intermittent SPI corruption issue that occasionally drops a byte."""
    buf = b""
    end = time.time() + deadline_s
    while time.time() < end:
        s.settimeout(max(0.05, end - time.time()))
        try:
            chunk = s.recv(256)
        except socket.timeout:
            break
        if not chunk:
            break
        buf += chunk
        if b"\n" in buf:
            break
    return buf.decode(errors="replace").strip()


def _clean_close(s, drain_timeout=0.3):
    """Drains any unread data and does a proper shutdown()+close() instead of
    an abrupt close(). IMPORTANT: closing a socket while there's still unread
    data sitting in the OS receive buffer makes the OS send a TCP RST instead
    of a graceful FIN — and the firmware's SCPI server only has ONE TCP
    socket (SCPI_SLOTS=1, see scpi_server.c). A client that RSTs it instead
    of closing cleanly has been observed to wedge that socket until the board
    is reset (confirmed by testing: repeated clean-draining connections work
    fine back-to-back; repeated abrupt closes wedge it after just one)."""
    try:
        s.settimeout(drain_timeout)
        while s.recv(4096):
            pass
    except OSError:
        pass
    try:
        s.shutdown(socket.SHUT_WR)
        s.settimeout(drain_timeout)
        while s.recv(4096):
            pass
    except OSError:
        pass
    try:
        s.close()
    except OSError:
        pass


def _probe_scpi(ip, timeout=0.5):
    # Single attempt, no retry: see _clean_close() for why this is gentle
    # with the one TCP socket the firmware's SCPI server has.
    try:
        s = socket.create_connection((str(ip), PORT), timeout=timeout)
    except OSError:
        return None
    try:
        s.sendall(b"*IDN?\n")
        reply = _read_line(s, timeout)
        return reply or None
    finally:
        _clean_close(s)


def _read_arp_table():
    """Dumps the OS's current ARP cache (Windows `arp -a` format: IP, MAC,
    type). Only shows what's already cached — an address only appears here
    after this host has actually talked to it at least once (which the SCPI
    port scan in find_device_ip() does for every host on the subnet, so by
    the time this runs the cache is reasonably fresh)."""
    try:
        out = subprocess.run(["arp", "-a"], capture_output=True, text=True, timeout=10).stdout
    except OSError:
        return []
    entries = []
    for line in out.splitlines():
        m = re.search(r"(\d{1,3}(?:\.\d{1,3}){3}).*?([0-9a-fA-F]{2}([-:])[0-9a-fA-F]{2}(?:\3[0-9a-fA-F]{2}){4})", line)
        if m:
            entries.append((m.group(1), m.group(2).lower().replace("-", ":")))
    return entries


def print_arp_table(highlight_ip=None):
    """Prints every device currently in the local ARP cache."""
    entries = _read_arp_table()
    print("All ARP entries:")
    if not entries:
        print("  (ARP cache is empty)")
        return
    for ip, mac in sorted(entries, key=lambda e: ipaddress.ip_address(e[0])):
        marker = "  <-- this device" if ip == highlight_ip else ""
        print(f"  {ip:<15s} {mac}{marker}")


def find_device_ip(verbose=True):
    """Scans the local /24 for anything answering SCPI on port 5025 with an
    *IDN? reply starting with IDN_PREFIX. Returns the first match's IP, or
    None if nothing responded."""
    try:
        local_ip, network = _local_ipv4_and_guess_subnet()
    except OSError:
        print("Could not determine local subnet.")
        return None

    print(f"Scanning {network} for a CableTester on port {PORT} (this takes a few seconds)...")
    hosts = [h for h in network.hosts() if str(h) != local_ip]

    found = {}
    with ThreadPoolExecutor(max_workers=128) as pool:
        for ip, reply in zip(hosts, pool.map(_probe_scpi, hosts)):
            if reply:
                found[str(ip)] = reply

    matches = {ip: idn for ip, idn in found.items() if idn.startswith(IDN_PREFIX)}

    if verbose:
        if not found:
            print("  (nothing answered on port 5025)")
        for ip in sorted(found, key=ipaddress.ip_address):
            marker = "  <-- match" if ip in matches else "  (unrecognized *IDN? reply)"
            print(f"  {ip:<15s} {found[ip]!r}{marker}")

    result = next(iter(sorted(matches, key=ipaddress.ip_address))) if matches else None

    if verbose:
        print()
        print_arp_table(highlight_ip=result)

    return result

def burst_test(ip, rounds=20, burst_size=5, cmd="*IDN?",
                expected="Company,CableTester,SN001,FW1.0", round_pause=0.5):
    """Stress test for the rapid-fire reply-loss issue (see this module's
    docstring): fires `burst_size` copies of `cmd` back-to-back in one
    sendall() call — zero delay between them, unlike the spaced-out CASES
    test below — then waits for all `burst_size` replies before starting the
    next round. Reports exactly how many replies went missing, per round and
    in total, instead of just "looks fine" / "looks broken"."""
    print(f"Connecting to {ip}:{PORT} for burst test ({rounds} rounds x {burst_size} commands, cmd={cmd!r})...")
    s = socket.create_connection((ip, PORT), timeout=5)
    print("Connected.\n")

    total_sent = 0
    total_received = 0
    rounds_with_loss = 0

    try:
        for r in range(1, rounds + 1):
            payload = (cmd + "\n") * burst_size
            try:
                s.sendall(payload.encode())
            except OSError as e:
                print(f"[round {r}] send failed — connection dropped: {e}")
                break
            total_sent += burst_size

            replies = []
            buf = b""
            deadline = time.time() + 3.0
            while len(replies) < burst_size and time.time() < deadline:
                s.settimeout(max(0.05, deadline - time.time()))
                try:
                    chunk = s.recv(4096)
                except socket.timeout:
                    break
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    replies.append(line.decode(errors="replace").strip())

            total_received += len(replies)
            lost = burst_size - len(replies)
            if lost > 0:
                rounds_with_loss += 1
                print(f"[round {r:3d}] sent {burst_size}, got {len(replies)} replies  <-- LOST {lost}")
            else:
                bad = [rep for rep in replies if expected is not None and rep != expected]
                tag = "OK" if not bad else f"OK but {len(bad)} had unexpected content: {bad!r}"
                print(f"[round {r:3d}] sent {burst_size}, got {len(replies)} replies  [{tag}]")

            time.sleep(round_pause)
    finally:
        _clean_close(s)

    print(f"\nBurst test done: {total_received}/{total_sent} replies received "
          f"({rounds_with_loss}/{rounds} rounds had at least one lost reply).")
    return total_sent, total_received


CASES = [
    # Only commands actually registered in L3_App/scpi_commands.c's
    # s_scpi_commands table that are *idn?/*tst? (universal SCPI) or
    # TEST:LED:* — everything else (SYSTem:*, CONFigure:*, MEASure:*,
    # FETCh:*, TEST:STRing:*, TEST:ECHO?) is either not implemented by this
    # firmware at all or deliberately excluded here by request.
    ('*IDN?', 'Company,CableTester,SN001,FW1.0'),
    ('*TST?', '0'),  # 0 = full self-test pass
    ('TEST:LED:INTerval?', None),  # current blink interval in ms — varies
    ('TEST:LED:STATus?', None),  # "running,done,total" — varies
    ('TEST:LED:BLINk 20', 'OK'),
    ('TEST:LED:STATus?', None),
    ('TEST:LED:ABORt', 'OK'),
    ('TEST:LED:STATus?', None),
]

# Cycled through by semicolon_burst_test() below — same restriction (basic +
# TEST:LED:* only), queries only so every command always produces a reply to
# count.
BASIC_QUERY_CMDS = ['*IDN?', '*TST?', 'TEST:LED:STATus?', 'TEST:LED:INTerval?']


def semicolon_burst_test(ip, rounds=20, group_size=4, round_pause=0.5):
    """Joins `group_size` commands (cycled from BASIC_QUERY_CMDS) onto ONE
    line with ';' separators — standard SCPI syntax for chaining multiple
    commands on one line (confirmed working manually: '*idn?;*idn?' comes
    back as both replies joined by ';' on one line) — and checks that exactly
    `group_size` semicolon-separated reply fields come back every round.

    This is different from burst_test() above: that one fires `burst_size`
    separate \\n-terminated commands back-to-back with no delay. This one
    sends ONE line containing multiple ';'-separated commands — the scenario
    specifically asked for: does a single combined multi-command line ever
    lose one of its reply fields.

    IMPORTANT: uses a receive buffer that PERSISTS across rounds (see
    read_one_line() below), not a fresh one per round. An earlier version
    reset its buffer every round and gave up on timeout — if a reply simply
    arrived a little late, the abandoned bytes sat in the OS socket buffer
    and got picked up (and potentially merged with) a LATER round's read,
    which looked exactly like random reply loss/corruption but was actually
    just this script misattributing slow-but-intact replies to the wrong
    round. Carrying one buffer across the whole connection and always
    splitting off exactly one line at a time fixes that: a late reply still
    shows up in full, on whichever read call it actually arrives before,
    never merged with another line."""
    print(f"Connecting to {ip}:{PORT} for semicolon-burst test "
          f"({rounds} rounds x {group_size} commands/line)...")
    s = socket.create_connection((ip, PORT), timeout=5)
    print("Connected.\n")

    recv_buf = b""

    def read_one_line(deadline_s):
        nonlocal recv_buf
        end = time.time() + deadline_s
        while b"\n" not in recv_buf and time.time() < end:
            s.settimeout(max(0.05, end - time.time()))
            try:
                chunk = s.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                break
            recv_buf += chunk
        if b"\n" in recv_buf:
            line, recv_buf = recv_buf.split(b"\n", 1)
            return line.decode(errors="replace").strip()
        return None  # nothing complete yet — may still show up on a later call

    total_sent = 0
    total_received = 0
    rounds_with_loss = 0
    cmd_cycle = 0

    try:
        for r in range(1, rounds + 1):
            cmds = [BASIC_QUERY_CMDS[(cmd_cycle + i) % len(BASIC_QUERY_CMDS)] for i in range(group_size)]
            cmd_cycle += group_size
            # SCPI compound-command rule: a chained command that doesn't start
            # with ':' or '*' is treated as an ABBREVIATION of the previous
            # command's hierarchical prefix (see
            # ThirdParty/scpi-parser/libscpi/src/utils.c's
            # composeCompoundCommand()) — writing a second FULL path like
            # "TEST:LED:STATus?;TEST:LED:INTerval?" makes the parser prepend
            # the previous prefix AGAIN, producing the bogus
            # "TEST:LED:TEST:LED:INTerval?" and silently dropping that
            # command. A leading ':' on every full path after the first
            # avoids that (same fix as always abbreviating would give).
            line = cmds[0] + "".join(
                (";" + c) if c.startswith("*") else (";:" + c) for c in cmds[1:]
            ) + "\n"
            try:
                s.sendall(line.encode())
            except OSError as e:
                print(f"[round {r}] send failed — connection dropped: {e}")
                break
            total_sent += group_size

            reply_line = read_one_line(3.0)
            parts = reply_line.split(";") if reply_line else []

            total_received += len(parts)
            lost = group_size - len(parts)
            sent_desc = line.strip()
            if lost > 0:
                rounds_with_loss += 1
                print(f"[round {r:3d}] sent [{sent_desc}]")
                print(f"              -> got {len(parts)}/{group_size} fields  <-- LOST {lost} (or just late): {parts!r}")
            else:
                print(f"[round {r:3d}] sent [{sent_desc}] -> got {len(parts)}/{group_size} fields  [OK]")

            time.sleep(round_pause)

        # Give anything still in flight a last chance to land before judging
        # total loss, now that late replies are never silently merged/lost.
        deadline = time.time() + 2.0
        while time.time() < deadline:
            line = read_one_line(deadline - time.time())
            if line is None:
                break
            parts = line.split(";")
            total_received += len(parts)
            print(f"[late arrival] -> {parts!r}")
    finally:
        _clean_close(s)

    print(f"\nSemicolon-burst test done: {total_received}/{total_sent} reply fields received "
          f"({rounds_with_loss}/{rounds} rounds had at least one lost-or-late field).")
    return total_sent, total_received


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Manual SCPI test — spaced-out by default (safe); "
                     "--burst switches to a rapid-fire reply-loss stress test.")
    parser.add_argument("ip", nargs="?", default=None,
                         help="Device IP (default: auto-scan the local /24, or just pass "
                              "the fixed IP directly, e.g. 192.168.2.131)")
    parser.add_argument("--burst", action="store_true",
                         help="Rapid-fire test: `--size` separate \\n-terminated copies of "
                              "`--cmd` back-to-back with no delay between them.")
    parser.add_argument("--semicolon", action="store_true",
                         help="Semicolon-burst test: `--size` DIFFERENT basic/TEST:LED commands "
                              "joined with ';' on ONE line (standard SCPI multi-command syntax) — "
                              "checks whether any of the ';'-separated reply fields go missing.")
    parser.add_argument("--rounds", type=int, default=20, help="Burst modes: number of rounds (default 20)")
    parser.add_argument("--size", type=int, default=5, help="Burst modes: commands per round (default 5)")
    parser.add_argument("--cmd", default="*IDN?", help="--burst only: command to repeat (default *IDN?)")
    parser.add_argument("--pause", type=float, default=0.5,
                         help="Burst modes: seconds between rounds (default 0.5) — the gap a real "
                              "client leaves between commands; manual typing naturally gives >=1s.")
    args = parser.parse_args()

    if args.burst and args.semicolon:
        print("Pass only one of --burst / --semicolon.")
        sys.exit(2)

    if args.ip:
        ip = args.ip
    else:
        ip = find_device_ip()
        if ip is None:
            print("Could not find a CableTester on the local subnet.")
            print("Check the UART log for its IP and pass it explicitly:")
            print("  python manual_scpi_test.py <ip>")
            sys.exit(1)
        print(f"Found device at {ip}\n")

    if args.burst:
        expected = "Company,CableTester,SN001,FW1.0" if args.cmd.strip().upper() == "*IDN?" else None
        sent, received = burst_test(ip, rounds=args.rounds, burst_size=args.size,
                                     cmd=args.cmd, expected=expected, round_pause=args.pause)
        sys.exit(0 if sent == received else 1)

    if args.semicolon:
        sent, received = semicolon_burst_test(ip, rounds=args.rounds, group_size=args.size,
                                               round_pause=args.pause)
        sys.exit(0 if sent == received else 1)

    print(f"Connecting to {ip}:{PORT} ...")
    s = socket.create_connection((ip, PORT), timeout=5)
    print("Connected.\n")

    passed = 0
    total = 0

    for cmd, expected in CASES:
        print(f">> {cmd}")
        try:
            s.sendall((cmd + "\n").encode())
        except OSError as e:
            print(f"   (send failed — connection dropped: {e})")
            break
        time.sleep(DELAY)

        if "?" not in cmd:
            # not a query, no reply expected
            continue

        total += 1
        try:
            reply = _read_line(s, 2.0)
        except OSError as e:
            print(f"   (connection dropped: {e})")
            break
        if not reply:
            print("   (no reply — timed out)")
            continue

        if expected is not None:
            ok = reply == expected
            status = "OK" if ok else "MISMATCH"
            print(f"   reply: {reply!r}  expected: {expected!r}  [{status}]")
            if ok:
                passed += 1
        else:
            print(f"   reply: {reply!r}")
            passed += 1

        time.sleep(DELAY)

    print(f"\n{passed}/{total} queries got a reply (content checked where an exact value is expected).")
    _clean_close(s)


if __name__ == "__main__":
    main()
