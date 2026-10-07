#!/usr/bin/env python3
"""Build tiny synthetic MP3s with hand-written ID3 tags. No borrowed audio."""

import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))


def synchsafe(n):
    return bytes([(n >> 21) & 0x7F, (n >> 14) & 0x7F, (n >> 7) & 0x7F, n & 0x7F])


def plain32(n):
    return struct.pack(">I", n)


def text_frame(fid, text, version):
    body = b"\x00" + text.encode("latin1")
    size = synchsafe(len(body)) if version == 4 else plain32(len(body))
    return fid.encode("ascii") + size + b"\x00\x00" + body


def mpeg_frame():
    # MPEG1 Layer III, 32 kbps, 44100 Hz, no padding: 144*32000/44100 = 104 bytes.
    # Payload is zeros; these tests only check that the tag parser stops
    # and the following bytes are the frame header.
    return bytes([0xFF, 0xFB, 0x10, 0xC0]) + bytes(100)


def tag(version, body):
    return b"ID3" + bytes([version, 0, 0]) + synchsafe(len(body)) + body


def write(name, blob):
    path = os.path.join(HERE, name)
    with open(path, "wb") as f:
        f.write(blob)
    print("%s %d bytes" % (name, len(blob)))


def main():
    audio = mpeg_frame()

    small = text_frame("TIT2", "Hi", 4) + b"\x00" * 8
    write("small-v24.mp3", tag(4, small) + audio)

    # Body is 128 bytes, so the v2.4 size is 00 00 01 00.
    # Read as a plain uint32 that is 256, which swallows the next frame.
    title = text_frame("TIT2", "A" * 127, 4)
    performer = text_frame("TPE1", "X", 4)
    assert len(title) == 138
    # Old parser consumes 4+4+2+256 = 266 bytes of the first frame, so the
    # bogus next header must sit at offset 266. 138 + 12 + 116 = 266.
    gap = b"\x00" * 116
    bomb = b"APIC" + bytes([0x20, 0x00, 0x00, 0x00]) + b"\x00\x00"
    body = title + performer + gap + bomb
    assert body[266:270] == b"APIC"
    write("large-v24.mp3", tag(4, body) + audio)

    # Plain size 256 is synchsafe 128. v2.3 must keep the plain read,
    # otherwise the following TPE1 frame is skipped.
    v23 = text_frame("TIT2", "B" * 255, 3) + text_frame("TPE1", "Q", 3) + b"\x00" * 8
    assert v23[4:8] == b"\x00\x00\x01\x00"
    write("large-v23.mp3", tag(3, v23) + audio)

    # Synchsafe size 0x0FFFFFFF, but the tag ends immediately. The skip
    # loop has to stop at the tag, or it spins on that length.
    trunc = b"TIT2" + bytes([0x0F, 0x7F, 0x7F, 0x7F]) + b"\x00\x00" + b"\x00Z"
    write("truncated-v24.mp3", tag(4, trunc) + audio)


if __name__ == "__main__":
    main()
