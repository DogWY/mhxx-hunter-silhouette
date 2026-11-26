"""Build the standalone quest distance patch from a verified MHGU 1.4.0 main."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile
from zipfile import ZipFile, ZipInfo

from build_mod import MAIN_ID, ROOT, link, nso, retail_text

START = 0x3CBC9C
END = 0x3CBD60
NAME = "MHGU-Quest-Camera"


def build(exefs):
    retail_text(nso(exefs / "main", MAIN_ID)[0])
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dist) as tmp:
        obj = Path(tmp) / "camera.o"
        binary = Path(tmp) / "camera.bin"
        subprocess.run(["clang", "-target", "armv7-none-eabi", "-c",
                        ROOT / "camera/quest_camera.S", "-o", obj], check=True)
        link([obj], binary, ROOT / "camera/quest_camera.ld", ["--oformat=binary"])
        code = binary.read_bytes()
        if len(code) != END - START or code[-8:] != b"QCM1\0\0\0\0":
            raise ValueError("Unexpected camera patch layout")
        patch = b"IPS32" + struct.pack(">IH", START + 0x100, len(code)) + code + b"EEOF"
        sections = []
        for index, percent in enumerate((20, 40, 60, 80)):
            sections.append(f"[任务视距 +{percent}%]\n"
                            f"14050000 {START:08X} {int.from_bytes(code[:4], 'little'):08X}\n"
                            f"14050000 {END - 8:08X} {int.from_bytes(b'QCM1', 'little'):08X}\n"
                            f"11050000 {END - 4 + index:08X} 00000000\n"
                            "400F0000 00000000 00000000\n"
                            f"010F0000 {END - 4 + index:08X} 00000001\n"
                            "20000000\n20000000\n20000000\n")
        out = Path(tmp) / (NAME + ".zip")
        with ZipFile(out, "w") as archive:
            archive.writestr(ZipInfo(f"{NAME}/exefs/{MAIN_ID.hex().upper()}.ips"), patch)
            archive.writestr(ZipInfo(f"{NAME}/cheats/{MAIN_ID[:8].hex()}.txt"), "\n".join(sections))
        out.replace(dist / out.name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exefs", type=Path)
    build(parser.parse_args().exefs)
