#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 the freeciv-3ds authors
"""Minimal 3dslink sender: pushes a .3dsx to a 3DS over Wi-Fi and runs it.

On the 3DS, open the Homebrew Launcher and press Y ("NetLoader"), then:

    python3 tools/3dslink.py <3ds-ip> freeciv3ds.3dsx [args...]

The file is written to /3ds/<name> on the SD card and launched. Same wire
protocol as devkitPro's 3dslink (TCP 17491, zlib-deflated chunks).
"""
import socket
import struct
import sys
import zlib

PORT = 17491
CHUNK = 16 * 1024


def send_i32(s, v):
    s.sendall(struct.pack("<i", v))


def recv_i32(s):
    buf = b""
    while len(buf) < 4:
        part = s.recv(4 - len(buf))
        if not part:
            raise ConnectionError("3DS closed the connection")
        buf += part
    return struct.unpack("<i", buf)[0]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    host, path, args = sys.argv[1], sys.argv[2], sys.argv[3:]
    data = open(path, "rb").read()
    name = path.replace("\\", "/").rsplit("/", 1)[-1].encode()

    with socket.create_connection((host, PORT), timeout=15) as s:
        send_i32(s, len(name))
        s.sendall(name)
        send_i32(s, len(data))
        if recv_i32(s) != 0:
            print("3DS refused the file (not enough space?)")
            return 1
        z = zlib.compressobj()
        for i in range(0, len(data), CHUNK):
            out = z.compress(data[i:i + CHUNK])
            if out:
                send_i32(s, len(out))
                s.sendall(out)
        out = z.flush()
        if out:
            send_i32(s, len(out))
            s.sendall(out)
        if recv_i32(s) != 0:
            print("3DS reported an error writing the file")
            return 1
        argv = b"\0".join([b"sdmc:/3ds/" + name] + [a.encode() for a in args]) + b"\0"
        send_i32(s, len(argv))
        s.sendall(argv)
    print(f"sent {name.decode()} ({len(data)} bytes) to {host}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
