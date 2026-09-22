#!/usr/bin/env python3
"""
make_icon.py — يولّد أيقونة GameLauncher (ملف .ico متعدد الأحجام) بلا أي مكتبة عدا Pillow.
    pip install pillow
    python tools/make_icon.py                # يكتب Common/app_icon.ico + معاينة PNG
    python tools/make_icon.py --variant B    # نمط آخر (A أو B أو C)

الفكرة: دائرة ألعاب (Radial) — أيقونات ملوّنة تدور حول مركز مضيء، داخل مربع داكن بحواف ناعمة.
كل حجم يُرسم على حدة (تفاصيل أقل في 16/24/32 بكسل) لتبقى واضحة في شريط المهام والتراي.
"""
import argparse, math, os, struct, io
from PIL import Image, ImageDraw, ImageFilter, ImageChops

PALETTE = [(139, 92, 246), (56, 189, 248), (52, 211, 153), (251, 191, 36), (251, 113, 133), (192, 132, 252)]

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

def vgrad(size, top, bottom):
    g = Image.new("RGB", (1, size))
    for y in range(size): g.putpixel((0, y), lerp(top, bottom, y / max(1, size - 1)))
    return g.resize((size, size))

def circle_mask(S, cx, cy, r):
    m = Image.new("L", (S, S), 0)
    ImageDraw.Draw(m).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    return m

def glow(layer, radius, strength=1.0):
    b = layer.filter(ImageFilter.GaussianBlur(radius))
    if strength != 1.0:
        a = b.getchannel("A").point(lambda v: min(255, int(v * strength)))
        b.putalpha(a)
    return b

def squircle_mask(S, r):
    m = Image.new("L", (S, S), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, S - 1, S - 1), radius=r, fill=255)
    return m

def render_b(size):
    """النمط B: حلقة سميكة من أربعة أقواس ملوّنة حول مركز مضيء — يبقى واضحاً حتى 16 بكسل."""
    SS = 4 if size >= 48 else 8
    S = size * SS
    tiny = size <= 16
    bg = vgrad(S, (30, 22, 74), (9, 7, 24)).convert("RGBA")
    hl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(hl).ellipse((-S * 0.2, -S * 0.4, S * 0.9, S * 0.55), fill=(120, 90, 250, 100))
    bg = Image.alpha_composite(bg, glow(hl, S * 0.12))
    cx = cy = S / 2
    R = S * 0.285
    width = int(S * (0.125 if not tiny else 0.15))
    cols = [(139, 92, 246), (56, 189, 248), (251, 191, 36), (251, 113, 133)]
    ring = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(ring)
    for k, col in enumerate(cols):
        a0 = -90 + 90 * k + 9
        d.arc((cx - R, cy - R, cx + R, cy + R), a0, a0 + 72, fill=col + (255,), width=width)
        # نهايات دائرية للقوس
        for ang in (a0, a0 + 72):
            ex = cx + (R - width / 2 + 0.5) * math.cos(math.radians(ang)); ey = cy + (R - width / 2 + 0.5) * math.sin(math.radians(ang))
            d.ellipse((ex - width / 2, ey - width / 2, ex + width / 2, ey + width / 2), fill=col + (255,))
    art = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    if not tiny: art = Image.alpha_composite(art, glow(ring, S * 0.03, 1.5))
    art = Image.alpha_composite(art, ring)
    hub_r = S * (0.135 if not tiny else 0.14)
    g = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(g).ellipse((cx - hub_r, cy - hub_r, cx + hub_r, cy + hub_r), fill=(167, 139, 250, 230))
    if not tiny: art = Image.alpha_composite(art, glow(g, S * 0.05, 1.6))
    m = circle_mask(S, cx, cy, hub_r)
    hub = Image.new("RGBA", (S, S), (0, 0, 0, 0)); hub.paste(vgrad(S, (221, 214, 254), (109, 40, 217)).convert("RGBA"), (0, 0), m)
    art = Image.alpha_composite(art, hub)
    out = Image.alpha_composite(bg, art)
    final = Image.new("RGBA", (S, S), (0, 0, 0, 0)); final.paste(out, (0, 0), squircle_mask(S, int(S * 0.225)))
    return final.resize((size, size), Image.LANCZOS)

