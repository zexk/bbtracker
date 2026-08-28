#!/usr/bin/env python3
"""List files in MGS4 Master Collection VPAK archives without extracting them."""

import argparse
import io
import struct
import sys

HEADER = struct.Struct("<4sHHII")
ENTRY = struct.Struct("<QQQQIIQ")


def read_exact(stream, size):
    data = stream.read(size)
    if len(data) != size:
        raise ValueError("truncated VPAK index")
    return data


def entries(stream):
    magic, version, flags, count, index_size = HEADER.unpack(read_exact(stream, HEADER.size))
    if magic != b"VPAK" or version != 3:
        raise ValueError(f"unsupported VPAK magic/version: {magic!r}/{version}")

    stream.seek(0, io.SEEK_END)
    file_size = stream.tell()
    index_offset = file_size - index_size
    if index_offset < HEADER.size:
        raise ValueError("invalid VPAK index size")
    stream.seek(index_offset)

    for _ in range(count):
        path_size, path_flags = struct.unpack("<IH", read_exact(stream, 6))
        if path_size > 4096 or stream.tell() + path_size + ENTRY.size > file_size:
            raise ValueError(f"invalid VPAK path size: {path_size}")
        path = read_exact(stream, path_size).decode("utf-8")
        unknown0, size, stored_size, offset, chunk_size, chunk_count, unknown1 = ENTRY.unpack(
            read_exact(stream, ENTRY.size)
        )
        if chunk_count < 1:
            raise ValueError(f"invalid chunk count for {path!r}: {chunk_count}")
        boundaries = struct.unpack(f"<{chunk_count - 1}Q", read_exact(stream, 8 * (chunk_count - 1)))
        payload_size = stored_size or size
        if offset + payload_size > index_offset:
            raise ValueError(f"payload outside VPAK data area: {path!r}")
        if payload_size and any(
            left >= right for left, right in zip((0,) + boundaries, boundaries + (payload_size,))
        ):
            raise ValueError(f"invalid chunk boundaries for {path!r}")
        yield path, path_flags, size, stored_size, offset, chunk_size, chunk_count, unknown0, unknown1, boundaries

    if stream.tell() != file_size:
        raise ValueError(f"index has {file_size - stream.tell()} trailing bytes")

def self_test():
    path = b"test/file.bin"
    metadata = ENTRY.pack(0, 30, 20, 16, 16, 2, 0) + struct.pack("<Q", 10)
    index = struct.pack("<IH", len(path), 0x100) + path + metadata
    archive = HEADER.pack(b"VPAK", 3, 1, 1, len(index)) + bytes(20) + index
    parsed = list(entries(io.BytesIO(archive)))
    assert parsed[0][:7] == ("test/file.bin", 0x100, 30, 20, 16, 16, 2)
    assert parsed[0][-1] == (10,)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        self_test()
        return
    if not args.archive:
        parser.error("archive is required")

    with open(args.archive, "rb") as stream:
        for entry in entries(stream):
            path, path_flags, size, stored_size, offset, chunk_size, chunk_count, unknown0, unknown1, _ = entry
            print(f"{path}\t{size}\t{stored_size}\t{offset}\t{chunk_size}\t{chunk_count}\t{path_flags:#x}\t{unknown0:#x}\t{unknown1:#x}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, UnicodeDecodeError, ValueError) as error:
        sys.exit(str(error))
