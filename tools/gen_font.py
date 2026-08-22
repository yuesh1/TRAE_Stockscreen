#!/usr/bin/env python3
"""从 macOS 系统 CJK 字体提取 config.h 中 CN_CHARSET 的字形，
生成 include/stock_font.h（16×16 点阵 C 数组）。

用法：改了 config.h 里的 CN_CHARSET 或自选股后运行：
    python3 tools/gen_font.py
"""
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(ROOT, "include", "config.h")
OUT = os.path.join(ROOT, "include", "stock_font.h")

FONT_CANDIDATES = [
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
]


def read_charset():
    text = open(CONFIG, encoding="utf-8").read()
    m = re.search(r'#define\s+CN_CHARSET\s+"([^"]*)"', text)
    if not m:
        sys.exit("在 include/config.h 中未找到 CN_CHARSET 定义")
    return m.group(1)


def find_font():
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return path
    sys.exit("未找到系统 CJK 字体（尝试了: %s）" % ", ".join(FONT_CANDIDATES))


def main():
    charset = read_charset()
    # 只保留非 ASCII 字符，去重并排序（保证生成的数组稳定）
    chars = sorted({ch for ch in charset if ord(ch) > 0x7F})
    if not chars:
        sys.exit("CN_CHARSET 中没有中文字符")

    font = ImageFont.truetype(find_font(), 16)
    glyphs = []
    for ch in chars:
        img = Image.new("L", (16, 16), 0)
        d = ImageDraw.Draw(img)
        d.text((0, 0), ch, font=font, fill=255, anchor="la")
        rows = []
        for y in range(16):
            row = 0
            for x in range(16):
                if img.getpixel((x, y)) > 127:
                    row |= 1 << (15 - x)
            rows.append(row)
        glyphs.append((ch, rows))

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// 由 tools/gen_font.py 自动生成，请勿手改\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("typedef struct { const char* utf8; uint16_t rows[16]; } CnGlyph;\n\n")
        f.write("static const CnGlyph CN_FONT[] = {\n")
        for ch, rows in glyphs:
            esc = "".join("\\x%02x" % b for b in ch.encode("utf-8"))
            f.write('  {"%s", {' % esc)
            f.write(",".join("0x%04X" % r for r in rows))
            f.write("}},\n")
        f.write("};\n")
        f.write("#define CN_FONT_COUNT %d\n" % len(glyphs))

    print("生成 %d 个字形 -> %s" % (len(glyphs), OUT))


if __name__ == "__main__":
    main()
