#!/usr/bin/env python3
"""Report which games RetroAchievements recognizes, and which have a set.

    tools/ra-check.py [--roms DIR] [--system NAME] [--verbose]

RA identifies a game by an MD5 over the ROM, but not always over the WHOLE
ROM - each console has its own rule about what to skip. This implements the
rules for the nine systems ZyperOS ships and asks RA what it makes of each.

Needs no credentials. `r=gameid` is unauthenticated; it does need a
User-Agent, because RA refuses curl's default with `unsupported_client`,
which reads like a permissions problem and is not one.

SEQUENTIALLY, on purpose. A first version ran six requests at a time, got
rate-limited, counted the failures as "no achievements" and reported 48%
coverage. The real figure was 93%. Anything that turns a network failure into
a negative result will eventually tell you a confident lie; failures are
retried here and counted separately.

Measured 2026-08-29 over 180 ROMs: 168 recognized. Of the 12 that are not,
8 are fan translations - romhacks are in no No-Intro-derived database under
those names, so RA has never seen their hashes and never will.
"""
import argparse
import hashlib
import struct
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UA = "ZyperOS/1.0 (+achievement id check)"

# systems.cfg tag -> how much of the ROM RA hashes.
#   NES   an iNES header is 16 bytes of container, not game
#   SFC   copier headers are 512 bytes and only present sometimes, which is
#         what the size test detects
#   PCE   the same idea against a different block size
#   MD/SMS/GG  an SMD-style header, likewise conditional
#   GB/GBC/GBA  the whole file; there is no container to strip
#   NES (FDS)  fwNES's 16-byte "FDS\x1a" header, rc_hash_nes's second case
#   A7800  a 128-byte A78 header, "ATARI7800" at byte 1 (rc_hash_7800)
#   LYNX   a 64-byte "LYNX\0" header (rc_hash_lynx)
#   N64    the big-endian (.z64) order whatever the file's: 0x37 first is
#          byte-swapped (.v64), 0x40 little-endian words (.n64); any other
#          first byte is not an N64 ROM, and None (rc_hash_n64)
#   NDS    not the whole file: the header's first 0x160 bytes, the ARM9 and
#          ARM7 code and 0xA00 bytes of icon and titles, where the header
#          puts each, past a SuperCard's own 512-byte header; zeros past the
#          file's end; None when the codes pass 16 MB (rc_hash_nintendo_ds)
def nds_body(d):
    if len(d) < 512:
        return None
    off = 0
    if d[0:4] == b"\x2e\x00\x00\xea" and d[0xB0:0xB4] == b"\x44\x46\x96\x00":
        off = 512
    h = d[off:off + 512]
    h += bytes(512 - len(h))
    u32 = lambda o: int.from_bytes(h[o:o + 4], "little")
    arm9_addr, arm9_size = u32(0x20), u32(0x2C)
    arm7_addr, arm7_size = u32(0x30), u32(0x3C)
    icon_addr = u32(0x68)
    if arm9_size + arm7_size > 16 * 1024 * 1024:
        return None

    def part(at, size):
        b = d[off + at:off + at + size]
        return b + bytes(size - len(b))
    return h[:0x160] + part(arm9_addr, arm9_size) + part(arm7_addr, arm7_size) + part(icon_addr, 0xA00)


def ra_body(tag, d):
    if tag == "NES":
        return d[16:] if d[:4] in (b"NES\x1a", b"FDS\x1a") else d
    if tag == "A7800":
        return d[128:] if d[1:10] == b"ATARI7800" else d
    if tag == "LYNX":
        return d[64:] if d[:5] == b"LYNX\0" else d
    if tag == "NDS":
        return nds_body(d)
    if tag == "N64":
        if not d:
            return None
        if d[0] == 0x80:
            return d
        if d[0] == 0x37:
            b = bytearray(d)
            b[0:len(b) & ~1:2], b[1:len(b) & ~1:2] = d[1:len(d) & ~1:2], d[0:len(d) & ~1:2]
            return bytes(b)
        if d[0] == 0x40:
            b = bytearray(d)
            m = len(d) & ~3
            for k in range(4):
                b[k:m:4] = d[3 - k:m:4]
            return bytes(b)
        return None
    if tag == "SFC":
        return d[512:] if len(d) % 1024 == 512 else d
    if tag == "PCE":
        return d[512:] if len(d) % 131072 == 512 else d
    if tag in ("MD", "SMS", "GG"):
        return d[512:] if len(d) % 16384 == 512 else d
    return d


