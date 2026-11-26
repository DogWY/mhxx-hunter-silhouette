"""Report depth target matches from the latest MHGU run in one Astris log."""
import argparse
from collections import defaultdict
from pathlib import Path


def report(path):
    attachments = defaultdict(set)
    hunter, monster = defaultdict(set), defaultdict(set)
    active = False
    with path.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if "Application Loaded:" in line:
                active = "[0100770008dd8000]" in line.lower()
                attachments.clear()
                hunter.clear()
                monster.clear()
            marker = "OutputDebugString: MHGU depth "
            if not active or marker not in line:
                continue
            parts = line.split(marker, 1)[1].split()
            if len(parts) not in (4, 8):
                continue
            try:
                stage, render_pass, texture, view, *shape = (int(p, 16) for p in parts)
            except ValueError:
                continue
            if not texture:
                continue
            key = (texture, view)
            if stage == 0 and len(shape) == 4:
                attachments[key].add(tuple(shape))
            elif stage == 1:
                hunter[key].add(render_pass)
            elif stage == 2:
                monster[key].add(render_pass)
    if not active:
        return "Latest session is not MHGU."
    rows = []
    for key in sorted(attachments.keys() | hunter.keys() | monster.keys()):
        metadata = ", ".join(f"{w}x{h} format={fmt:#x} samples={samples}"
                             for w, h, fmt, samples in sorted(attachments[key]))
        rows.append(f"texture={key[0]:#x} view={key[1]:#x} {metadata} "
                    f"hunter={sorted(hunter[key])} monster={sorted(monster[key])} "
                    f"shared_passes={sorted(hunter[key] & monster[key])}")
    return "\n".join(rows) or "No depth bindings captured yet."


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    print(report(parser.parse_args().log))
