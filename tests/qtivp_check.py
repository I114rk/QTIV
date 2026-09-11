#!/usr/bin/env python3
"""Независимая реализация формата .qtivp для интероп-теста.

Читает и пишет альбомы строго по спецификации docs/QTIVP-SPEC.md,
не используя код QTIV. Проверяет совместимость в обоих направлениях:
  1) qtiv --pack создаёт альбом → читаем и проверяем его здесь.
  2) Мы пишем альбом по спецификации → qtiv --info читает и проверяет.
Плюс проверяем режим терминального просмотра qtiv -c --ascii (чистый
текст без escape-последовательностей — работает в пайпе).
"""

import argparse
import json
import os
import struct
import subprocess
import sys
import zlib

MAGIC = b"QTIVP1\x00\x00"
VERSION = 1
FLAG_ZLIB = 0x1


def make_png(w, h):
    """Минимальный RGB PNG с градиентом (без внешних зависимостей)."""
    def px(x, y):
        return bytes((x * 255 // max(w - 1, 1), y * 255 // max(h - 1, 1), 128))

    raw = b"".join(b"\x00" + b"".join(px(x, y) for x in range(w)) for y in range(h))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def read_album(path):
    data = open(path, "rb").read()
    if data[:8] != MAGIC:
        raise AssertionError("bad magic: %r" % data[:8])
    version, flags, count = struct.unpack_from("<HHI", data, 8)
    assert version == VERSION, "unexpected version %d" % version
    assert flags == 0, "unexpected header flags %d" % flags

    off = 16
    entries = []
    for _ in range(count):
        (nlen,) = struct.unpack_from("<H", data, off)
        off += 2
        name = data[off:off + nlen].decode("utf-8")
        off += nlen
        (mlen,) = struct.unpack_from("<H", data, off)
        off += 2
        mime = data[off:off + mlen].decode("ascii")
        off += mlen
        width, height, raw_size, stored, eoff, eflags, crc = struct.unpack_from(
            "<IIIIQII", data, off)
        off += 32

        stored_bytes = data[eoff:eoff + stored]
        assert len(stored_bytes) == stored, "blob truncated"
        if eflags & FLAG_ZLIB:
            # zlib-поток с 4-байтовым big-endian префиксом длины (как qCompress)
            (raw_len,) = struct.unpack_from(">I", stored_bytes, 0)
            blob = zlib.decompress(stored_bytes[4:])
            assert len(blob) == raw_len, "zlib length prefix mismatch"
        else:
            blob = stored_bytes
        assert len(blob) == raw_size, "raw size mismatch"
        assert (zlib.crc32(blob) & 0xFFFFFFFF) == crc, "crc mismatch"
        entries.append(dict(name=name, mime=mime, width=width, height=height,
                            raw_size=raw_size, compressed=bool(eflags & FLAG_ZLIB),
                            blob=blob))
    return entries


def write_album(path, items):
    """items: (name, mime, width, height, blob, force_zlib)"""
    headers = []
    blobs = []
    for name, mime, w, h, blob, force_zlib in items:
        nb = name.encode("utf-8")
        mb = mime.encode("ascii")
        if force_zlib:
            stored = struct.pack(">I", len(blob)) + zlib.compress(blob, 9)
            flags = FLAG_ZLIB
        else:
            stored, flags = blob, 0
        headers.append((nb, mb, w, h, len(blob), len(stored), flags,
                        zlib.crc32(blob) & 0xFFFFFFFF))
        blobs.append(stored)

    # Смещения blob'ов начинаются после заголовка и всего индекса.
    index_size = sum(2 + len(nb) + 2 + len(mb) + 32 for nb, mb, *_ in headers)
    offset = 16 + index_size

    index = b""
    for nb, mb, w, h, raw_size, stored, flags, crc in headers:
        index += struct.pack("<H", len(nb)) + nb + struct.pack("<H", len(mb)) + mb
        index += struct.pack("<IIIIQII", w, h, raw_size, stored, offset, flags, crc)
        offset += stored

    with open(path, "wb") as f:
        f.write(MAGIC + struct.pack("<HHI", VERSION, 0, len(items)) + index + b"".join(blobs))


def run(cmd):
    r = subprocess.run(cmd, capture_output=True)
    if r.returncode != 0:
        raise AssertionError("command failed (%d): %s\nstderr: %s"
                             % (r.returncode, " ".join(cmd), r.stderr.decode()))
    return r.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True, help="path to the qtiv binary")
    ap.add_argument("--workdir", required=True)
    args = ap.parse_args()
    os.makedirs(args.workdir, exist_ok=True)

    png_a = make_png(8, 6)
    png_b = make_png(16, 4)
    file_a = os.path.join(args.workdir, "a.png")
    file_b = os.path.join(args.workdir, "b.png")
    with open(file_a, "wb") as f:
        f.write(png_a)
    with open(file_b, "wb") as f:
        f.write(png_b)

    # 1) qtiv --pack → читаем альбом независимо по спецификации.
    album_qtiv = os.path.join(args.workdir, "made-by-qtiv.qtivp")
    out = run([args.bin, "--pack", album_qtiv, file_a, file_b]).decode()
    assert "OK: packed 2" in out, out

    entries = read_album(album_qtiv)
    assert len(entries) == 2, len(entries)
    assert entries[0]["name"] == "a.png", entries[0]["name"]
    assert entries[0]["mime"] == "image/png", entries[0]["mime"]
    assert entries[0]["width"] == 8 and entries[0]["height"] == 6, entries[0]
    assert entries[0]["blob"] == png_a, "blob A differs from original file bytes"
    assert entries[1]["blob"] == png_b, "blob B differs from original file bytes"
    print("1) qtiv --pack album verified by independent reader: OK")

    # 2) Пишем альбом сами (одна запись с zlib, одна без) → qtiv --info.
    album_py = os.path.join(args.workdir, "made-by-python.qtivp")
    write_album(album_py, [
        ("alpha.png", "image/png", 8, 6, png_a, False),
        ("beta.png", "image/png", 16, 4, png_b, True),
    ])
    info = json.loads(run([args.bin, "--info", album_py]))
    assert info["type"] == "album", info
    assert info["count"] == 2, info
    assert info["entries"][0]["name"] == "alpha.png", info
    assert info["entries"][1]["name"] == "beta.png", info
    assert info["entries"][1]["width"] == 16, info
    assert info["entries"][1]["compressed"] is True, info
    print("2) python-written album verified by qtiv --info: OK")

    # 3) --info на обычном изображении.
    img_info = json.loads(run([args.bin, "--info", file_a]))
    assert img_info["type"] == "image", img_info
    assert img_info["width"] == 8 and img_info["height"] == 6, img_info
    print("3) qtiv --info on plain image: OK")

    # 4) Терминальный просмотр в пайпе: чистый ASCII без escape.
    art = run([args.bin, "-c", "--ascii", file_a]).decode("utf-8", "replace")
    assert len(art) > 40, "ascii output too short"
    assert all(ord(c) < 128 for c in art), "output contains non-ASCII bytes"
    assert "\x1b" not in art, "pipe mode must not emit escape sequences"
    assert "\n" in art, "ascii output must be multi-line"
    print("4) qtiv -c --ascii in pipe: OK")

    art_album = run([args.bin, "-c", "--ascii", album_py]).decode("utf-8", "replace")
    assert len(art_album) > 40 and "\x1b" not in art_album
    print("5) qtiv -c --ascii on album in pipe: OK")

    print("\nAll interop checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
