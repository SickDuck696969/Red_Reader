"""Verify the actual Switch linker map, not the desktop SDL dependencies.

MuPDF 1.17's bundled HarfBuzz allocates through a context installed by
fz_hb_lock. SDL_ttf calls HarfBuzz independently, including at TTF_OpenFont,
so linking that private copy into SDL_ttf dereferences a null context.
"""
import re
import sys
from pathlib import Path

def validate(text):
    if re.search(r"libmupdf-third\.a\(hb[-_]", text):
        raise ValueError("MuPDF's private HarfBuzz objects were linked; SDL_ttf startup is unsafe")
    if "libharfbuzz.a(" not in text:
        raise ValueError("The portlibs HarfBuzz implementation is missing")
    for symbol in ("hb_font_create", "hb_buffer_create", "hb_shape"):
        if not re.search(r"0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.M):
            raise ValueError("Missing resolved HarfBuzz symbol: " + symbol)

if __name__ == "__main__":
    try:
        validate(Path(sys.argv[1] if len(sys.argv)>1 else "build/Red_Reader.map").read_text())
    except (OSError, ValueError) as error:
        print("FAIL:", error, file=sys.stderr)
        sys.exit(1)
    print("PASS: Switch font shaping uses portlibs HarfBuzz, not MuPDF's private allocator")