# A PlayStation disc is not hashed whole: RA hashes the boot executable that
# SYSTEM.CNF names (PSX.EXE without one), prefixed with that name. rcheevos's
# rc_hash_psx and its ISO 9660 walk, restated - and restated again in C in
# src/rahash.c, which check-rahash holds to this one. It reads sectors, not the
# whole image. .cue, a bare .bin/.iso/.img, the first disc of an .m3u and a
# PSone Classics .pbp (its first disc: the disc's hash, as DuckStation makes
# it). A PSP disc is hashed by PARAM.SFO then EBOOT.BIN (rc_hash_psp), from an
# .iso or a .cso; a PSP .pbp is hashed whole.
#
# .chd needs a CHD reader, which the C side has (src/chd.c) and this one does
# not: a .chd gives None here, and check-rahash knows it.
DISC_TAGS = ("PS1", "PSP")
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"


def _sibling(path, name):
    return name if os.path.isabs(name) else os.path.join(os.path.dirname(path), name)


class _File:
    """An image read as it is"""
    def __init__(self, path):
        self.f = open(path, "rb")

    def read_at(self, off, n):
        self.f.seek(off)
        return self.f.read(n)

    def close(self):
        self.f.close()


class _Blocks:
    """A container of zlib blocks, read as the image they hold: a .cso
    (src/discimg.c, PPSSPP's CISO reader) or a PSone .pbp (PCSX ReARMed's)"""
    def __init__(self, path):
        self.f = open(path, "rb")
        self.f.seek(0, 2)
        self.fsize = self.f.tell()
        self.at, self.buf = -1, b""

    def raw(self, off, n):
        self.f.seek(off)
        return self.f.read(n)

    def read_at(self, off, n):
        out = bytearray()
        while n > 0 and off < self.size:
            b, inside = divmod(off, self.block)
            if b != self.at:
                d = self.load(b)
                if d is None:
                    break
                self.at, self.buf = b, d
            take = min(n, self.block - inside, self.size - off)
            out += self.buf[inside:inside + take]
            off, n = off + take, n - take
        return bytes(out)

    def close(self):
        self.f.close()


