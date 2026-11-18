#!/usr/bin/env python3
"""Generate the small archive fixtures used by test/host/test_archive.c.

The files are committed so the host suite only needs gcc.  Re-run this
script after changing the fixture layout:

    python3 tools/gen_archive_fixtures.py
"""

from __future__ import annotations

import bz2
import gzip
import io
import pathlib
import tarfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "test" / "host" / "fixtures"
OUT.mkdir(parents=True, exist_ok=True)

HELLO = b"Hello, OpenLua!\n"
NESTED = b"nested member\n"
EMPTY = b""
LONG = b"long member\n"
LONG_NAME = ("very/long/path/that/needs/gnu/long/name/support/"
             "abcdefghijklmnopqrstuvwxyz0123456789.txt")
STORED = b"plain stored data\n"


def write_zip() -> None:
    with zipfile.ZipFile(OUT / "archive.zip", "w") as z:
        info = zipfile.ZipInfo("dir/")
        info.external_attr = (0o40755 << 16) | 0x10
        z.writestr(info, b"")
        z.writestr("hello.txt", HELLO, zipfile.ZIP_DEFLATED)
        z.writestr("dir/nested.txt", NESTED, zipfile.ZIP_DEFLATED)
        z.writestr("stored.txt", STORED, zipfile.ZIP_STORED)
        z.writestr("empty.txt", EMPTY, zipfile.ZIP_DEFLATED)
        z.writestr(LONG_NAME, LONG, zipfile.ZIP_DEFLATED)


def tar_bytes(fmt: int) -> bytes:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=fmt) as t:
        for name, data, is_dir in (
            ("hello.txt", HELLO, False),
            ("dir/nested.txt", NESTED, False),
            ("empty.txt", EMPTY, False),
            (LONG_NAME, LONG, False),
            ("dir", None, True),
        ):
            info = tarfile.TarInfo(name)
            if is_dir:
                info.type = tarfile.DIRTYPE
                info.size = 0
                t.addfile(info)
            else:
                info.size = len(data)
                info.mode = 0o644
                t.addfile(info, io.BytesIO(data))
    return buf.getvalue()


def write_tar() -> None:
    data = tar_bytes(tarfile.GNU_FORMAT)
    (OUT / "archive.tar").write_bytes(data)
    (OUT / "archive.tgz").write_bytes(gzip.compress(data, mtime=0))
    (OUT / "archive.tar.bz2").write_bytes(bz2.compress(data, compresslevel=9))


def write_gz() -> None:
    with open(OUT / "hello.txt.gz", "wb") as f:
        f.write(gzip.compress(b"Hello, gzip!\n", mtime=0))


def write_bz2() -> None:
    (OUT / "hello.txt.bz2").write_bytes(
        bz2.compress(b"Hello, bzip2!\n", compresslevel=9))


def main() -> None:
    write_zip()
    write_tar()
    write_gz()
    write_bz2()
    for path in sorted(OUT.iterdir()):
        print(f"{path.relative_to(ROOT)}: {path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
