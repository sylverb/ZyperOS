#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Made-up discs for the hash checks: a PlayStation disc and a PSP disc built
here, byte for byte the same on every run, and the containers ZyperOS reads
them from - .cue/.bin, .iso, .cso and .pbp. No game data: every file on them
is generated (text, counters, hashed noise), so they can be built at check
time and the containers made from them committed.

    tools/rahash-fixtures.py   makes the .chd files from these with chdman,
                               and the hashes the checks expect, with
                               upstream rcheevos (test-only, on a Mac)
    tools/rahash-check.py      builds these again and holds both hashers to
                               those hashes

The ISO 9660 writer is plain: a volume descriptor at 16, path tables,
directories in name order (a record never crosses a sector), then files,
each on its own sectors. The CD sectors carry real EDC and ECC, so chdman
takes the sync and ECC out of them as it does from a real disc's.
"""
import hashlib
import struct
import zlib


def noise(seed, n):
    """n bytes no compressor shrinks, the same every run"""
    out = bytearray()
    i = 0
    while len(out) < n:
        out += hashlib.sha256(seed.encode() + i.to_bytes(8, "little")).digest()
        i += 1
    return bytes(out[:n])


def prose(seed, n):
    """n bytes of words, which compress: most of a real disc does"""
    words = ("zyper", "pixel", "handheld", "achievement", "sector", "track",
             "disc", "memory", "card", "level", "boss", "save", "point", "the",
             "of", "and", "a", "to", "in", "is", "it", "that", "on", "for")
    h = hashlib.sha256(seed.encode()).digest()
    out = bytearray()
    i = 0
    while len(out) < n:
        out += words[h[i % 32] % len(words)].encode() + b" "
        if i % 32 == 31:
            h = hashlib.sha256(h).digest()
        i += 1
    return bytes(out[:n])


# ------------------------------------------------------------- ISO 9660 ----

def _both16(v):
    return struct.pack("<H", v) + struct.pack(">H", v)


def _both32(v):
    return struct.pack("<I", v) + struct.pack(">I", v)


_DATE = bytes([126, 11, 14, 2, 30, 0, 0])      # 2026-11-14 02:30:00 UTC


def _record(lba, size, name, is_dir):
    n = len(name)
    ln = 33 + n + (1 - n % 2)
    return (bytes([ln, 0]) + _both32(lba) + _both32(size) + _DATE
            + bytes([2 if is_dir else 0, 0, 0]) + _both16(1) + bytes([n]) + name
            + bytes(ln - 33 - n))


class Iso:
    """Files by path ("PSP_GAME/SYSDIR/EBOOT.BIN"); directories follow.

    `base` is the disc sector the image's first is: every LBA written is
    counted from there (a GD-ROM's file system starts at 45000, a MIL-CD's
    at its second session). `system` is what the 16 sectors before the
    volume descriptor hold (a Dreamcast's IP.BIN). `at` puts a file at a
    sector of the image's own, after the rest, which is how a file comes to
    be on a later track than its directory."""

    def __init__(self, volume, system_id="PLAYSTATION", base=0, system=b"", at=None):
        self.volume = volume
        self.files = {}
        self.system_id, self.base, self.system = system_id, base, system
        self.at = dict(at or {})

    def add(self, path, data):
        self.files[path] = bytes(data)

    def build(self):
        # Every directory, parents first
        dirs = {""}
        for p in self.files:
            parts = p.split("/")[:-1]
            for i in range(1, len(parts) + 1):
                dirs.add("/".join(parts[:i]))
        order = sorted(dirs, key=lambda d: (d.count("/") if d else -1, d))

        def parent_of(p):
            return p.rsplit("/", 1)[0] if "/" in p else ""

        def children(d):
            subs = sorted(x for x in dirs if x and parent_of(x) == d)
            fs = sorted(p for p in self.files if parent_of(p) == d)
            return subs, fs

        # Sizes of the directories, which need the records, which need no
        # LBAs to be counted: lay the records out once with zeros
        def dir_bytes(d, lbas, sizes):
            subs, fs = children(d)
            parent = parent_of(d)
            recs = [_record(lbas.get(d, 0), sizes.get(d, 0), b"\0", True),
                    _record(lbas.get(parent, 0), sizes.get(parent, 0), b"\1", True)]
            for s in subs:
                recs.append(_record(lbas.get(s, 0), sizes.get(s, 0),
                                    s.rsplit("/", 1)[-1].encode(), True))
            for f in fs:
                recs.append(_record(lbas.get(f, 0), len(self.files[f]),
                                    f.rsplit("/", 1)[-1].encode() + b";1", False))
            out = bytearray()
            for r in recs:
                if len(out) % 2048 + len(r) > 2048:
                    out += bytes(2048 - len(out) % 2048)
                out += r
            out += bytes(-len(out) % 2048)
            return bytes(out)

        sizes = {d: len(dir_bytes(d, {}, {})) for d in order}
        lba = 20                                  # 16 PVD, 17 end, 18/19 path tables
        lbas = {}
        for d in order:
            lbas[d] = lba
            lba += sizes[d] // 2048
        for f in sorted(self.files):
            if f in self.at:
                continue
            lbas[f] = lba
            lba += max(1, -(-len(self.files[f]) // 2048))
        self.end = lba                            # where the laid-out files end
        for f, where in self.at.items():
            assert where >= lba, f"{f} at {where}, before the files' end {lba}"
            lbas[f] = where
            lba = max(lba, where + max(1, -(-len(self.files[f]) // 2048)))
        total = lba

        img = bytearray(total * 2048)
        b = self.base
        disc = {k: v + b for k, v in lbas.items()}
        for d in order:
            img[lbas[d] * 2048:lbas[d] * 2048 + sizes[d]] = dir_bytes(d, disc, sizes)
        for f, data in self.files.items():
            img[lbas[f] * 2048:lbas[f] * 2048 + len(data)] = data
        img[0:len(self.system)] = self.system

        # Path tables, little- then big-endian, dirs numbered in order
        num = {d: i + 1 for i, d in enumerate(order)}
        lpt, mpt = bytearray(), bytearray()
        for d in order:
            name = d.rsplit("/", 1)[-1].encode() if d else b"\0"
            parent = num[d.rsplit("/", 1)[0] if "/" in d else ""]
            pad = bytes(len(name) % 2)
            lpt += bytes([len(name), 0]) + struct.pack("<IH", disc[d], parent) + name + pad
            mpt += bytes([len(name), 0]) + struct.pack(">IH", disc[d], parent) + name + pad
        img[18 * 2048:18 * 2048 + len(lpt)] = lpt
        img[19 * 2048:19 * 2048 + len(mpt)] = mpt

        pvd = bytearray(2048)
        pvd[0:7] = b"\x01CD001\x01"
        pvd[8:40] = self.system_id.encode().ljust(32)
        pvd[40:72] = self.volume.encode().ljust(32)
        pvd[80:88] = _both32(total + b)
        pvd[120:124] = _both16(1)
        pvd[124:128] = _both16(1)
        pvd[128:132] = _both16(2048)
        pvd[132:140] = _both32(len(lpt))
        pvd[140:144] = struct.pack("<I", 18 + b)
        pvd[148:152] = struct.pack(">I", 19 + b)
        pvd[156:190] = _record(disc[""], sizes[""], b"\0", True)
        pvd[881] = 1
        img[16 * 2048:17 * 2048] = pvd
        img[17 * 2048:17 * 2048 + 7] = b"\xffCD001\x01"
        return bytes(img)


# ----------------------------------------------------------- CD sectors ----

_F, _B, _EDC = [0] * 256, [0] * 256, [0] * 256
for _i in range(256):
    _j = (_i << 1) ^ (0x11D if _i & 0x80 else 0)
    _F[_i] = _j & 0xff
    _B[_i ^ (_j & 0xff)] = _i
    _e = _i
    for _ in range(8):
        _e = (_e >> 1) ^ (0xD8018001 if _e & 1 else 0)
    _EDC[_i] = _e


def _edc(data):
    e = 0
    for b in data:
        e = (e >> 8) ^ _EDC[(e ^ b) & 0xff]
    return e


def _ecc_block(src, major_count, minor_count, major_mult, minor_inc):
    size = major_count * minor_count
    out = bytearray(major_count * 2)
    for major in range(major_count):
        index = (major >> 1) * major_mult + (major & 1)
        a = b = 0
        for _ in range(minor_count):
            t = src[index]
            index += minor_inc
            if index >= size:
                index -= size
            a ^= t
            b ^= t
            a = _F[a]
        a = _B[_F[a] ^ b]
        out[major] = a
        out[major + major_count] = a ^ b
    return out


def _ecc(sector, zero_address):
    s = bytearray(sector)
    if zero_address:
        s[12:16] = bytes(4)
    p = _ecc_block(s[12:12 + 2064], 86, 24, 2, 86)
    s[2076:2076 + 172] = p
    q = _ecc_block(s[12:12 + 2236], 52, 43, 86, 88)
    s[2248:2248 + 104] = q
    if zero_address:
        s[12:16] = sector[12:16]
    return bytes(s)


SYNC = b"\x00" + b"\xff" * 10 + b"\x00"


def _msf(lba):
    v = lba + 150
    bcd = lambda x: ((x // 10) << 4) | (x % 10)
    return bytes([bcd(v // 4500), bcd(v // 75 % 60), bcd(v % 75)])


def mode2_form1(lba, data, last=False):
    sub = bytes([0, 0, 0x08 | (0x81 if last else 0), 0]) * 2
    s = bytearray(SYNC + _msf(lba) + b"\x02" + sub + data + bytes(4 + 276))
    s[2072:2076] = struct.pack("<I", _edc(s[16:2072]))
    return _ecc(bytes(s), True)


def mode2_form2(lba, data):
    """An XA audio sector: no ECC, 2324 bytes of data"""
    sub = bytes([1, 1, 0x64, 0]) * 2
    s = bytearray(SYNC + _msf(lba) + b"\x02" + sub + data[:2324].ljust(2324, b"\0") + bytes(4))
    s[2348:2352] = struct.pack("<I", _edc(s[16:2348]))
    return bytes(s)


def mode1(lba, data):
    s = bytearray(SYNC + _msf(lba) + b"\x01" + data + bytes(4 + 8 + 276))
    s[2064:2068] = struct.pack("<I", _edc(s[0:2064]))
    return _ecc(bytes(s), False)


def raw_track(iso, mode=2, xa=(), base=0):
    """2048-byte sectors as raw 2352-byte ones, the first at disc sector
    `base`. `xa` are sector ranges to write as Form 2 audio instead (a file
    of them reads as noise to a walk, which never goes there)."""
    out = bytearray()
    n = len(iso) // 2048
    for lba in range(n):
        d = iso[lba * 2048:(lba + 1) * 2048]
        if any(a <= lba < b for a, b in xa):
            out += mode2_form2(base + lba, d + d[:276])
        elif mode == 1:
            out += mode1(base + lba, d)
        else:
            out += mode2_form1(base + lba, d, last=lba == n - 1)
    return bytes(out)


def audio_track(seconds_frames, seed):
    """CD audio: a sine-ish wave, 16-bit stereo, little-endian like a .bin"""
    import math
    out = bytearray()
    k = 0
    for _ in range(seconds_frames * 588):
        v = int(9000 * math.sin(k / 23.0)) + (hashlib.md5(f"{seed}{k}".encode()).digest()[0] - 128)
        out += struct.pack("<hh", v, -v)
        k += 1
    return bytes(out)


# ---------------------------------------------------------------- discs ----

def psx_exe(text_size, seed):
    """A PS-X EXE: its header says how much follows it"""
    h = bytearray(2048)
    h[0:8] = b"PS-X EXE"
    h[16:20] = struct.pack("<I", 0x80010000)    # pc0
    h[24:28] = struct.pack("<I", 0x80010000)    # t_addr
    h[28:32] = struct.pack("<I", text_size)     # t_size
    h[0x4c:0x4c + 30] = b"Sony Computer Entertainment Inc"[:30]
    body = bytearray()
    while len(body) < text_size:
        body += prose(seed + str(len(body)), 1900) + noise(seed + str(len(body)), 148)
    return bytes(h) + bytes(body[:text_size])


def ps1_iso(name="ZYPER", serial="SLUS_123.45", subdir=False, seed="ps1", small=False):
    """A PlayStation disc's files. The EXE is a few hunks long, so it starts
    and ends mid-hunk in a CHD; filler before it moves it off sector 23.
    `small` is the one whose CHDs are committed: less noise, fewer sectors."""
    iso = Iso(name)
    exe = psx_exe(0x5000 if small else 0x1B000, seed)
    boot = f"EXE\\{serial}" if subdir else serial
    iso.add("SYSTEM.CNF", f"BOOT = cdrom:\\{boot};1\r\nTCB = 4\r\nEVENT = 10\r\n"
            f"STACK = 801FFF00\r\n".encode())
    iso.add("A_INTRO.STR", prose(seed + "intro", (3 if small else 40) * 2048))
    iso.add(f"EXE/{serial}" if subdir else serial, exe)
    iso.add("DATA/LEVELS.DAT", prose(seed + "levels", (4 if small else 30) * 2048)
            + noise(seed + "lv", 600 if small else 6000))
    iso.add("ZZ_MUSIC.XA", noise(seed + "music", (3 if small else 24) * 2048))
    for i in range(50):                        # a root longer than a sector
        iso.add(f"FILE{i:02d}.BIN", prose(f"{seed}f{i}", 24 + i if small else 300 + 97 * i))
    img = iso.build()
    return img, (f"EXE/{serial}" if subdir else serial)


def xa_ranges(img):
    """Where ZZ_MUSIC.XA is, for raw_track to write it as XA audio"""
    pvd = img[16 * 2048:17 * 2048]
    root = struct.unpack("<I", pvd[158:162])[0]
    size = struct.unpack("<I", pvd[166:170])[0]
    for s in range(root, root + size // 2048):
        sec = img[s * 2048:(s + 1) * 2048]
        r = 0
        while r < 2048 - 34 and sec[r]:
            n = sec[r + 32]
            if sec[r + 33:r + 33 + n] == b"ZZ_MUSIC.XA;1":
                lba = struct.unpack("<I", sec[r + 2:r + 6])[0]
                ln = struct.unpack("<I", sec[r + 10:r + 14])[0]
                return [(lba, lba + ln // 2048)]
            r += sec[r]
    return []


def sfo(entries):
    """A PARAM.SFO: the PSP's own key-value file"""
    keys, data, index = bytearray(), bytearray(), bytearray()
    for k, v in entries:
        if isinstance(v, int):
            raw, fmt, ln, mx = struct.pack("<I", v), 0x0404, 4, 4
        else:
            raw = v.encode() + b"\0"
            fmt, ln, mx = 0x0204, len(raw), (len(raw) + 3) & ~3
        index += struct.pack("<HHIII", len(keys), fmt, ln, mx, len(data))
        keys += k.encode() + b"\0"
        data += raw.ljust(mx, b"\0")
    keys += bytes(-len(keys) % 4)
    kstart = 20 + len(index)
    dstart = kstart + len(keys)
    return (b"\0PSF" + struct.pack("<IIII", 0x0101, kstart, dstart, len(entries))
            + index + keys + data)


def psp_iso(title="ZYPER TEST", disc_id="ULUS99999", seed="psp", small=False):
    iso = Iso(title.replace(" ", "_"))
    iso.add("UMD_DATA.BIN", f"{disc_id[:4]}-{disc_id[4:]}|0000000000000001|0001|G".encode())
    iso.add("PSP_GAME/PARAM.SFO", sfo([("BOOTABLE", 1), ("CATEGORY", "UG"),
                                       ("DISC_ID", disc_id), ("DISC_VERSION", "1.00"),
                                       ("PARENTAL_LEVEL", 1), ("PSP_SYSTEM_VER", "6.60"),
                                       ("REGION", 32768), ("TITLE", title)]))
    iso.add("PSP_GAME/ICON0.PNG", b"\x89PNG\r\n\x1a\n" + noise(seed + "icon", 900 if small else 9000))
    # Encrypted on a real disc, so noise: chdman stores most of it plain
    eboot = (b"~PSP" + noise(seed + "eboot", 9000 if small else 150000)
             + prose(seed + "eb", 30000 if small else 60000))
    iso.add("PSP_GAME/SYSDIR/EBOOT.BIN", eboot)
    iso.add("PSP_GAME/SYSDIR/BOOT.BIN", prose(seed + "boot", 20000))
    iso.add("PSP_GAME/SYSDIR/UPDATE/DATA.BIN", prose(seed + "upd", 5000))
    iso.add("PSP_GAME/USRDIR/DATA.PAK", prose(seed + "pak", (8 if small else 90) * 2048))
    return iso.build()


def cue(binname, mode="MODE2/2352", audio_bin=None):
    s = f'FILE "{binname}" BINARY\n  TRACK 01 {mode}\n    INDEX 01 00:00:00\n'
    if audio_bin:
        s += f'FILE "{audio_bin}" BINARY\n  TRACK 02 AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:02:00\n'
    return s


# ------------------------------------------------------------ containers ----

def cso(iso, block=2048, align=0, ver=1, plain_every=0):
    """CISO: a 24-byte header, an index, raw-deflate blocks (a block that
    would not shrink, or every plain_every'th, stored plain)"""
    n = -(-len(iso) // block)
    idx, body = [], bytearray()
    start = 24 + (n + 1) * 4
    start += -start % (1 << align)
    for i in range(n):
        d = iso[i * block:(i + 1) * block].ljust(block, b"\0")
        c = zlib.compressobj(9, zlib.DEFLATED, -15)
        z = c.compress(d) + c.flush()
        pos = start + len(body)
        if len(z) >= block or (plain_every and i % plain_every == plain_every - 1):
            idx.append(0x80000000 | (pos >> align))
            body += d
        else:
            idx.append(pos >> align)
            body += z
        body += bytes(-(start + len(body)) % (1 << align))
    idx.append((start + len(body)) >> align)
    hdr = b"CISO" + struct.pack("<IQIBB2x", 0 if ver == 0 else 0x18, len(iso), block, ver, align)
    out = bytearray(hdr + b"".join(struct.pack("<I", x) for x in idx))
    out += bytes(start - len(out))
    return bytes(out + body)


def _psisoimg(raw, disc_id, stored=()):
    """PSISOIMG0000: blocks of 16 raw sectors, raw deflate (or as they are,
    for the indices in `stored`), the index at +0x4000, the data at +0x100000"""
    blk = 16 * 2352
    n = -(-len(raw) // blk)
    idx, body = bytearray(), bytearray()
    for i in range(n):
        d = raw[i * blk:(i + 1) * blk].ljust(blk, b"\0")
        if i in stored:
            z = d
        else:
            c = zlib.compressobj(9, zlib.DEFLATED, -15)
            z = c.compress(d) + c.flush()
        idx += struct.pack("<IHH", len(body), len(z), 1) + hashlib.sha1(z).digest()[:16] + bytes(8)
        body += z
    head = bytearray(0x100000)
    head[0:12] = b"PSISOIMG0000"
    head[12:16] = struct.pack("<I", 0x100000 + len(body))
    head[0x400:0x400 + len(disc_id)] = disc_id.encode()
    # The TOC PCSX ReARMed reads: A0 (first track), A1 (last), A2 (lead-out)
    toc = bytearray(10 * 4)
    frames = len(raw) // 2352 + 150
    bcd = lambda x: ((x // 10) << 4) | (x % 10)
    toc[0:10] = bytes([0x41, 0, 0xA0, 0, 0, 0, 0, 0x01, 0x20, 0])
    toc[10:20] = bytes([0x41, 0, 0xA1, 0, 0, 0, 0, 0x01, 0, 0])
    toc[20:30] = bytes([0x41, 0, 0xA2, 0, 0, 0, 0, bcd(frames // 4500), bcd(frames // 75 % 60), bcd(frames % 75)])
    toc[30:40] = bytes([0x41, 0, 0x01, 0, 0, 0, 0, 0x00, 0x02, 0x00])
    head[0x800:0x800 + len(toc)] = toc
    head[0x4000:0x4000 + len(idx)] = idx
    return bytes(head + body)


def _pbp(psar, title):
    sfo_ = sfo([("BOOTABLE", 1), ("CATEGORY", "ME"), ("DISC_ID", "SLUS12345"),
                ("TITLE", title)])
    off = 40
    offs = [off]                                # PARAM.SFO
    off += len(sfo_)
    offs += [off] * 6                           # ICON0 .. DATA.PSP, empty
    pad = -off % 0x10000
    offs.append(off + pad)                      # DATA.PSAR
    return (b"\0PBP" + struct.pack("<I", 0x00010000) + b"".join(struct.pack("<I", o) for o in offs)
            + sfo_ + bytes(pad) + psar)


def pbp_psx(raw, title="ZYPER", stored=()):
    return _pbp(_psisoimg(raw, "_SLUS_12345", stored), title)


def pbp_psx_multi(raws, title="ZYPER"):
    """PSTITLEIMG000000: each disc's PSISOIMG, their offsets at +0x200"""
    head = bytearray(0x8000)
    head[0:16] = b"PSTITLEIMG000000"
    discs, off = [], len(head)
    for i, r in enumerate(raws):
        img = _psisoimg(r, f"_SLUS_1234{i}")
        discs.append(img)
        head[0x200 + 4 * i:0x204 + 4 * i] = struct.pack("<I", off)
        off += len(img)
    return _pbp(bytes(head) + b"".join(discs), title)


def pbp_psp(seed="pspn"):
    """A PSN game's EBOOT.PBP; RA hashes it whole, so its insides are moot"""
    return _pbp(b"NPUMDIMG" + noise(seed, 300000), "ZYPER PSN")


# ------------------------------------------------------------- Dreamcast ----
#
# A Dreamcast disc as RetroAchievements hashes it (rc_hash_dreamcast): the
# first 256 bytes of IP.BIN, at the start of its data track, then the boot
# file IP.BIN names at byte 96. A GD-ROM has a small single-density area
# (track 1 data, track 2 audio) and its high-density area from sector 45000
# (track 3, and on some discs more tracks after it); a MIL-CD, as a .cdi
# holds one, has an audio track and then a second session whose data track
# holds IP.BIN and the file system. No game data: IP.BIN's bootstrap and
# the boot file are made up here like everything else.

GD_HD = 45000                  # the high-density area's first sector
MIL_LBA = 11702                # a MIL-CD's second session (a 4-second track 1)


def dc_ip(boot="1ST_READ.BIN", title="ZYPEROS TEST DISC", product="T-99999", seed="ip"):
    """IP.BIN: the 256 bytes of meta RA hashes, then its bootstrap, 16
    sectors in all (mc.pp.se/dc/ip0000.bin.html)"""
    meta = (b"SEGA SEGAKATANA " + b"SEGA ENTERPRISES" + b"5D63 GD-ROM1/1  "
            + b"JUE     " + b"0799A10 " + product.encode().ljust(10) + b"V1.000"
            + b"20261119        " + boot.encode().ljust(16) + b"ZYPEROS         "
            + title.encode().ljust(128))
    assert len(meta) == 256
    return meta + prose(seed + "boot", 0x3000) + noise(seed + "code", 0x8000 - 256 - 0x3000)


def dc_exe(size, seed):
    """A boot file: SH-4 code to the hash, made up"""
    body = bytearray()
    while len(body) < size:
        body += noise(seed + str(len(body)), 1500) + prose(seed + "s" + str(len(body)), 548)
    return bytes(body[:size])


def dc_hd_iso(seed="dc", boot="1ST_READ.BIN", exe_size=0x1B234, base=GD_HD, boot_at=None):
    """The data track a Dreamcast boots from: IP.BIN in its first 16
    sectors, then an ISO 9660 file system counted from `base`. `boot_at`
    puts the boot file that many sectors in, past a gap, for a disc whose
    boot file is on a later track. Returns the image, its IP.BIN, the boot
    file, and the Iso (for where the laid-out files end)."""
    ip = dc_ip(boot, seed=seed + "ip")
    exe = dc_exe(exe_size, seed + "exe")
    iso = Iso("ZYPEROS_DC", system_id="SEGA SEGAKATANA", base=base, system=ip,
              at={boot: boot_at} if boot_at else None)
    iso.add(boot, exe)
    iso.add("0GDTEX.PVR", noise(seed + "tex", 3000))
    iso.add("DATA/STAGE1.BIN", prose(seed + "st1", 9 * 2048))
    iso.add("DATA/STAGE2.BIN", prose(seed + "st2", 5 * 2048 + 77))
    for i in range(40):                         # a root longer than a sector
        iso.add(f"SND{i:02d}.ADX", prose(f"{seed}a{i}", 40 + 13 * i))
    return iso.build(), ip, exe, iso


def dc_hash_input(ip, exe):
    """What rc_hash_dreamcast takes the md5 of"""
    return ip[:256] + exe


def _pad_sectors(raw, n, sector=2352):
    return raw + bytes(n * sector - len(raw)) if len(raw) < n * sector else raw


def gdrom(seed="dc", split=False, small=False, exe=None):
    """A GD-ROM as a .gdi and its track files, and as a Redump-style .cue
    (one file a track, track 2's pregap in its file). `split` puts the boot
    file on track 5, past an audio track 4, as some games have it. `small`
    is the one whose CHDs are committed: short tracks 1, 2 and 4, a smaller
    boot file. `exe` is the boot file's size, for one other than those.
    Returns {name: bytes} for both sheets' files, and the bytes RA hashes."""
    lo = Iso("ZYPEROS_LD", system_id="SEGA SEGAKATANA")
    lo.add("README.TXT", prose(seed + "lo", 3000))
    n1, na = (30, 75) if small else (300, 300)
    exe = exe or (0x9876 if small else 0x1B234)
    t1 = _pad_sectors(raw_track(lo.build(), mode=1), n1)
    t2 = audio_track(na, seed + "a2")
    t2_lba = n1 + 150
    if not split:
        img, ip, exe, _ = dc_hd_iso(seed, exe_size=exe)
        t3 = raw_track(img, mode=1, base=GD_HD)
        tracks = [(1, 0, 4, t1), (2, t2_lba, 0, t2), (3, GD_HD, 4, t3)]
    else:
        # Track 3 to the files' end, an audio track 4 after a 2-second gap,
        # and track 5 after another, holding the boot file
        probe = dc_hd_iso(seed, exe_size=exe, boot_at=None)[3]
        n3 = probe.end + 4
        t4 = audio_track(na, seed + "a4")
        t4_lba = GD_HD + n3 + 150
        t5_lba = t4_lba + na + 150
        img, ip, exe, _ = dc_hd_iso(seed, exe_size=exe, boot_at=t5_lba - GD_HD)
        t3 = raw_track(img[:n3 * 2048], mode=1, base=GD_HD)
        t5 = raw_track(img[(t5_lba - GD_HD) * 2048:], mode=1, base=t5_lba)
        tracks = [(1, 0, 4, t1), (2, t2_lba, 0, t2), (3, GD_HD, 4, t3),
                  (4, t4_lba, 0, t4), (5, t5_lba, 4, t5)]
    files = {}
    gdi = [str(len(tracks))]
    for num, lba, kind, data in tracks:
        name = f"track{num:02d}." + ("raw" if kind == 0 else "bin")
        files[name] = data
        gdi.append(f"{num} {lba} {kind} 2352 {name} 0")
    files["disc.gdi"] = ("\n".join(gdi) + "\n").encode()
    # The Redump way: each track its own file, track 2's file starting with
    # its 2-second pregap, and where the sheet puts track 3 is not 45000 -
    # its sectors' own headers say that
    cue = []
    for num, lba, kind, data in tracks:
        name = f"Disc (Track {num}).bin"
        if num == 3:
            cue.append("REM HIGH-DENSITY AREA")
        elif num == 1:
            cue.append("REM SINGLE-DENSITY AREA")
        cue.append(f'FILE "{name}" BINARY')
        if kind == 0:
            files[name] = bytes(150 * 2352) + data
            cue += [f"  TRACK {num:02d} AUDIO", "    INDEX 00 00:00:00", "    INDEX 01 00:02:00"]
        else:
            files[name] = data
            cue += [f"  TRACK {num:02d} MODE1/2352", "    INDEX 01 00:00:00"]
    files["disc.cue"] = ("\r\n".join(cue) + "\r\n").encode()
    return files, dc_hash_input(ip, exe)


# A DiscJuggler image's descriptor, the layout Flycast's reader takes
# (deps/chdpsr/cdipsr.cpp): written field by field, the fields it skips as
# zeros. Version 3.5 counts where the descriptor starts back from the end.
CDI_V2, CDI_V3, CDI_V35 = 0x80000004, 0x80000005, 0x80000006
_CDI_MARK = bytes([0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF])
_CDI_SIZE = {2048: 0, 2336: 1, 2352: 2, 2448: 4}


def _cdi_track(version, mode, ss, pregap, length, lba, name=b"ZYPER.CDI"):
    return (struct.pack("<I", 0) + _CDI_MARK + _CDI_MARK + bytes(4)
            + bytes([len(name)]) + name + bytes(11 + 4 + 4) + struct.pack("<I", 0)
            + bytes(2) + struct.pack("<II", pregap, length) + bytes(6)
            + struct.pack("<I", mode) + bytes(12) + struct.pack("<II", lba, pregap + length)
            + bytes(16) + struct.pack("<I", _CDI_SIZE[ss]) + bytes(29)
            + (bytes(5) + struct.pack("<I", 0) if version != CDI_V2 else b""))


def cdi(sessions, version=CDI_V35):
    """sessions: [[(mode, sector size, pregap, length, lba, bytes), ...], ...],
    each track's bytes its pregap then its sectors"""
    body = bytearray()
    desc = bytearray(struct.pack("<H", len(sessions)))
    for tracks in sessions:
        desc += struct.pack("<H", len(tracks))
        for mode, ss, pregap, length, lba, data in tracks:
            assert len(data) == (pregap + length) * ss, (len(data), pregap, length, ss)
            body += data
            desc += _cdi_track(version, mode, ss, pregap, length, lba)
        desc += bytes(4 + 8 + (1 if version != CDI_V2 else 0))
    start = len(body)
    total = start + len(desc) + 8
    tail = struct.pack("<II", version, total - start if version == CDI_V35 else start)
    return bytes(body + desc + tail)


def milcd(seed="dcm", ss=2352, version=CDI_V35):
    """A MIL-CD as a .cdi: session 1 an audio track, session 2 the data
    track at MIL_LBA, Mode 2 (2352 or 2336 bytes a sector) or Mode 1
    cooked (2048). Returns the .cdi and the bytes RA hashes."""
    img, ip, exe, _ = dc_hd_iso(seed, exe_size=0x9876, base=MIL_LBA)
    audio = audio_track(150 + 300, seed + "a1")
    if ss == 2048:
        mode, data = 1, bytes(150 * 2048) + img
    else:
        raw = raw_track(img, mode=2, base=MIL_LBA)
        if ss == 2336:
            raw = b"".join(raw[i + 16:i + 2352] for i in range(0, len(raw), 2352))
        mode, data = 2, bytes(150 * ss) + raw
    n = len(img) // 2048
    # Flycast's track starts at its LBA plus its pregap, from -150: the
    # listed LBA of a track whose pregap is 150 is its first sector's
    sessions = [[(0, 2352, 150, 300, 0, audio)],
                [(mode, ss, 150, n, MIL_LBA, data)]]
    return cdi(sessions, version), dc_hash_input(ip, exe)
