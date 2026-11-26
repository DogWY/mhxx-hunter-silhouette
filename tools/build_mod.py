"""Build a paired MHGU 1.4.0 NSO + IPS32 from a local updated ExeFS dump."""
import argparse
import ctypes
import ctypes.util
import hashlib
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import tempfile
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

ROOT = Path(__file__).resolve().parents[1]
MAIN_ID = bytes.fromhex("FB08F1D20FD1204FE7070672777F869B").ljust(32, b"\0")
SUBSDK0_ID = bytes.fromhex("C2774FA4CACE9ECC6A712BFC839214C19515F36E").ljust(32, b"\0")
TEXT_SHA = "09ff3ff102cf7aa14c9be6be8b7fa70304ba7ab57ccd1215b17f7fb106428df0"
# Retail outline dispatcher calls (text RVA, original BL, callee RVA).
CALLS = (
    (0x8DAEA4, 0xEBFFF20B, 0x8D76D8),
    (0x8DAEC4, 0xEBFFFAB8, 0x8D99AC),
    (0x8DAED4, 0xEBFFFAF6, 0x8D9AB4),
    (0x94E258, 0xEBFD0AF0, 0x890E20),
    (0x94E474, 0xEBFD0A69, 0x890E20),
    (0x8938DC, 0xEBFFFB7C, 0x8926D4),
    (0x93C780, 0xEBFD57D3, 0x8926D4),
    (0x99D174, 0xEBFBD556, 0x8926D4),
    (0xBC1200, 0xE12FFF37, None),  # NVN SetRenderTargets via BLX r7
    (0x893C00, 0xEB000028, 0x893CA8),
    (0x93C9C8, 0xEBFD5CB6, 0x893CA8),
    (0x99D38C, 0xEBFBDA45, 0x893CA8),
    (0xBBFA68, 0xE12FFF37, None),
    (0xBBD8D8, 0xEB00043A, 0xBBE9C8),
    (0xBBE714, 0xEB0000AB, 0xBBE9C8),
)
LAYOUTS = (0, 0x5000, 0x8000, 0xD000)
def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def nso(path, expected):
    raw = path.read_bytes()
    if raw[:4] != b"NSO0" or raw[64:96] != expected:
        raise ValueError(f"{path}: expected MHGU Switch v1.4.0 {expected.hex()}")
    end = sum(struct.unpack_from("<III", raw, 0x34))
    return raw, (end + 4095) & ~4095


def retail_text(raw):
    offset, _, size = struct.unpack_from("<III", raw, 0x10)
    text = raw[offset:offset + u32(raw, 0x60)]
    if u32(raw, 12) & 1:
        name = ctypes.util.find_library("lz4") or "/opt/homebrew/opt/lz4/lib/liblz4.dylib"
        lib = ctypes.CDLL(name)
        out = ctypes.create_string_buffer(size)
        if lib.LZ4_decompress_safe(text, out, len(text), size) != size:
            raise ValueError("Cannot decode retail .text")
        text = out.raw
    if hashlib.sha256(text).hexdigest() != TEXT_SHA:
        raise ValueError("Retail .text does not match the validated MHGU v1.4.0 build")
    return text