def render(size, variant="A"):
    if variant == "B": return render_b(size)
    SS = 4 if size >= 48 else 8          # تنعيم أعلى للأحجام الصغيرة
    S = size * SS
    small = size <= 32
    tiny = size <= 16
    # ---------- الخلفية ----------
    bg = vgrad(S, (34, 24, 84), (10, 8, 26)).convert("RGBA")
    hl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(hl).ellipse((-S * 0.25, -S * 0.35, S * 0.85, S * 0.6), fill=(110, 80, 240, 110))
    bg = Image.alpha_composite(bg, glow(hl, S * 0.12))
    if not tiny:
        ring_border = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        d = ImageDraw.Draw(ring_border)
        d.rounded_rectangle((SS, SS, S - SS - 1, S - SS - 1), radius=int(S * 0.225), outline=(255, 255, 255, 34), width=max(1, SS))
        bg = Image.alpha_composite(bg, ring_border)
    cx = cy = S / 2
    R = S * (0.305 if size >= 48 else 0.285)         # نصف قطر المدار
    stroke = S * (0.036 if not small else 0.05)
    n_nodes = 6 if size >= 48 else 4
    node_r = S * (0.108 if size >= 48 else (0.135 if size >= 24 else 0.155))
    start_ang = -math.pi / 2 if size >= 48 else -math.pi / 4      # صغير: أربع عقد بزوايا المربع
    art = Image.new("RGBA", (S, S), (0, 0, 0, 0))

    # ---------- الحلقة (تدرّج لوني على محيطها) ----------
    ringL = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    dr = ImageDraw.Draw(ringL)
    steps = 240
    stops = [(139, 92, 246), (217, 70, 239), (251, 146, 60), (139, 92, 246)]
    for i in range(steps):
        t = i / steps
        seg = min(len(stops) - 2, int(t * (len(stops) - 1)))
        col = lerp(stops[seg], stops[seg + 1], t * (len(stops) - 1) - seg)
        a0 = 360 * i / steps - 90; a1 = 360 * (i + 1.6) / steps - 90
        dr.arc((cx - R, cy - R, cx + R, cy + R), a0, a1, fill=col + (255,), width=int(stroke))
    if not tiny:
        art = Image.alpha_composite(art, glow(ringL, S * 0.03, 1.4))
    if size > 24:
        art = Image.alpha_composite(art, ringL)      # الحلقة تختفي في 24 و16 (تصير ضجيجاً)

    # ---------- العقد (أيقونات الألعاب) ----------
    for k in range(n_nodes):
        ang = start_ang + 2 * math.pi * k / n_nodes
        nx, ny = cx + R * math.cos(ang), cy + R * math.sin(ang)
        base = PALETTE[k % len(PALETTE)]
        # ظل/توهّج
        g = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(g).ellipse((nx - node_r, ny - node_r, nx + node_r, ny + node_r), fill=base + (200,))
        if not tiny: art = Image.alpha_composite(art, glow(g, S * 0.035, 1.2))
        # حدّ داكن يفصل العقدة عن الحلقة
        sep = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(sep).ellipse((nx - node_r - S * 0.014, ny - node_r - S * 0.014, nx + node_r + S * 0.014, ny + node_r + S * 0.014), fill=(12, 9, 30, 255))
        art = Image.alpha_composite(art, sep)
        # جسم العقدة بتدرّج
        m = circle_mask(S, nx, ny, node_r)
        grad = vgrad(S, lerp(base, (255, 255, 255), 0.20), lerp(base, (0, 0, 0), 0.30)).convert("RGBA")
        node = Image.new("RGBA", (S, S), (0, 0, 0, 0)); node.paste(grad, (0, 0), m)
        art = Image.alpha_composite(art, node)
        if not small:  # لمعة صغيرة
            sh = Image.new("RGBA", (S, S), (0, 0, 0, 0))
            ImageDraw.Draw(sh).ellipse((nx - node_r * 0.55, ny - node_r * 0.75, nx + node_r * 0.05, ny - node_r * 0.2), fill=(255, 255, 255, 90))
            art = Image.alpha_composite(art, sh.filter(ImageFilter.GaussianBlur(S * 0.004)))

    # ---------- المركز (Hub) ----------
    hub_r = S * (0.152 if not tiny else 0.17)
    g = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(g).ellipse((cx - hub_r, cy - hub_r, cx + hub_r, cy + hub_r), fill=(167, 139, 250, 230))
    if not tiny: art = Image.alpha_composite(art, glow(g, S * 0.05, 1.6))
    m = circle_mask(S, cx, cy, hub_r)
    grad = vgrad(S, (196, 181, 253), (91, 33, 182)).convert("RGBA")
    hub = Image.new("RGBA", (S, S), (0, 0, 0, 0)); hub.paste(grad, (0, 0), m)
    art = Image.alpha_composite(art, hub)
    if not small:  # مثلث التشغيل
        tr = hub_r * 0.5
        px = cx + tr * 0.14
        pts = [(px - tr * 0.62, cy - tr * 0.9), (px - tr * 0.62, cy + tr * 0.9), (px + tr * 0.95, cy)]
        tri = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(tri).polygon(pts, fill=(255, 255, 255, 245))
        art = Image.alpha_composite(art, tri.filter(ImageFilter.GaussianBlur(S * 0.006)))
    elif size >= 24:
        dot = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(dot).ellipse((cx - hub_r * 0.38, cy - hub_r * 0.38, cx + hub_r * 0.38, cy + hub_r * 0.38), fill=(255, 255, 255, 240))
        art = Image.alpha_composite(art, dot)

    out = Image.alpha_composite(bg, art)
    mask = squircle_mask(S, int(S * 0.225))
    final = Image.new("RGBA", (S, S), (0, 0, 0, 0)); final.paste(out, (0, 0), mask)
    return final.resize((size, size), Image.LANCZOS)

