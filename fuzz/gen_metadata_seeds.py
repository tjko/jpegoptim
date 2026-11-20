#!/usr/bin/env python3
"""
Generate a structured seed corpus for the jpegoptim "metadata_modes" fuzzers.

Each seed is what the fuzz harness consumes: a 4 byte little endian option
profile followed by the JPEG file payload. The generator is deterministic,
dependency free, and run from fuzz/build.sh at build time, so the corpus can
always be rebuilt from source (no external sample files needed).

The JPEG cores embedded below were generated once (Pillow) and validated with
djpeg; they are kept as hex constants to avoid any runtime dependency.

SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import os
import struct

# ---------------------------------------------------------------- JPEG cores

CORE_GRAY = ("ffd8ffe000104a46494600010100000100010000ffdb00430008060607060508070707090908"
"0a0c140d0c0b0b0c1912130f141d1a1f1e1d1a1c1c20242e2720222c231c1c2837292c303134"
"34341f27393d38323c2e333432ffc0000b080004000401011100ffc4001f0000010501010101"
"010100000000000000000102030405060708090a0bffc400b510000201030302040305050404"
"0000017d01020300041105122131410613516107227114328191a1082342b1c11552d1f02433"
"627282090a161718191a25262728292a3435363738393a434445464748494a53545556575859"
"5a636465666768696a737475767778797a838485868788898a92939495969798999aa2a3a4a5"
"a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6"
"e7e8e9eaf1f2f3f4f5f6f7f8f9faffda0008010100003f00f0a11263a57fffd9")

CORE_BASE = ("ffd8ffe000104a46494600010100000100010000ffdb00430008060607060508070707090908"
"0a0c140d0c0b0b0c1912130f141d1a1f1e1d1a1c1c20242e2720222c231c1c2837292c303134"
"34341f27393d38323c2e333432ffdb0043010909090c0b0c180d0d1832211c21323232323232"
"3232323232323232323232323232323232323232323232323232323232323232323232323232"
"323232323232ffc00011080004000403012200021101031101ffc4001f000001050101010101"
"0100000000000000000102030405060708090a0bffc400b51000020103030204030505040400"
"00017d01020300041105122131410613516107227114328191a1082342b1c11552d1f0243362"
"7282090a161718191a25262728292a3435363738393a434445464748494a535455565758595a"
"636465666768696a737475767778797a838485868788898a92939495969798999aa2a3a4a5a6"
"a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7"
"e8e9eaf1f2f3f4f5f6f7f8f9faffc4001f010003010101010101010101000000000000010203"
"0405060708090a0bffc400b51100020102040403040705040400010277000102031104052131"
"061241510761711322328108144291a1b1c109233352f0156272d10a162434e125f11718191a"
"262728292a35363738393a434445464748494a535455565758595a636465666768696a737475"
"767778797a82838485868788898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7"
"b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae2e3e4e5e6e7e8e9eaf2f3f4f5f6f7f8f9"
"faffda000c03010002110311003f009f45f0868dfd991ffa37f2a28a2aa3276474e0f115beaf"
"0f7decbab3ffd9")

CORE_PROG = ("ffd8ffe000104a46494600010100000100010000ffdb00430008060607060508070707090908"
"0a0c140d0c0b0b0c1912130f141d1a1f1e1d1a1c1c20242e2720222c231c1c2837292c303134"
"34341f27393d38323c2e333432ffdb0043010909090c0b0c180d0d1832211c21323232323232"
"3232323232323232323232323232323232323232323232323232323232323232323232323232"
"323232323232ffc20011080004000403012200021101031101ffc40015000101000000000000"
"00000000000000000002ffc4001501010100000000000000000000000000000103ffda000c03"
"010002100310000001b0d3ffc400161001010100000000000000000000000000030413ffda00"
"0801010001050219073fffc400191100010500000000000000000000000000030001021321ff"
"da0008010301013f0109275b6affc4001511010100000000000000000000000000000100ffda"
"0008010201013f011bffc400161000030000000000000000000000000000000122ffda000801"
"0100063f02527fffc40014100100000000000000000000000000000000ffda0008010100013f"
"2169ffda000c03010002000300000010f7ffc400171100030100000000000000000000000000"
"00013141ffda0008010301013f10a1c5acffc400161101010100000000000000000000000000"
"010011ffda0008010201013f104c2fffc40015100101000000000000000000000000000000e1"
"ffda0008010100013f108ebfffd9")

# ------------------------------------------------------- option profile bits

CFG_MPF = 1 << 0
CFG_ISO21496 = 1 << 1
CFG_KEEP_ALL = 1 << 2
CFG_STRIP_AI = 1 << 3
CFG_KEEP_ADOBE = 1 << 4
CFG_KEEP_JFXX = 1 << 5
CFG_RETRY = 1 << 6
CFG_NOACTION = 1 << 7
CFG_THRESHOLD = 1 << 8
CFG_FORCE = 1 << 9
CFG_CSV = 1 << 10
CFG_QUIET = 1 << 11
CFG_DEST = 1 << 12
CFG_OVERWRITE = 1 << 13
CFG_PRESERVE_PERMS = 1 << 14
CFG_PRESERVE_MTIME = 1 << 15
CFG_NOFIX = 1 << 16
CFG_MAX_PIXELS = 1 << 17
CFG_STDIN = 1 << 18
CFG_SAVE_EXTRA = 1 << 19
CFG_NO_COM = 1 << 20
CFG_NO_EXIF = 1 << 21
CFG_NO_IPTC = 1 << 22
CFG_NO_ICC = 1 << 23
CFG_NO_XMP = 1 << 24
CFG_NO_JFIF = 1 << 25


def cfg_of(bits=0, verbose=0, tsize=0, mode=0):
    return bits | (verbose << 26) | (tsize << 28) | (mode << 30)


# ------------------------------------------------------------ basic builders

def unhex(s):
    return bytes.fromhex(s)


def seg(marker, payload):
    return bytes([0xFF, marker]) + struct.pack(">H", len(payload) + 2) + payload


def soi():
    return b"\xff\xd8"


def eoi():
    return b"\xff\xd9"


def jfif():
    return seg(0xE0, b"JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00")


def jfxx():
    return seg(0xE0, b"JFXX\x00\x10" + bytes(96))


def icc():
    return seg(0xE2, b"ICC_PROFILE\x00\x00\x01" + bytes(48))


def adobe():
    return seg(0xEE, b"Adobe\x00\x64\x00\x00\x00\x00\x00\x00")


def iptc():
    rec = b"\x1c\x02\x19\x00\x03algorithmic"
    return seg(0xED, b"Photoshop 3.0\x008BIM\x04\x04\x00\x00\x00\x00\x00\x04\x04\x00" + rec)


def iptc_adobe25():
    return seg(0xED, b"Adobe_Photoshop2.5" + bytes(16))


def com(text=b"jpegoptim fuzz metadata seed"):
    return seg(0xFE, text)


def iso21496():
    return seg(0xE2, b"urn:iso:std:iso:ts:21496:-1\x00" + bytes(40))


# ------------------------------------------------------------- Exif builder

def exif(big=True, ai=True):
    """Minimal TIFF with an Exif sub-IFD carrying a UserComment tag."""
    fmt = ">" if big else "<"

    def u16(v):
        return struct.pack(fmt + "H", v)

    def u32(v):
        return struct.pack(fmt + "I", v)

    comment_text = (b"ASCII\x00\x00\x00"
                    b"negative prompt: blurry, cfg scale: 7.5, model hash: abc123"
                    if ai else
                    b"ASCII\x00\x00\x00hello world")
    ifd0_off = 8
    ifd0_size = 2 + 2 * 12 + 4
    exif_ifd_off = ifd0_off + ifd0_size
    exif_ifd_size = 2 + 12 + 4
    uc_off = exif_ifd_off + exif_ifd_size

    tiff = bytearray()
    tiff += (b"MM\x00*" if big else b"II*\x00")
    tiff += u32(ifd0_off)
    # IFD0: Make (inline ASCII), ExifIFDPointer
    tiff += u16(2)
    tiff += u16(0x010F) + u16(2) + u32(4) + b"jpt\x00"
    tiff += u16(0x8769) + u16(4) + u32(1) + u32(exif_ifd_off)
    tiff += u32(0)
    # Exif IFD: UserComment
    tiff += u16(1)
    tiff += u16(0x9286) + u16(7) + u32(len(comment_text)) + u32(uc_off)
    tiff += u32(0)
    tiff += comment_text

    return seg(0xE1, b"Exif\x00\x00" + bytes(tiff))


# -------------------------------------------------------------- XMP builder

XMP_HEAD = (b"<?xpacket begin=\"?\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>"
            b"<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF "
            b"xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
            b"<rdf:Description rdf:about=\"\" "
            b"xmlns:photoshop=\"http://ns.adobe.com/photoshop/1.0/\" "
            b"xmlns:dcterms=\"http://purl.org/dc/terms/\">")


def xmp(ai=True, attribute_form=False):
    """XMP packet carrying (or not) the IPTC digital source type and a
    C2PA provenance reference, in element or attribute form."""
    if ai:
        src = "http://iptc.org/std/vocabulary/1/#/trainedAlgorithmicMedia"
        prov = "c2pa:jumbf manifest locator"
    else:
        src = "http://iptc.org/std/vocabulary/1/#/humanCaptured"
        prov = "human photographer"

    if attribute_form:
        body = ("<rdf:Description photoshop:DigitalSourceType=\"%s\" "
                "dcterms:provenance=\"%s\">"
                "</rdf:Description>" % (src, prov))
        body = body.split("<rdf:Description", 1)[1]
        body = "<rdf:Description" + body
    else:
        body = ("<photoshop:DigitalSourceType>%s</photoshop:DigitalSourceType>"
                "<dcterms:provenance>%s</dcterms:provenance>" % (src, prov))

    packet = (XMP_HEAD + body.encode() +
              b"</rdf:RDF></x:xmpmeta>"
              b"<?xpacket end=\"w\"?>")
    return seg(0xE1, b"http://ns.adobe.com/xap/1.0/\x00" + packet)


# ------------------------------------------------------------ JUMBF builder

def jumbf_c2pa():
    """APP11 JUMBF superbox description marking a C2PA manifest, plus a
    continuation marker of the same box instance and an unrelated box.

    Each segment is 'JP', the 16-bit box instance number and the 32-bit
    sequence number, followed by the boxes; strip_ai_markers() expects
    'jumb' at offset 12 and the description type at offset 20."""
    def app11(box, seq, tail):
        payload = (b"JP" + struct.pack(">H", box) +
                   struct.pack(">I", seq) + tail)
        return seg(0xEB, payload)

    # Superbox: length + 'jumb' + description box ('jumdc2pa').
    desc = (struct.pack(">I", 24) + b"jumb" +
            struct.pack(">I", 16) + b"jumdc2pa" + bytes(4))
    first = app11(1, 1, desc)
    cont = app11(1, 2,
                 struct.pack(">I", 24) + b"jpg " + bytes(12))
    other = app11(2, 1,
                  struct.pack(">I", 16) + b"jpeg" + bytes(4))
    return first + cont + other


# ------------------------------------------------------------- MPF builder

def mpf(big=True, uid=False, frames=False, bad_endian=False, bad_order=False,
        second_img_offset=4096):
    """APP2 MPF marker: 'MPF\\0' + TIFF-style header, Index IFD, MP entry
    list and an optional Attribute IFD. Offsets follow CIPA DC-007 (relative
    to the start of 'MPF\\0')."""
    fmt = ">" if big else "<"

    def u16(v):
        return struct.pack(fmt + "H", v)

    def u32(v):
        return struct.pack(fmt + "I", v)

    n_images = 2
    n_tags = 3 + (1 if uid else 0) + (1 if frames else 0)
    index_ifd_abs = 16  # absolute position of the Index IFD
    index_ifd_size = 2 + n_tags * 12 + 4
    entries_abs = index_ifd_abs + index_ifd_size
    entries_size = n_images * 16
    uid_abs = entries_abs + entries_size
    uid_size = n_images * 33 if uid else 0
    attr_abs = uid_abs + uid_size
    attr_size = 2 + 12 + 4 if not bad_order else 0

    out = bytearray()
    out += b"MPF\x00"
    if bad_endian:
        out += b"XX\x01\x00"
    else:
        out += b"MM\x00*" if big else b"II*\x00"
    out += u32(index_ifd_abs - 4)  # offset to first IFD (from 'MPF\0')
    out += b"\x00\x00\x00\x00"     # reserved (CIPA DC-007: 4 bytes)

    # Tag entries (CIPA requires ascending tag order; bad_order breaks it)
    tag_version = u16(0xB000) + u16(7) + u32(4) + b"0100"
    tag_numimgs = u16(0xB001) + u16(4) + u32(1) + u32(n_images)
    tag_entry = u16(0xB002) + u16(7) + u32(n_images * 16) + u32(entries_abs - 4)
    tag_uid = u16(0xB003) + u16(7) + u32(n_images * 33) + u32(uid_abs - 4)
    tag_frames = u16(0xB004) + u16(4) + u32(1) + u32(2)

    # Index IFD
    out += u16(n_tags)
    if bad_order:
        out += tag_numimgs + tag_version
    else:
        out += tag_version + tag_numimgs + tag_entry
        if uid:
            out += tag_uid
        if frames:
            out += tag_frames
    out += u32(attr_abs - 4 if attr_size else 0)  # next IFD

    # MP Entry list
    out += u32(0x030000) + u32(0x00000400) + u32(0) + u16(0) + u16(0)
    out += u32(0x010001) + u32(100) + u32(second_img_offset) + u16(0) + u16(0)

    if uid:
        for i in range(n_images):
            out += bytes(33)

    if attr_size:
        # Attribute IFD: MPIndividualNum
        out += u16(1)
        out += u16(0xB101) + u16(4) + u32(1) + u32(1)
        out += u32(0)

    return seg(0xE2, bytes(out))


# ---------------------------------------------------------------- seed list

def core(which):
    """Embedded JPEG core without its own SOI (the seed adds one at the
    start; a second SOI mid-stream would make libjpeg abort). The core's
    own EOI terminates the image."""
    return unhex(which)[2:]


def meta_markers():
    return b"".join([
        jfif(), jfxx(),
        exif(big=True, ai=True),
        exif(big=False, ai=False),
        xmp(ai=True), xmp(ai=False), xmp(ai=True, attribute_form=True),
        icc(), iptc(), iptc_adobe25(), adobe(),
        com(), com(b"second comment"),
    ])


def build_all_meta():
    return soi() + meta_markers() + core(CORE_BASE)


def seed_mpf_valid(big=True, **kw):
    return soi() + jfif() + com() + mpf(big=big, **kw) + core(CORE_BASE)


def gen_seeds():
    seeds = []
    add = seeds.append

    # Defaults / sanity (lossless path, CLI-default save flags)
    add(("default-gray", cfg_of(), unhex(CORE_GRAY)))
    add(("default-allmeta", cfg_of(), build_all_meta()))
    add(("default-progressive", cfg_of(),
         soi() + jfif() + com() + core(CORE_PROG)))

    # MPF handling (valid markers, both endians, tag variants)
    add(("mpf-valid-be", cfg_of(CFG_MPF | CFG_SAVE_EXTRA | 1 << 28),
         seed_mpf_valid(big=True)))
    add(("mpf-valid-le", cfg_of(CFG_MPF | CFG_SAVE_EXTRA),
         seed_mpf_valid(big=False)))
    add(("mpf-valid-uid-frames", cfg_of(CFG_MPF | CFG_ISO21496),
         seed_mpf_valid(big=True, uid=True, frames=True)))
    add(("mpf-valid-progressive", cfg_of(CFG_MPF | CFG_SAVE_EXTRA, mode=2),
         soi() + jfif() + mpf(big=True) + core(CORE_PROG)))
    add(("mpf-valid-auto", cfg_of(CFG_MPF | CFG_SAVE_EXTRA, tsize=1, mode=3),
         seed_mpf_valid(big=True)))
    add(("mpf-valid-retry", cfg_of(CFG_MPF | CFG_SAVE_EXTRA, tsize=1),
         seed_mpf_valid(big=True, second_img_offset=100000)))
    add(("mpf-bad-rel-offset", cfg_of(CFG_MPF | CFG_SAVE_EXTRA, tsize=1),
         seed_mpf_valid(big=True, second_img_offset=10)))
    add(("mpf-bad-endian", cfg_of(CFG_MPF | CFG_SAVE_EXTRA),
         seed_mpf_valid(big=True, bad_endian=True)))
    add(("mpf-bad-tag-order", cfg_of(CFG_MPF | CFG_SAVE_EXTRA),
         seed_mpf_valid(big=True, bad_order=True)))

    # Multiple MPF markers ("multiple MPF markers found" path)
    add(("mpf-multiple", cfg_of(CFG_MPF | CFG_SAVE_EXTRA),
         soi() + mpf(big=True) + mpf(big=False) + core(CORE_GRAY)))

    # MPF + ISO 21496-1 HDR duo (--keep-hdr surface)
    add(("mpf-iso21496", cfg_of(CFG_MPF | CFG_ISO21496),
         soi() + jfif() + iso21496() + mpf(big=True) + core(CORE_GRAY)))

    # AI metadata stripping (--strip-ai surface)
    add(("ai-strip-element-form", cfg_of(CFG_STRIP_AI | CFG_FORCE),
         soi() + jfif() + xmp(ai=True) + exif(big=True, ai=True) +
         jumbf_c2pa() + core(CORE_BASE)))
    add(("ai-strip-attribute-form", cfg_of(CFG_STRIP_AI | CFG_FORCE),
         soi() + xmp(ai=True, attribute_form=True) +
         exif(big=False, ai=True) + core(CORE_BASE)))
    add(("ai-nostrip-clean", cfg_of(CFG_STRIP_AI),
         soi() + xmp(ai=False) + exif(big=False, ai=False) +
         core(CORE_BASE)))
    add(("ai-strip-progressive-retry",
         cfg_of(CFG_STRIP_AI | CFG_FORCE | CFG_RETRY, tsize=1, mode=2),
         soi() + xmp(ai=True) + jumbf_c2pa() + core(CORE_PROG)))

    # keep-all / strip-all matrix on the full metadata set
    add(("keepall-allmeta", cfg_of(CFG_KEEP_ALL | CFG_SAVE_EXTRA, verbose=3),
         soi() + meta_markers() + iso21496() + mpf(big=True) +
         core(CORE_BASE)))
    add(("stripall-allmeta",
         cfg_of(CFG_NO_COM | CFG_NO_EXIF | CFG_NO_IPTC | CFG_NO_ICC |
                CFG_NO_XMP | CFG_NO_JFIF),
         build_all_meta()))
    add(("strip-partial", cfg_of(CFG_NO_EXIF | CFG_NO_XMP | CFG_KEEP_ADOBE |
                                 CFG_KEEP_JFXX),
         build_all_meta()))
    add(("keepall-verbose-csv",
         cfg_of(CFG_KEEP_ALL | CFG_CSV | CFG_QUIET, verbose=2, tsize=1),
         soi() + meta_markers() + core(CORE_BASE)))

    # save_extra: extraneous data after EOI (with and without MPF patching)
    add(("trailing-extra", cfg_of(CFG_SAVE_EXTRA, tsize=1),
         soi() + jfif() + com() + core(CORE_BASE) +
         b"EXTRA-TRAILING-DATA-AFTER-EOI"))
    add(("trailing-extra-mpf", cfg_of(CFG_MPF | CFG_SAVE_EXTRA, tsize=1),
         soi() + mpf(big=True) + core(CORE_BASE) + bytes(24)))

    # Error / fix paths
    add(("broken-truncated-nofix", cfg_of(CFG_NOFIX, tsize=1),
         soi() + jfif() + core(CORE_BASE)[:len(core(CORE_BASE)) // 2]))
    add(("broken-truncated-fix", cfg_of(),
         soi() + jfif() + core(CORE_PROG)[:60]))
    add(("max-pixels-abort", cfg_of(CFG_MAX_PIXELS),
         soi() + big_sof()[2:]))

    # File destination / overwrite / preserve / noaction paths
    add(("dest-overwrite", cfg_of(CFG_DEST | CFG_OVERWRITE, tsize=1),
         build_all_meta()))
    add(("dest-exists-abort", cfg_of(CFG_DEST, tsize=1), build_all_meta()))
    add(("preserve-perms-mtime",
         cfg_of(CFG_PRESERVE_PERMS | CFG_PRESERVE_MTIME, tsize=1),
         build_all_meta()))
    add(("noaction", cfg_of(CFG_NOACTION, tsize=1), build_all_meta()))
    add(("threshold-skip", cfg_of(CFG_THRESHOLD), build_all_meta()))
    add(("threshold-force", cfg_of(CFG_THRESHOLD | CFG_FORCE),
         build_all_meta()))
    add(("stdin-mode", cfg_of(CFG_STDIN, tsize=1), build_all_meta()))

    return seeds


def big_sof():
    """Tiny JPEG with oversized SOF0 dimensions (max-pixels abort path).
    2000x2000 = 4 MP > the 1 MP limit, but small enough that processing the
    image without --max-pixels stays well inside fuzzer memory limits."""
    core = bytearray(unhex(CORE_GRAY))
    i = core.find(b"\xff\xc0")
    if i >= 0:
        core[i + 5:i + 7] = struct.pack(">H", 2000)  # height
        core[i + 7:i + 9] = struct.pack(">H", 2000)  # width
    return bytes(core)


def main():
    parser = argparse.ArgumentParser(
        description="Generate jpegoptim metadata_modes seed corpus")
    parser.add_argument("--out", required=True, help="output directory")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    for name, cfg, payload in gen_seeds():
        path = os.path.join(args.out, name + ".jpg")
        with open(path, "wb") as f:
            f.write(struct.pack("<I", cfg) + payload)
        print("wrote %s (%d bytes, cfg=0x%08x)" % (path,
                                                   4 + len(payload), cfg))


if __name__ == "__main__":
    main()

# eof :-)
