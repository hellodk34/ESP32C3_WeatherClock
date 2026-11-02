#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
给 TFT_eSPI smooth font (msyhl25.h) 补全缺失的字形。
用系统 Noto Sans CJK Light 渲染缺失字形，按"锚定字符"（字体里已存在的同类型字符）
的度量对齐后合并进现有字体数组。

锚定规则：每个新字指定一个已在字体里的参考字，新字继承该参考字的 advance，
并按参考字在 Noto 里的 ink 高度缩放到参考字在字体里的 ink 高度（保证 baseline 对齐）。

用法:
    python3 gen_font_addchars.py            # dry-run，打印参数不写文件
    python3 gen_font_addchars.py --write    # 备份原文件并写新 msyhl25.h

依赖(在目标 venv 中): Pillow, fontTools
"""
import re
import struct
import sys
import os

from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont
from fontTools.pens.boundsPen import BoundsPen

FONT_H = os.path.join(os.path.dirname(__file__), "..", "font", "msyhl25.h")
TTC = "/usr/share/fonts/opentype/noto/NotoSansCJK-Light.ttc"
TTC_INDEX = 2          # Noto Sans CJK SC
RENDER_SIZE = 100      # 大尺寸渲染再缩小，保证精度

# 要补的字：(字, 锚定字符)。锚定字符必须已存在于现有字体。
#   - 汉字锚定 "海"（advance=25, inkH≈22）
#   - 半角符号 "-" 锚定数字 "0"（advance=14, inkH=18）
ADD_CHARS = [
    ("阵", "海"), ("伴", "海"), ("夹", "海"), ("扬", "海"),
    ("飓", "海"), ("热", "海"), ("卷", "海"), ("知", "海"),
    ("-", "0"), ("~", "0"),
]

# 手动覆盖尺寸：(W, H, dY)，用于连字符等按 ink 缩放会过细的字形。
# advance 仍取锚定字的 advance；dX 由 (adv - W)//2 计算。
OVERRIDE = {
    "-": (9, 3, 10),   # 横杠：9px 长、3px 粗，中线对齐数字中线(baseline上方9px)
    "~": (10, 5, 11),  # 波浪号：10px 长、5px 高，中线对齐数字中线
}


def parse_font(path):
    data = open(path, "rb").read()
    m = re.search(rb'\{([^}]*)\}', data, re.S)
    if not m:
        raise SystemExit("未在字体头文件中找到数组体")
    hexes = re.findall(rb'0x([0-9a-fA-F]{2})', m.group(1))
    b = bytes(int(h, 16) for h in hexes)

    def u32(i):
        return struct.unpack('>I', b[i:i + 4])[0]

    gcount = u32(0)
    version = u32(4)
    size = u32(8)
    ascent = u32(16)
    descent = u32(20)
    print(f"[parse] gCount={gcount} version={version} size={size} ascent={ascent} descent={descent}")

    glyphs = {}            # uni -> [H, W, adv, dY, dX, desc(28B), bmp]
    desc_off = 24
    bmp_base = 24 + 28 * gcount
    bmp_cursor = bmp_base
    for g in range(gcount):
        uni = u32(desc_off)
        H = u32(desc_off + 4)
        W = u32(desc_off + 8)
        adv = u32(desc_off + 12)
        dY = struct.unpack('>i', b[desc_off + 16:desc_off + 20])[0]
        dX = struct.unpack('>i', b[desc_off + 20:desc_off + 24])[0]
        desc = b[desc_off:desc_off + 28]
        desc_off += 28
        blen = H * W
        bmp = b[bmp_cursor:bmp_cursor + blen]
        bmp_cursor += blen
        glyphs[uni] = [H, W, adv, dY, dX, desc, bmp]

    tail = len(b) - bmp_cursor
    print(f"[parse] 位图累计={bmp_cursor - bmp_base}B, 总字节={len(b)}B, 末尾余量={tail}B")
    return {
        "gcount": gcount, "version": version, "size": size,
        "ascent": ascent, "descent": descent,
        "glyphs": glyphs, "raw_len": len(b),
    }


def noto_metrics(chars):
    f = TTFont(TTC, fontNumber=TTC_INDEX)
    cmap = f.getBestCmap()
    gs = f.getGlyphSet()
    out = {}
    for ch in chars:
        g = gs[cmap[ord(ch)]]
        pen = BoundsPen(gs)
        g.draw(pen)
        out[ch] = pen.bounds
    return out


def render_glyph(ch, W, H):
    font = ImageFont.truetype(TTC, RENDER_SIZE, index=TTC_INDEX)
    canvas = Image.new("L", (RENDER_SIZE + 40, RENDER_SIZE + 40), 0)
    d = ImageDraw.Draw(canvas)
    d.text((20, RENDER_SIZE + 5), ch, font=font, fill=255, anchor="ls")
    bbox = canvas.getbbox()
    if bbox is None:
        return bytes(H * W)
    crop = canvas.crop(bbox).resize((W, H), Image.LANCZOS)
    return crop.tobytes()


def build_new_glyphs(font_info):
    glyphs = font_info["glyphs"]

    # 收集所有需要的字（新字 + 锚定字）
    need = [ch for ch, anchor in ADD_CHARS] + [anchor for ch, anchor in ADD_CHARS]
    metrics = noto_metrics(need)

    new = []
    for ch, anchor in ADD_CHARS:
        uni = ord(ch)
        if uni in glyphs:
            print(f"[gen] {ch} U+{uni:04X}: 已存在，跳过")
            continue
        aH, aW, aadv, _, _, _, _ = glyphs[ord(anchor)]   # 锚定字在字体里的参数
        xMin, yMin, xMax, yMax = metrics[ch]              # 新字 Noto ink
        axMin, ayMin, axMax, ayMax = metrics[anchor]      # 锚定字 Noto ink

        if ch in OVERRIDE:
            W, H, dY = OVERRIDE[ch]
            dX = max((aadv - W) // 2, 0)
        else:
            scale = aH / (ayMax - ayMin)                  # 缩放到锚定字的 ink 高度
            W = round((xMax - xMin) * scale)
            H = round((yMax - yMin) * scale)
            dY = round(yMax * scale)                      # 字形顶部到 baseline（与锚定字同 baseline）
            dX = max((aadv - W) // 2, 0)                  # 水平居中

        bmp = render_glyph(ch, W, H)
        desc = struct.pack('>IIIIIII', uni, H, W, aadv, dY, dX, 0)
        new.append((uni, ch, H, W, aadv, dY, dX, desc, bmp))
        print(f"[gen] {ch} U+{uni:04X} (锚定'{anchor}'): H={H} W={W} adv={aadv} dY={dY} dX={dX} bmp={len(bmp)}B")
    return new


def serialize(font_info, new_glyphs):
    glyphs = list(font_info["glyphs"].items())   # [(uni, [H,W,adv,dY,dX,desc,bmp])]
    for uni, ch, H, W, adv, dY, dX, desc, bmp in new_glyphs:
        glyphs.append((uni, [H, W, adv, dY, dX, desc, bmp]))

    glyphs.sort(key=lambda kv: kv[0])            # 按 unicode 升序稳定排序

    gcount = len(glyphs)
    header = struct.pack('>IIIIII', gcount, font_info["version"], font_info["size"],
                         0, font_info["ascent"], font_info["descent"])
    body = bytearray(header)
    for uni, g in glyphs:
        body += g[5]
    for uni, g in glyphs:
        body += g[6]
    return gcount, bytes(body)


def to_c_array(data):
    lines = ["#include <pgmspace.h>",
             "const unsigned char msyhl25[] PROGMEM = {"]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("    " + ", ".join("0x%02x" % b for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    write = "--write" in sys.argv
    font_info = parse_font(FONT_H)
    new_glyphs = build_new_glyphs(font_info)
    gcount, data = serialize(font_info, new_glyphs)

    print(f"[merge] 新 gCount={gcount}, 新字体字节={len(data)}B (原 {font_info['raw_len']}B)")
    added = [f"{ch}" for uni, ch, *_ in new_glyphs]
    print(f"[merge] 本次新增: {''.join(added) or '无'}")

    if not write:
        print("\n[dry-run] 未写文件。确认参数无误后加 --write 再跑。")
        return

    bak = FONT_H + ".bak"
    if not os.path.exists(bak):
        os.replace(FONT_H, bak)
        print(f"[write] 已备份原文件 -> {bak}")
    else:
        print(f"[write] 备份已存在({bak})，跳过备份")
    with open(FONT_H, "w", encoding="utf-8", newline="\n") as f:
        f.write(to_c_array(data))
    print(f"[write] 已写入 {FONT_H} ({len(data)}B)")


if __name__ == "__main__":
    main()