def arm_branch(site, target, link):
    delta = target - site - 8
    if delta % 4 or not -(1 << 25) <= delta < (1 << 25):
        raise ValueError(f"Unreachable ARM branch {site:#x} -> {target:#x}")
    return (0xEB000000 if link else 0xEA000000) | (delta // 4 & 0xffffff)


def package_nso(elf):
    phoff = u32(elf, 28)
    phsize, count = struct.unpack_from("<HH", elf, 42)
    loads = [struct.unpack_from("<8I", elf, phoff + i * phsize) for i in range(count)]
    loads = [s for s in loads if s[0] == 1]
    if [s[6] for s in loads] != [5, 4, 6]:
        raise ValueError("Unexpected module ELF segments")
    header, payload = bytearray(256), bytearray()
    struct.pack_into("<4sIII", header, 0, b"NSO0", 0, 0, 0x38)
    for i, (_, pos, addr, _, size, _, _, _) in enumerate(loads):
        part = elf[pos:pos + size]
        struct.pack_into("<III", header, 16 + i*16, 256 + len(payload), addr, size)
        struct.pack_into("<I", header, 96 + i*4, size)
        header[160+i*32:192+i*32] = hashlib.sha256(part).digest()
        payload.extend(part)
    struct.pack_into("<I", header, 60, loads[-1][5] - loads[-1][4])
    header[64:96] = hashlib.sha256(payload).digest()
    return header + payload


def link(objects, output, script=ROOT / "src/module.ld", extra=()):
    path = os.environ.get("LD_LLD") or shutil.which("ld.lld")
    if path:
        linker = [path]
    else:
        rust = subprocess.check_output(["rustc", "--print", "sysroot"], text=True).strip()
        linker = [str(next((Path(rust)/"lib/rustlib").glob("*/bin/rust-lld"))), "-flavor", "gnu"]
    subprocess.run(linker + ["-T", script, "--no-undefined", *extra, *objects, "-o", output],
                   check=True)


def ips_ranges(data):
    if not data.startswith(b"IPS32") or not data.endswith(b"EEOF"):
        raise ValueError("Expected an IPS32 patch")
    pos = 5
    limit = len(data) - 4
    while pos < limit:
        site, size = struct.unpack_from(">IH", data, pos)
        pos += 6
        if size:
            pos += size
        else:
            size = struct.unpack_from(">H", data, pos)[0]
            pos += 3
        if pos > limit or not size:
            raise ValueError("Malformed IPS32 patch record")
        yield site, site + size


def validate_outline_shader():
    # Check the NVN machine code that Astris translates, not the host GLSL.
    control = (ROOT / "src/shaders/edge.ctrl").read_bytes()
    code = (ROOT / "src/shaders/edge.gpu").read_bytes()
    size = u32(control, 0x6f8)
    start = u32(control, 0x708)
    end = start + size
    if size < 0x70 or end > len(code) or (size - 0x50) % 32:
        raise ValueError("Invalid Maxwell fragment program extent")
    # Each 32-byte bundle starts with scheduling data and has three instructions.
    for bundle in range(start + 0x50, end, 32):
        for word, in struct.iter_unpack("<Q", code[bundle + 8:bundle + 32]):
            if word >> 52 in (0xe27, 0xe32):
                raise ValueError("Astris cannot translate PRET/RET in the outline shader")


def width_cheats(image, module):
    # The patched call and its landing address are identical in all four layouts.
    slot = 8  # bind_targets
    site = CALLS[slot][0]
    landing = module + max(LAYOUTS) + 0x100
    signature = landing + 4 * len(CALLS)
    flags_rvas = set()
    for extent in LAYOUTS:
        offset = signature - module - extent + u32(image, 16)
        if image[offset:offset + 4] != b"WID1":
            raise ValueError("Missing outline cheat signature")
        flags_rvas.add(u32(image, offset + 4) - module - extent)
    rw_start = u32(image, 0x34)
    rw_end = rw_start + u32(image, 0x38) + u32(image, 0x3c)
    if len(flags_rvas) != 1 or not rw_start <= flags_rvas.pop() <= rw_end - 4:
        raise ValueError("Invalid outline cheat configuration address")
    sections = []
    for index, (width, label) in enumerate(((1, "细"), (3, "标准"), (5, "粗"), (8, "特粗"))):
        sections.append(f"[猎人描边线宽 {width}px（{label}）]\n"
                        f"14050000 {site:08X} {arm_branch(site, landing + 4 * slot, True):08X}\n"
                        f"14050000 {signature:08X} {int.from_bytes(b'WID1', 'little'):08X}\n"
                        f"540F0000 {signature + 4:08X}\n"
                        f"010F0000 {index:08X} 00000001\n"
                        "20000000\n20000000\n")
    return "\n".join(sections)


def build(exefs, companions, diagnostics=False):
    validate_outline_shader()
    main, main_extent = nso(exefs / "main", MAIN_ID)
    _, middleware_extent = nso(exefs / "subsdk0", SUBSDK0_ID)
    module = main_extent + middleware_extent
    if module != 0x2715000:
        raise ValueError("Unsupported module layout")
    text = retail_text(main)
    with tempfile.TemporaryDirectory() as tmp:
        obj, elf = (Path(tmp) / name for name in ("probe.o", "probe.elf"))
        subprocess.run(["clang", "--target=armv7a-none-eabi", "-marm", "-c",
                        *(["-DMHGU_TRACE_BINDINGS"] if diagnostics else []),
                        ROOT / "src/probe.S", "-o", obj], check=True, cwd=ROOT)
        objects = [obj]
        for source in ("model_trace", "depth_trace", "mask_capture", "composite"):
            model = Path(tmp) / (source + ".o")
            subprocess.run(["clang", "--target=armv7a-none-eabi", "-marm", "-mfloat-abi=soft",
                        "-Os", "-ffreestanding", "-fPIC", "-fvisibility=hidden", "-Wall",
                        "-Wextra", "-Werror", "-c", ROOT / "src" / (source + ".c"), "-o", model],
                       check=True)
            objects.append(model)
        link(objects, elf)
        image = package_nso(elf.read_bytes())
        patch = bytearray(b"IPS32")
        for index, (site, expected, callee) in enumerate(CALLS):
            if u32(text, site) != expected or (callee is not None and expected != arm_branch(site, callee, True)):
                raise ValueError(f"Unexpected outline call at {site:#x}")
            # The first eight landing slots only report original outline/model activity.
            if index < 8 and not diagnostics:
                continue
            target = module + max(LAYOUTS) + 0x100 + 4 * index
            for extent in LAYOUTS:
                landing = target - module - extent
                branch = u32(image, u32(image, 16) + landing)
                if branch >> 24 != 0xea:
                    raise ValueError(f"Invalid probe landing at {landing:#x}")
                dest = landing + 8 + (((branch & 0xffffff) ^ 0x800000) - 0x800000) * 4
                if extent == LAYOUTS[0]:
                    routine = dest
                elif dest != routine:
                    raise ValueError("Inconsistent probe target")
            patch.extend(struct.pack(">IH", site + 0x100, 4))
            patch.extend(struct.pack("<I", arm_branch(site, target, True)))
        patch.extend(b"EEOF")
        for path in companions:
            with ZipFile(path) as archive:
                for name in archive.namelist():
                    leaf = PurePosixPath(name).name.lower()
                    if leaf == "subsdk3":
                        raise ValueError(f"Module slot collision in {path}")
                    if leaf in ("subsdk1", "subsdk2"):
                        raw = archive.read(name)
                        extent = (sum(struct.unpack_from("<III", raw, 0x34))+4095) & ~4095
                        if extent != {"subsdk1": 0x5000, "subsdk2": 0x8000}[leaf]:
                            raise ValueError(f"Unknown mapping in {path}")
                    if leaf == MAIN_ID.hex() + ".ips":
                        for start, end in ips_ranges(archive.read(name)):
                            if any(start < b and a < end for a,b in ips_ranges(patch)):
                                raise ValueError(f"Overlapping IPS in {path}")
        name = "MHGU-Outline-Trace"
        dist = ROOT / "dist"
        dist.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=dist) as staging:
            out = Path(staging) / (name + ".zip")
            with ZipFile(out, "w") as archive:
                for leaf, data in (("subsdk3", image), (MAIN_ID.hex().upper()+".ips", patch)):
                    archive.writestr(ZipInfo(name+"/exefs/"+leaf), data, compress_type=ZIP_DEFLATED)
                archive.writestr(ZipInfo(name + "/romfs/mhgu-outline/outline-width.txt"),
                                 (ROOT / "config/outline-width.txt").read_bytes())
                bid = MAIN_ID[:8].hex()
                archive.writestr(ZipInfo(f"{name}/cheats/{bid}.txt"), width_cheats(image, module))
            out.replace(dist / out.name)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build the Astris outline-pass trace probe")
    parser.add_argument("exefs", type=Path)
    parser.add_argument("--companion", action="append", default=[], type=Path)
    parser.add_argument("--diagnostics", action="store_true", help="Enable per-model and depth-binding probes")
    args = parser.parse_args()
    build(args.exefs, args.companion, args.diagnostics)
