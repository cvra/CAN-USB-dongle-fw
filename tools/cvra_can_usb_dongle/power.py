#!/usr/bin/env python3
"""
Turns on or off the CAN bus power.
"""

import argparse
import serial



def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Serial port to which the dongle is connected.")
    parser.add_argument("power", choices=('on', 'off'), help="Bus state to apply")

    return parser.parse_args()

def main():
    args = parse_args()

    conn = serial.Serial(args.port, 115200)

    if args.power == "on":
        msg = "P\r"
    else:
        msg = "p\r"

    conn.write(msg.encode())

    data = bytes()

    while not data.endswith(b"\r"):
        data += conn.read(1)

if __name__ == '__main__':
    main()