def write_ico(path, images):
    """يكتب ICO يدوياً: 256 بصيغة PNG وبقية الأحجام DIB (متوافق مع rc.exe وكل إصدارات ويندوز)."""
    blobs = []
    for im in images:
        w, h = im.size
        if w >= 256:
            b = io.BytesIO(); im.save(b, "PNG"); blobs.append((w, h, b.getvalue()))
        else:
            px = im.convert("RGBA")
            rows = []
            for y in range(h - 1, -1, -1):
                row = bytearray()
                for x in range(w):
                    r, g, bl, a = px.getpixel((x, y)); row += bytes((bl, g, r, a))
                rows.append(bytes(row))
            mask_row = ((w + 31) // 32) * 4
            mask = bytes(mask_row * h)
            header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, len(b"".join(rows)) + len(mask), 0, 0, 0, 0)
            blobs.append((w, h, header + b"".join(rows) + mask))
    out = bytearray(struct.pack("<HHH", 0, 1, len(blobs)))
    offset = 6 + 16 * len(blobs)
    for w, h, data in blobs:
        out += struct.pack("<BBBBHHII", 0 if w >= 256 else w, 0 if h >= 256 else h, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    for _, _, data in blobs: out += data
    with open(path, "wb") as f: f.write(out)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", default="A")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "Common", "app_icon.ico"))
    a = ap.parse_args()
    sizes = [256, 128, 64, 48, 32, 24, 16]
    imgs = [render(s, a.variant) for s in sizes]
    write_ico(a.out, imgs)
    imgs[0].save(os.path.splitext(a.out)[0] + "_preview.png")
    print("wrote", os.path.abspath(a.out))
