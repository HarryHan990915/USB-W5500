# test_led_blink.py — demonstrates the start/status pattern a future
# long-running command (e.g. a loopback benchmark) should follow:
# TEST:LED:BLINk <n> starts the sequence in the background on the device
# (ScpiTask, not NetTask) and returns immediately; TEST:LED:STATus? can be
# polled at any time afterward for progress, without blocking on the whole
# sequence like the old TEST:LED:BLINk? query did.
#
# Run: python test_led_blink.py [ip] [count]
# (count defaults to 10; each blink is ~1s, so count=10 takes ~10s)

import sys
import socket
import time

from manual_scpi_test import find_device_ip, _read_line, _clean_close, PORT


def main():
    args = sys.argv[1:]
    ip = None
    count = 10
    if args and not args[0].isdigit():
        ip = args[0]
        args = args[1:]
    if args:
        count = int(args[0])

    if ip is None:
        ip = find_device_ip()
        if ip is None:
            print("Could not find a CableTester on the local subnet.")
            print("Check the UART log for its IP and pass it explicitly:")
            print("  python test_led_blink.py <ip> [count]")
            sys.exit(1)
        print(f"Found device at {ip}\n")

    print(f"Connecting to {ip}:{PORT} ...")
    s = socket.create_connection((ip, PORT), timeout=5)
    print("Connected.\n")

    cmd = f"TEST:LED:BLINk {count}"
    print(f">> {cmd}  (not a query — starts in the background, returns immediately)")
    s.sendall((cmd + "\n").encode())

    print(">> polling TEST:LED:STATus? ...")
    last_printed = None
    deadline = time.time() + count + 10.0
    done_seen = False
    while time.time() < deadline:
        s.sendall(b"TEST:LED:STATus?\n")
        reply = _read_line(s, 2.0)
        if not reply:
            print("   (no reply to STATus? — timed out)")
            break

        if reply != last_printed:
            print(f"   status: {reply!r}")
            last_printed = reply

        parts = reply.strip('"').split(",")
        if len(parts) == 3:
            running, done, total = parts
            if running == "0" and total not in ("", "0") and done == total:
                done_seen = True
                break

        time.sleep(0.5)

    if done_seen:
        print(f"   done — all {count} blinks completed. OK")
    else:
        print("   (gave up waiting for completion)")

    _clean_close(s)


if __name__ == "__main__":
    main()
