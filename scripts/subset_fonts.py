#!/usr/bin/env python3
"""Subset the Inter release to the glyphs the overlay can render.

Dropping unused glyphs, OpenType layout features, and hinting (stb_truetype
ignores hinting bytecode) takes each static cut from ~410 KB to ~22 KB.

Subsetting makes these Modified Versions under OFL 1.1, so the Reserved Font
Name must go: they are renamed to "BB Tracker Sans". The license and copyright
notice stay untouched.

Usage: subset_fonts.py <inter-release-root> <output-dir>
"""
import sys
import tempfile
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont

UNICODES = ",".join([
    "U+0020-007E",    # ASCII
    "U+00A0-00FF",    # Latin-1 supplement (accents such as "Río")
    "U+0100-017F",    # Latin Extended-A
    "U+2013-2014",    # en/em dash
    "U+2018-201D",    # curly quotes
    "U+2022",         # bullet
    "U+2026",         # ellipsis
    "U+2192",         # right arrow
    "U+25B2", "U+25CF", "U+2605",  # triangle, circle, star
    "U+2713", "U+2717",            # ✓ ✗ state glyphs
])
FAMILY = "BB Tracker Sans"
TABULAR = {
    "0": "zero.tf", "1": "one.tf", "2": "two.tf", "3": "three.tf", "4": "four.tf",
    "5": "five.tf", "6": "six.tf", "7": "seven.tf", "8": "eight.tf", "9": "nine.tf",
}
STYLES = {
    "Inter-Regular.ttf": "Regular",
    "Inter-SemiBold.ttf": "SemiBold",
    "Inter-Bold.ttf": "Bold",
}


def rename(font, style):
    name = font["name"]
    for record in list(name.names):
        if record.nameID in (1, 2, 3, 4, 6):
            name.names.remove(record)

    # SemiBold is not RIBBI, so its family/style split matches convention:
    # family "BB Tracker Sans SemiBold" + style "Regular".
    if style in ("Regular", "Bold"):
        family, subfamily = FAMILY, style
    else:
        family, subfamily = f"{FAMILY} {style}", "Regular"
    full = f"{FAMILY} {style}"
    postscript = f"BBTrackerSans-{style}"
    for name_id, value in ((1, family), (2, subfamily), (3, f"4.001;bbtracker;{postscript}"),
                           (4, full), (6, postscript),
                           (16, FAMILY), (17, style)):
        name.setName(value, name_id, 3, 1, 0x409)


def bake_tabular_digits(font):
    """Point the ASCII digits at Inter's tabular variants.

    ImGui cannot apply the tnum OpenType feature, so remap the cmap instead;
    the subsetter then keeps the .tf glyphs because the cmap references them.
    """
    glyphs = set(font.getGlyphOrder())
    for table in font["cmap"].tables:
        if not table.isUnicode():
            continue
        for digit, name in TABULAR.items():
            if ord(digit) in table.cmap and name in glyphs:
                table.cmap[ord(digit)] = name


def main():
    src_root, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    for filename, style in STYLES.items():
        source = src_root / "extras" / "ttf" / filename
        target = out_dir / filename
        with tempfile.TemporaryDirectory() as tmp:
            remapped = Path(tmp) / filename
            font = TTFont(source)
            bake_tabular_digits(font)
            font.save(remapped)
            subset.main([
                str(remapped),
                f"--unicodes={UNICODES}",
                "--layout-features=",
                "--name-IDs=0,1,2,3,4,5,6,13,14",
                "--no-hinting",
                f"--output-file={target}",
            ])
        font = TTFont(target)
        rename(font, style)
        font.save(target)
        print(f"{target}: {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
