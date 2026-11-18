#!/usr/bin/env python3
"""
Generate a structured seed corpus for the jpegoptim "mpf_parser" fuzzers.

Seeds are raw APP2 "MPF\\0" marker payloads (what libjpeg saves from an
APP2 segment). Deterministic, dependency free, run from fuzz/build.sh.

SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import os
import struct


def u16(fmt, v):
    return struct.pack(fmt + "H", v)


def u32(fmt, v):
    return struct.pack(fmt + "I", v)


def mpf(big=True, n_images=2, uid=False, frames=False, endian_magic=None,
        first_ifd_offset=None, tag_order=True, entry_count=None,
        entry_offset=None, uid_count=None, next_ifd=None, attr=True,
        entries=None, truncate=0):
    fmt = ">" if big else "<"
    out = bytearray()
    out += b"MPF\x00"
    out += endian_magic if endian_magic else (b"MM\x00*" if big else b"II*\x00")

    n_tags = 3 + (1 if uid else 0) + (1 if frames else 0)
    index_ifd_abs = 16
    index_ifd_size = 2 + n_tags * 12 + 4
    entries_abs = index_ifd_abs + index_ifd_size
    entries_size = (entry_count if entry_count is not None else n_images) * 16
    uid_abs = entries_abs + entries_size
    uid_size = ((uid_count if uid_count is not None else n_images) * 33
                if uid else 0)
    attr_abs = uid_abs + uid_size

    out += u32(fmt, first_ifd_offset if first_ifd_offset is not None
               else index_ifd_abs - 4)
    out += b"\x00\x00\x00\x00"  # reserved

    tags = [u16(fmt, 0xB000) + u16(fmt, 7) + u32(fmt, 4) + b"0100",
            u16(fmt, 0xB001) + u16(fmt, 4) + u32(fmt, 1) + u32(fmt, n_images),
            u16(fmt, 0xB002) + u16(fmt, 7) +
            u32(fmt, entry_count if entry_count is not None
                else n_images * 16) +
            u32(fmt, entry_offset if entry_offset is not None
                else entries_abs - 4)]
    if uid:
        tags.append(u16(fmt, 0xB003) + u16(fmt, 7) +
                    u32(fmt, uid_count if uid_count is not None
                        else n_images * 33) +
                    u32(fmt, uid_abs - 4))
    if frames:
        tags.append(u16(fmt, 0xB004) + u16(fmt, 4) + u32(fmt, 1) + u32(fmt, 2))
    if not tag_order:
        tags = tags[::-1]

    out += u16(fmt, len(tags))
    out += b"".join(tags)
    out += u32(fmt, (attr_abs - 4 if attr and not next_ifd
                     else next_ifd if next_ifd is not None else 0))

    if entries is not None:
        for e in entries:
            out += e
    else:
        for i in range(n_images):
            if i == 0:
                out += (u32(fmt, 0x030000) + u32(fmt, 0x400) + u32(fmt, 0) +
                        u16(fmt, 0) + u16(fmt, 0))
            else:
                out += (u32(fmt, 0x010001) + u32(fmt, 100) +
                        u32(fmt, 4096 * i) + u16(fmt, 0) + u16(fmt, 0))

    if uid:
        for i in range(uid_count if uid_count is not None else n_images):
            out += bytes(33)

    if attr and not next_ifd:
        out += u16(fmt, 1)
        out += u16(fmt, 0xB101) + u16(fmt, 4) + u32(fmt, 1) + u32(fmt, 1)
        out += u32(fmt, 0)

    if truncate:
        out = out[:-truncate] if truncate < len(out) else b""
    return bytes(out)


def entry(big, attr, size, offset):
    fmt = ">" if big else "<"
    return (struct.pack(fmt + "I", attr) + struct.pack(fmt + "I", size) +
            struct.pack(fmt + "I", offset) + b"\x00\x00\x00\x00")


def gen_seeds():
    seeds = []
    add = seeds.append

    add(("valid-be", mpf(big=True)))
    add(("valid-le", mpf(big=False)))
    add(("valid-be-minimal", mpf(big=True, attr=False)))
    add(("valid-be-4img", mpf(big=True, n_images=4, attr=False,
                              entries=[entry(True, 0x030000, 0x400, 0)] +
                              [entry(True, 0x010001, 100, 4096 * i)
                               for i in range(1, 4)])))
    add(("valid-be-uid-frames", mpf(big=True, uid=True, frames=True)))
    add(("valid-uid-frames-le", mpf(big=False, uid=True, frames=True)))
    add(("entry-second-zero", mpf(big=True, attr=False,
                                  entries=[entry(True, 0x030000, 0x400, 0),
                                           entry(True, 0x010001, 100, 0)])))
    add(("entry-second-small", mpf(big=True, attr=False,
                                   entries=[entry(True, 0x030000, 0x400, 0),
                                            entry(True, 0x010001, 100, 8)])))
    add(("entry-second-huge", mpf(big=True, attr=False,
                                  entries=[entry(True, 0x030000, 0x400, 0),
                                           entry(True, 0x010001, 100,
                                                 0xFFFF0000)])))
    add(("entry-size-mismatch", mpf(big=True, attr=False,
                                    entries=[entry(True, 0x030000, 9, 0),
                                             entry(True, 0x010001, 9,
                                                   4096)])))
    add(("bad-endian", mpf(big=True, endian_magic=b"XX\x01\x00")))
    add(("bad-fio-small", mpf(big=True, first_ifd_offset=4)))
    add(("bad-fio-huge", mpf(big=True, first_ifd_offset=0xFFFFFF00)))
    add(("bad-tag-order", mpf(big=True, tag_order=False)))
    add(("bad-entry-count", mpf(big=True, entry_count=99, attr=False)))
    add(("bad-entry-offset", mpf(big=True, entry_offset=10000, attr=False)))
    add(("bad-uid-count", mpf(big=True, uid=True, uid_count=1)))
    add(("bad-next-ifd", mpf(big=True, next_ifd=0xFFFFFF00)))
    add(("bad-numimages-1", mpf(big=True, n_images=1)))
    add(("truncated-8", mpf(big=True, truncate=8)))
    add(("truncated-half", mpf(big=True, truncate=len(mpf(big=True)) // 2)))
    add(("minimal-header", b"MPF\x00II*\x00\x08\x00\x00\x00"))

    return seeds


def main():
    parser = argparse.ArgumentParser(
        description="Generate jpegoptim mpf_parser seed corpus")
    parser.add_argument("--out", required=True, help="output directory")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    for name, blob in gen_seeds():
        path = os.path.join(args.out, name + ".mpf")
        with open(path, "wb") as f:
            f.write(blob)
        print("wrote %s (%d bytes)" % (path, len(blob)))


if __name__ == "__main__":
    main()

# eof :-)
