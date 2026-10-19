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

CASES = [
    ('*IDN?', 'Company,CableTester,SN001,FW1.0'),
    ('*TST?', '0'),  # 0 = full self-test pass
    ('TEST:ECHO? "hello"', '"hello"'),
    ('TEST:STRing:LENgth? "abc"', '3'),
    ('TEST:STRing:UPPer? "Ab1"', '"AB1"'),
    ('TEST:STRing:REVerse? "abc"', '"cba"'),
    ('TEST:STRing:CRC? "123456789"', '"CBF43926"'),
    ('TEST:STRing:STORe "abc"', None),  # no reply expected (not a query)
    ('TEST:STRing:STORe?', '"abc"'),
    ('TEST:COUNt?', None),  # just an increasing integer, no fixed expected value
    ('SYSTem:ERRor?', None),  # whatever's in the error queue
    ('CONFigure:PINS 8', None),  # no reply (not a query)
    ('CONFigure:THReshold:RESistance 50', None),  # ohms, no reply
    ('MEASure:CONTinuity?', None),  # 0 = all pins pass, 1 = at least one fault
    ('FETCh:RESult?', None),  # per-pin OPEN/OK/SHORT/MISWIRE list
    ('DIAGnostic:LATency?', None),  # previous command's recv-to-reply-sent, in microseconds
    ('SYSTem:CONFig:SAVE', None),  # persists current pin count/threshold/IP config to flash
]


def main():
    if len(sys.argv) > 1:
        ip = sys.argv[1]
    else:
        ip = find_device_ip()
        if ip is None:
            print("Could not find a CableTester on the local subnet.")
            print("Check the UART log for its IP and pass it explicitly:")
            print("  python manual_scpi_test.py <ip>")
            sys.exit(1)
        print(f"Found device at {ip}\n")

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