class _Cso(_Blocks):
    def __init__(self, path):
        super().__init__(path)
        h = self.raw(0, 24)
        if len(h) < 24 or h[:4] != b"CISO" or h[20] > 1:
            raise ValueError("not a CISO this reads")
        self.size = int.from_bytes(h[8:16], "little")
        self.block, self.align = int.from_bytes(h[16:20], "little"), h[21]
        if (self.block < 2048 or self.block > 16 << 20 or self.block & (self.block - 1)
                or self.align > 20):
            raise ValueError("bad CISO header")
        self.nblocks = -(-self.size // self.block)

    def load(self, b):
        import zlib
        if b >= self.nblocks:
            return None
        e = self.raw(24 + 4 * b, 8)
        if len(e) < 8:
            return None
        i0, i1 = int.from_bytes(e[:4], "little"), int.from_bytes(e[4:], "little")
        pos, end = (i0 & 0x7fffffff) << self.align, (i1 & 0x7fffffff) << self.align
        if i0 & 0x80000000:
            return self.raw(pos, self.block).ljust(self.block, b"\0")
        z = zlib.decompressobj(-15)
        try:
            d = z.decompress(self.raw(pos, min(end - pos, self.block + (1 << self.align))))
        except zlib.error:
            return None
        return d if len(d) == self.block and z.eof else None


class _PbpPsx(_Blocks):
    BLOCK = 16 * 2352

    def __init__(self, path):
        super().__init__(path)
        h = self.raw(0, 40)
        if len(h) < 40 or h[:4] != b"\0PBP":
            raise ValueError("not a PBP")
        psar = int.from_bytes(h[36:40], "little")
        img, sig = psar, self.raw(psar, 16)
        if sig[:10] == b"PSTITLEIMG":
            first = int.from_bytes(self.raw(psar + 0x200, 4), "little")
            if not first:
                raise ValueError("no disc")
            img = psar + first
            sig = self.raw(img, 16)
        if sig[:10] != b"PSISOIMG00":
            raise ValueError("no PSone disc")
        self.index, self.base = img + 0x4000, img + 0x100000
        self.block = self.BLOCK
        self.nblocks = (0x100000 - 0x4000) // 32
        self.size = self.nblocks * self.BLOCK

    def load(self, b):
        import zlib
        e = self.raw(self.index + 32 * b, 8)
        if len(e) < 8:
            return None
        pos, ln = self.base + int.from_bytes(e[:4], "little"), int.from_bytes(e[4:6], "little")
        if not ln or ln > self.BLOCK:
            return None
        data = self.raw(pos, ln)
        z = zlib.decompressobj(-15)
        try:
            d = z.decompress(data)
            if z.eof:
                return d.ljust(self.BLOCK, b"\0")
        except zlib.error:
            pass
        return data if ln == self.BLOCK else None


def _psx_track1(path):
    """(reader, byte offset of track 1) for a disc, or None."""
    ext = os.path.splitext(path)[1].lower()
    if ext == ".m3u":
        for line in open(path, encoding="utf-8", errors="replace"):
            line = line.strip()
            if line and not line.startswith("#"):
                first = _sibling(path, line.split("|")[0])
                return None if first.lower().endswith(".m3u") else _psx_track1(first)
        return None
    if ext == ".cue":
        cur, ss, in1 = None, 2352, False
        for line in open(path, encoding="utf-8", errors="replace"):
            t = line.strip()
            u = t.upper()
            if u.startswith("FILE"):
                q = t.split('"')
                if len(q) >= 3:
                    cur = q[1]
            elif u.startswith("TRACK"):
                f = t.split()
                in1 = len(f) > 1 and f[1].isdigit() and int(f[1]) == 1
                if in1:
                    ss = 2048 if "/2048" in t else 2336 if "/2336" in t else 2352
            elif in1 and u.startswith("INDEX 01") and cur:
                mm, sec, fr = (int(x) for x in t[8:].strip().split(":")[:3])
                f = _sibling(path, cur)
                return (_File(f), ((mm * 60 + sec) * 75 + fr) * ss) if os.path.isfile(f) else None
        return None
    try:
        if ext in (".bin", ".iso", ".img"):
            return _File(path), 0
        if ext == ".pbp":
            return _PbpPsx(path), 0
    except (OSError, ValueError):
        return None
    return None


class _Track:
    count = 0                   # sectors, where a track ends before its file

    def __init__(self, f, offset):
        self.f, self.offset = f, offset
        self.ss = None
        for ss in (2352, 2336):
            h = f.read_at(16 * ss + offset, 32)
            if len(h) == 32 and h[:12] == SYNC:
                bcd = lambda b: (b >> 4) * 10 + (b & 15)
                self.ss, self.hdr = ss, (24 if h[25:30] == b"CD001" else 16)
                self.first = (bcd(h[12]) * 60 + bcd(h[13])) * 75 + bcd(h[14]) - 150 - 16
                return
        h = f.read_at(16 * 2048 + offset, 32)
        if h[1:6] == b"CD001":
            self.ss, self.hdr, self.first = 2048, 0, 0

    @classmethod
    def at(cls, f, ss, hdr, first, offset=0, count=0):
        """A track whose layout is known already"""
        t = cls.__new__(cls)
        t.f, t.ss, t.hdr, t.first, t.offset, t.count = f, ss, hdr, first, offset, count
        return t

    def read(self, sector, n=2048):
        if sector < self.first or (self.count and sector - self.first >= self.count):
            return b""
        return self.f.read_at((sector - self.first) * self.ss + self.hdr + self.offset, min(n, 2048))

    def find(self, path):
        """(sector, size) of a file by backslashed path, or (0, 0). The root's
        length keeps counting down inside a subdirectory, as in rcheevos."""
        pvd = self.read(self.first + 16, 256)
        if pvd[:6] != b"\x01CD001":
            return 0, 0
        lbs = int.from_bytes(pvd[128:130], "little")
        sector = int.from_bytes(pvd[158:161], "little")
        remaining = int.from_bytes(pvd[166:170], "little")
        parts = path.lstrip("\\").split("\\")
        for i, part in enumerate(parts):
            last, want = i == len(parts) - 1, part.encode().lower()
            hit = None
            while hit is None:
                sec = self.read(sector)
                if len(sec) < 2048:
                    return 0, 0
                r = 0
                while r < 2048 - 34 and sec[r]:
                    n = sec[32 + r]
                    # The same bounds the C side holds: a name longer than its
                    # record, or than the sector, ends the scan. Python would
                    # not over-read, but the two must stop at the same record.
                    if r + sec[r] > 2048 or 33 + n > sec[r] or r + 33 + n > 2048:
                        break
                    name = sec[r + 33:r + 33 + n]
                    if n >= len(want) and name[:len(want)].lower() == want and (
                            n == len(want) or (last and name[len(want):len(want) + 1] == b";")):
                        hit = (int.from_bytes(sec[r + 2:r + 5], "little"),
                               int.from_bytes(sec[r + 10:r + 14], "little"))
                        break
                    r += sec[r]
                if hit is None:
                    if remaining <= lbs:
                        return 0, 0
                    remaining -= lbs
                    sector += 1
            if last:
                return hit
            sector = hit[0]
        return 0, 0

    def file(self, sector, size):
        """A file's bytes as rc_hash_cd_file reads them: its first sector
        whole or nothing, then up to its size, capped at 64 MB"""
        first = self.read(sector)
        if len(first) < 2048:
            return None
        size = min(size, 64 * 1024 * 1024)
        out = [first[:size]]
        size -= min(size, 2048)
        while size > 0:
            sector += 1
            d = self.read(sector, min(size, 2048))
            if not d:
                break
            out.append(d)
            size -= len(d)
        return b"".join(out)


def psx_hash_input(path):
    """The bytes RA's PlayStation hash is the MD5 of, or None."""
    t1 = _psx_track1(path)
    if not t1:
        return None
    f = t1[0]
    try:
        t = _Track(f, t1[1])
        if not t.ss:
            return None
        name, (sector, size) = b"", t.find("SYSTEM.CNF")
        if sector:
            cnf = t.read(sector, 2047).split(b"\0")[0]
            sector = 0
            for line in cnf.split(b"\n"):
                if not line.startswith(b"BOOT"):
                    continue
                v = line[4:].lstrip()
                if not v.startswith(b"="):
                    continue
                v = v[1:].lstrip()
                if v.startswith(b"cdrom:"):
                    v = v[6:]
                v = v.lstrip(b"\\")
                end = 0
                while end < len(v) and not chr(v[end]).isspace() and v[end:end + 1] != b";":
                    end += 1
                name = v[:end][:63]
                sector, size = t.find(name.decode("latin-1"))
                break
        if not sector:
            sector, size = t.find("PSX.EXE")
            name = b"PSX.EXE" if sector else b""
        head = t.read(sector, 32) if sector else b""
        if len(head) < 32:
            return None
        if head[:7] == b"PS-X EXE":
            size = int.from_bytes(head[28:32], "little") + 2048
        size = min(size, 64 * 1024 * 1024)
        out = [name]
        while size > 0:
            d = t.read(sector, min(size, 2048))
            if not d:
                break
            out.append(d)
            size -= len(d)
            sector += 1
        return b"".join(out)
    finally:
        f.close()


def psp_md5(path):
    """RA's PSP hash (rc_hash_psp), or None: a .pbp whole, else PARAM.SFO
    then EBOOT.BIN off the disc an .iso or a .cso holds"""
    ext = os.path.splitext(path)[1].lower()
    if ext == ".pbp":
        m = hashlib.md5()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                m.update(chunk)
        return m.hexdigest()
    try:
        f = _File(path) if ext == ".iso" else _Cso(path) if ext == ".cso" else None
    except (OSError, ValueError):
        return None
    if f is None:
        return None
    try:
        t = _Track(f, 0)
        if not t.ss:
            return None
        m = hashlib.md5()
        for name in ("PSP_GAME\\PARAM.SFO", "PSP_GAME\\SYSDIR\\EBOOT.BIN"):
            sector, size = t.find(name)
            body = t.file(sector, size) if sector else None
            if body is None:
                return None
            m.update(body)
        return m.hexdigest()
    finally:
        f.close()


# ----------------------------------------------------------- Dreamcast ----
# rcheevos's rc_hash_dreamcast, as src/rahash.c's ra_hash_dc: IP.BIN's 256
# bytes off track 3 (else the first data track), then the boot file it names,
# from that track or from the last one. Where each container puts a track:
# .gdi and .cue as rcheevos's own reader, .cdi as Flycast's. No .chd here
# (Python has no CHD reader, as for the PlayStation's): None.

DC_FIRST_DATA, DC_LAST = -1, -2
_C_SPACE = b" \t\n\v\f\r"          # C's isspace


def _atoi(text):
    """C's atoi: the leading number, 0 when there is none"""
    m = re.match(r"\s*([+-]?\d+)", text)
    return int(m.group(1)) if m else 0


def _msf(h):
    bcd = lambda b: (b >> 4) * 10 + (b & 15)
    return (bcd(h[12]) * 60 + bcd(h[13])) * 75 + bcd(h[14]) - 150


def _dc_gdi(path, want):
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    if not lines:
        return None
    if want == DC_LAST:
        want = _atoi(lines[0])
    for line in lines[1:]:
        f = line.split(None, 4)
        if len(f) < 5 or not all(x.lstrip("-").isdigit() for x in f[:4]):
            continue
        num, lba, kind, ss = (int(x) for x in f[:4])
        rest = f[4].strip()
        if rest.startswith('"'):
            end = rest.find('"', 1)
            if end < 0:
                continue
            name = rest[1:end]
        else:
            name = rest.split()[0]
        if (kind != 4) if want == DC_FIRST_DATA else (num != want):
            continue
        p = _sibling(path, name)
        if not os.path.isfile(p):
            return None
        t = _Track(_File(p), 0)
        if not t.ss:
            if ss not in (2352, 2048):
                t.f.close()
                return None
            t = _Track.at(t.f, ss, 16 if ss == 2352 else 0, 0)
        t.first = lba
        return t
    return None


def _dc_cue(path, want):
    """rcheevos's cdreader_open_cue_track, as src/rahash.c's dc_cue_track"""
    def blank():
        return {"id": 0, "first": 0, "pregap": 0, "count": 0, "ss": 0, "off": 0,
                "ffirst": 0, "data": False, "mode": "", "file": ""}
    cur, prev, done = blank(), blank(), False
    for raw in open(path, encoding="utf-8", errors="replace"):
        p = raw.lstrip(" \t").rstrip("\r\n")
        u = p.upper()
        if u.startswith("INDEX "):
            f = p[6:].split()
            idx = int(f[0]) if f and f[0].isdigit() else 0
            mm = [int(x) if x.isdigit() else 0 for x in (f[1].split(":") if len(f) > 1 else [])]
            mm = (mm + [0, 0, 0])[:3]
            at = (mm[0] * 60 + mm[1]) * 75 + mm[2]
            if cur["first"] == -1:
                cur["first"] = at
                if cur["file"] == prev["file"]:
                    prev["count"] = cur["first"] - prev["first"]
                    cur["off"] += prev["count"] * prev["ss"]
            if idx == 1:
                cur["pregap"] = at - cur["first"]
                if cur["id"] == want or (want == DC_FIRST_DATA and cur["data"]):
                    want, done = cur["id"], True
                    break
        elif u.startswith("TRACK "):
            if cur["ss"]:
                prev = dict(cur)
            f = p[6:].split()
            cur["id"] = int(f[0]) if f and f[0].isdigit() else 0
            cur["pregap"], cur["first"] = -1, -1
            cur["mode"] = f[1][:15] if len(f) > 1 else ""
            cur["data"] = cur["mode"].startswith("MODE")
            cur["ss"] = _atoi(cur["mode"][6:]) if cur["data"] else 2352
        elif u.startswith("FILE "):
            if cur["ss"]:
                prev = dict(cur)
                if prev["count"] == 0 and prev["ss"] > 0:
                    pb = _sibling(path, prev["file"])
                    size = os.path.getsize(pb) if os.path.isfile(pb) else 0
                    prev["count"] = size // prev["ss"] - prev["first"]
            cur = blank()
            cur["ffirst"] = prev["ffirst"] + prev["first"] + prev["count"]
            q = p[5:].lstrip(" ")
            if q.startswith('"'):
                end = q.find('"', 1)
                cur["file"] = q[1:end] if end > 0 else q[1:]
            else:
                cur["file"] = q.split()[0] if q.split() else ""
    if want == DC_LAST and not done:
        want = cur["id"]
    if not cur["id"] or cur["id"] != want or not cur["file"]:
        return None
    p = _sibling(path, cur["file"])
    if not os.path.isfile(p):
        return None
    f = _File(p)
    pregap = max(cur["pregap"], 0)
    first, off = cur["ffirst"] + cur["first"], cur["off"]
    ss = hdr = 0
    toc = 16 + pregap
    h = f.read_at(off + toc * 2352, 32)
    if len(h) == 32:
        if h[:12] == SYNC:
            ss, hdr, first = 2352, (24 if h[25:30] == b"CD001" else 16), _msf(h) - 16 - pregap
        else:
            h = f.read_at(off + toc * 2336, 32)
            if len(h) == 32 and h[:12] == SYNC:
                ss, hdr, first = 2336, (24 if h[25:30] == b"CD001" else 16), _msf(h) - 16 - pregap
            else:
                h = f.read_at(off + toc * 2048, 32)
                if len(h) == 32 and h[1:6] == b"CD001":
                    ss, hdr = 2048, 0
    if not ss:
        m = cur["mode"]
        for key, (s_, h_) in (("MODE2/2352", (2352, 24)), ("MODE1/2048", (2048, 0)),
                              ("MODE2/2336", (2336, 8)), ("MODE1/2352", (2352, 16)),
                              ("AUDIO", (2352, 0))):
            if m.startswith(key):
                ss, hdr = s_, h_
                break
    if not ss:
        f.close()
        return None
    return _Track.at(f, ss, hdr, first + pregap, off + pregap * ss)


CDI_VERSIONS = (0x80000004, 0x80000005, 0x80000006)
CDI_MARK = bytes([0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF])


def _dc_cdi(path, want):
    """A DiscJuggler .cdi as Flycast reads it, as src/rahash.c's dc_cdi_track"""
    size = os.path.getsize(path)
    if size < 8:
        return None
    with open(path, "rb") as fh:
        fh.seek(size - 8)
        version, hoff = struct.unpack("<II", fh.read(8))
        if version not in CDI_VERSIONS or not hoff:
            return None
        start = size - hoff if version == 0x80000006 else hoff
        if hoff > size or start >= size or size - start > 1 << 20:
            return None
        fh.seek(start)
        d = fh.read(size - start)
    at = 0

    def get(k):
        nonlocal at
        if at + k > len(d):
            raise ValueError("short")
        v = int.from_bytes(d[at:at + k], "little")
        at += k
        return v

    def skip(k):
        nonlocal at
        if at + k > len(d):
            raise ValueError("short")
        at += k

    pick, n, pos = None, 0, 0
    try:
        for _ in range(get(2)):
            for _ in range(get(2)):
                if get(4):
                    skip(8)
                if d[at:at + 10] != CDI_MARK or d[at + 10:at + 20] != CDI_MARK:
                    return None
                skip(24)
                skip(get(1))
                skip(19)
                if get(4) == 0x80000000:
                    skip(8)
                skip(2)
                pregap, length = get(4), get(4)
                skip(6)
                mode = get(4)
                skip(12)
                lba, total = get(4), get(4)
                skip(16)
                ss = {0: 2048, 1: 2336, 2: 2352, 4: 2448}.get(get(4), 0)
                skip(29)
                if version != 0x80000004:
                    skip(5)
                    if get(4) == 0xFFFFFFFF:
                        skip(78)
                if not ss:
                    return None
                n += 1
                tr = (mode, ss, pregap, length, lba, pos)
                if pick is None and ((want > 0 and want == n) or (want == DC_FIRST_DATA and mode)):
                    pick = tr
                if want == DC_LAST:
                    pick = tr
                pos += total if total < length + pregap else total * ss
            skip(4 + 8 + (1 if version != 0x80000004 else 0))
    except ValueError:
        return None
    if pick is None:
        return None
    mode, ss, pregap, length, lba, tpos = pick
    f = _File(path)
    off = tpos + pregap * ss
    if ss in (2352, 2448):
        h = f.read_at(off + 16 * ss, 16)
        hdr = 16 if len(h) == 16 and h[15] == 1 else 24
    else:
        hdr = 8 if ss == 2336 else 0
    return _Track.at(f, ss, hdr, lba + pregap - 150, off, length)


def _dc_open(path, want):
    ext = os.path.splitext(path)[1].lower()
    try:
        if ext == ".gdi":
            return _dc_gdi(path, want)
        if ext == ".cue":
            return _dc_cue(path, want)
        if ext == ".cdi":
            return _dc_cdi(path, want)
    except (OSError, ValueError, UnicodeDecodeError):
        return None
    return None


def dc_md5(path):
    """RA's Dreamcast hash (rc_hash_dreamcast), or None"""
    katana = b"SEGA SEGAKATANA "
    t = _dc_open(path, 3)
    ip = t.read(t.first, 256) if t else b""
    if ip[:16] != katana:
        if t:
            t.f.close()
        t = _dc_open(path, DC_FIRST_DATA)
        if not t:
            return None
        ip = t.read(t.first, 256)
        if ip[:16] != katana:
            t.f.close()
            return None
    ip = ip.ljust(256, b"\0")
    try:
        n = 0
        while n < 16 and ip[96 + n] not in _C_SPACE:
            n += 1
        if not n:
            return None
        sector, size = t.find(ip[96:96 + n].decode("latin-1"))
        if not sector:
            return None
        if not t.read(sector, 1):
            t.f.close()
            t = _dc_open(path, DC_LAST)
            if not t:
                return None
        body = t.file(sector, size)
        return hashlib.md5(ip + body).hexdigest() if body is not None else None
    finally:
        if t:
            t.f.close()


# Disc containers with no walk here, refused unread as src/rahash.c refuses
# them (is_disc_image). Not .bin: a Genesis or 2600 cartridge is one too.
DISC_EXTS = ("chd", "cue", "ccd", "toc", "iso", "cso", "img", "m3u", "pbp", "mdf",
             "cbn", "mds", "cdi", "gdi")


def ra_md5(tag, path):
    """RA's hash of one game, or None when there is none to make."""
    if tag == "PSP":
        return psp_md5(path)
    if tag == "DC":
        return dc_md5(path)
    if tag in DISC_TAGS:
        body = psx_hash_input(path)
        return hashlib.md5(body).hexdigest() if body is not None else None
    if tag in ("ARCADE", "NEOGEO", "MAME"):
        # rcheevos's rc_hash_arcade: the set's name, not its chips.
        name = os.path.splitext(os.path.basename(path))[0]
        return hashlib.md5(name.encode()).hexdigest()
    if os.path.splitext(path)[1][1:].lower() in DISC_EXTS:
        return None
    data = rom_bytes(path)
    body = ra_body(tag, data) if data is not None else None
    return hashlib.md5(body).hexdigest() if body is not None else None


def systems_from_cfg(path):
    out = []
    for line in open(path, encoding="utf-8"):
        f = line.rstrip("\n").split("|")
        if len(f) > 7 and f[0] == "sys":
            exts = {e.strip().lower() for e in f[7].split(",") if e.strip()}
            out.append((f[2], f[4], exts))
    return out


def rom_bytes(path):
    """The ROM itself. The library is zipped one game per archive, and RA
    hashes what is inside, not the container.

    THE LARGEST ENTRY, not the first, because that is the one Diatom loads
    (src/zip.c). For one-ROM-per-archive the two are the same file and the
    distinction never shows; for anything else this would hash one game and
    run another, and the achievements would silently be for something else."""
    if path.lower().endswith(".zip"):
        with zipfile.ZipFile(path) as z:
            infos = [i for i in z.infolist() if not i.is_dir() and i.file_size]
            if not infos:
                return None
            return z.read(max(infos, key=lambda i: i.file_size))
    return open(path, "rb").read()


def gameid(md5, tries=3):
    """RA's id for a hash. 0 means RA does not know it; -1 means the question
    could not be asked, which is a different thing and must not be counted as
    an answer."""
    for k in range(tries):
        try:
            body = urllib.parse.urlencode({"r": "gameid", "m": md5}).encode()
            req = urllib.request.Request("https://retroachievements.org/dorequest.php",
                                         data=body, headers={"User-Agent": UA})
            return json.load(urllib.request.urlopen(req, timeout=25)).get("GameID", 0)
        except Exception:
            time.sleep(1.5 * (k + 1))
    return -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--roms", default=os.path.join(ROOT, "ZyperOS-Test-Set", "Roms"))
    ap.add_argument("--system")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()

    systems = systems_from_cfg(os.path.join(ROOT, "config", "systems.cfg"))
    if a.system:
        systems = [s for s in systems if s[0] == a.system]
        if not systems:
            sys.exit(f"ra-check: {a.system} is not in systems.cfg")

    tot = hit = failed = 0
    misses = []
    for folder, tag, exts in systems:
        d = os.path.join(a.roms, folder)
        if not os.path.isdir(d):
            continue
        k = n = 0
        for f in sorted(os.listdir(d)):
            p = os.path.join(d, f)
            if f.startswith(".") or not os.path.isfile(p):
                continue
            if exts and os.path.splitext(f)[1][1:].lower() not in exts:
                continue
            h = ra_md5(tag, p)
            if h is None:
                continue
            n += 1
            g = gameid(h)
            if g > 0:
                k += 1
                if a.verbose:
                    print(f"    {g:>6}  {f}")
            elif g < 0:
                failed += 1
                print(f"    ASK FAILED: {folder}/{f}")
            else:
                misses.append((folder, f))
            time.sleep(0.12)          # civility, and it is what keeps this honest
        hit += k
        tot += n
        print(f"  {folder:<20} {k:>3}/{n}")

    if tot:
        print(f"  {'TOTAL':<20} {hit:>3}/{tot}  ({100 * hit // tot}%)"
              f"   unanswered: {failed}")
    tr = [m for m in misses if "Translated" in m[1]]
    if misses:
        print(f"\n  not recognized: {len(misses)}, of which fan translations: {len(tr)}")
        for folder, f in misses:
            if "Translated" not in f:
                print(f"    {folder}: {f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
