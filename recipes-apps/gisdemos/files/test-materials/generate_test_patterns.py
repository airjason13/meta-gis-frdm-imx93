import os
import math
import string
import subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUTPUT_DIR = "/home/venom/Pictures/Test-materials"
if not os.path.exists(OUTPUT_DIR):
    OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))
FONT_PATH = "/usr/share/fonts/truetype/arphic/uming.ttc"
SANS_BOLD_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

def get_font(size):
    return ImageFont.truetype(FONT_PATH, size, index=2)

W, H = 1920, 1080

def draw_siemens_star(draw, cx, cy, radius, num_spokes=36):
    angle_step = 2 * math.pi / num_spokes
    for i in range(num_spokes):
        if i % 2 == 0:
            a1 = i * angle_step
            a2 = (i + 1) * angle_step
            p1 = (cx, cy)
            p2 = (cx + radius * math.cos(a1), cy + radius * math.sin(a1))
            p3 = (cx + radius * math.cos(a2), cy + radius * math.sin(a2))
            draw.polygon([p1, p2, p3], fill=(255, 255, 255))
        else:
            a1 = i * angle_step
            a2 = (i + 1) * angle_step
            p1 = (cx, cy)
            p2 = (cx + radius * math.cos(a1), cy + radius * math.sin(a1))
            p3 = (cx + radius * math.cos(a2), cy + radius * math.sin(a2))
            draw.polygon([p1, p2, p3], fill=(0, 0, 0))

def draw_line_pairs(draw, x, y, size=80, orientation='both'):
    # size x size alternating 1px lines
    box = [(x, y), (x + size, y + size)]
    draw.rectangle(box, fill=(0, 0, 0), outline=(128, 128, 128))
    half = size // 2
    if orientation in ['both', 'h']:
        # horizontal lines in top half
        for ly in range(y, y + half, 2):
            draw.line([(x, ly), (x + size - 1, ly)], fill=(255, 255, 255), width=1)
    if orientation in ['both', 'v']:
        # vertical lines in bottom half
        for lx in range(x, x + size, 2):
            draw.line([(lx, y + half), (lx, y + size - 1)], fill=(255, 255, 255), width=1)

# ==========================================
# 01. Geometry & Focus Grid
# ==========================================
def gen_01_geometry_focus():
    img = Image.new('RGB', (W, H), (15, 15, 15))
    draw = ImageDraw.Draw(img)
    
    # 60px grid (32 columns, 18 rows)
    grid_sz = 60
    for x in range(0, W + 1, grid_sz):
        col = (70, 70, 70) if x % (grid_sz * 4) != 0 else (140, 140, 140)
        draw.line([(x, 0), (x, H)], fill=col, width=1)
    for y in range(0, H + 1, grid_sz):
        col = (70, 70, 70) if y % (grid_sz * 4) != 0 else (140, 140, 140)
        draw.line([(0, y), (W, y)], fill=col, width=1)
        
    # Safe area margins (95% and 90%)
    draw.rectangle([(W*0.025, H*0.025), (W*0.975, H*0.975)], outline=(0, 200, 255), width=1)
    draw.rectangle([(W*0.05, H*0.05), (W*0.95, H*0.95)], outline=(255, 180, 0), width=1)
    
    # Outer 1px frame
    draw.rectangle([(0, 0), (W - 1, H - 1)], outline=(255, 255, 255), width=1)
    
    # Center Crosshair with ticks
    cx, cy = W // 2, H // 2
    draw.line([(0, cy), (W, cy)], fill=(255, 255, 0), width=1)
    draw.line([(cx, 0), (cx, H)], fill=(255, 255, 0), width=1)
    for x in range(0, W, 10):
        draw.line([(x, cy - 3), (x, cy + 3)], fill=(255, 255, 0), width=1)
    for y in range(0, H, 10):
        draw.line([(cx - 3, y), (cx + 3, y)], fill=(255, 255, 0), width=1)

    # Concentric circles in center
    for r in [40, 80, 120, 180, 240, 300, 360, 420, 480]:
        draw.ellipse([(cx - r, cy - r), (cx + r, cy + r)], outline=(255, 255, 255), width=1)

    # Siemens Stars at Center and 4 Corners
    star_positions = [
        (cx, cy, 60),
        (120, 120, 50),
        (W - 120, 120, 50),
        (120, H - 120, 50),
        (W - 120, H - 120, 50),
    ]
    for sx, sy, sr in star_positions:
        draw_siemens_star(draw, sx, sy, sr, num_spokes=32)
        draw.ellipse([(sx - sr, sy - sr), (sx + sr, sy + sr)], outline=(255, 0, 0), width=2)
        
    # 1-pixel alternating line pairs at corners and edge centers
    lp_coords = [
        (200, 80), (W - 280, 80), (200, H - 160), (W - 280, H - 160),
        (cx - 40, 80), (cx - 40, H - 160), (80, cy - 40), (W - 160, cy - 40)
    ]
    for lx, ly in lp_coords:
        draw_line_pairs(draw, lx, ly, size=80)

    # Labels
    font = get_font(22)
    font_s = get_font(16)
    draw.text((30, 30), "01: 幾何失真、光學對焦與解析度測試 (1920x1080 1:1 Pixel)", fill=(255, 255, 255), font=font)
    draw.text((30, 60), "檢視項目：中心與四角聚焦銳利度、梯形/桶狀幾何畸變、1px線對清晰度、Siemens Star散焦", fill=(200, 200, 200), font=font_s)
    
    img.save(os.path.join(OUTPUT_DIR, "01_Geometry_Focus_Grid_1080p.png"))
    print("Generated 01_Geometry_Focus_Grid_1080p.png")

# ==========================================
# 02. ISO 12233 Resolution Chart 1080p
# ==========================================
def gen_02_iso12233():
    # Load the high-res render from ghostscript
    iso_src = "/tmp/iso_page.png"
    if os.path.exists(iso_src):
        src_im = Image.open(iso_src)
        # Target 1920x1080 with black bars / fit
        sw, sh = src_im.size
        scale = min(1920 / sw, 1080 / sh)
        nw, nh = int(sw * scale), int(sh * scale)
        resized = src_im.resize((nw, nh), Image.LANCZOS)
        
        bg = Image.new('RGB', (W, H), (0, 0, 0))
        ox, oy = (W - nw) // 2, (H - nh) // 2
        bg.paste(resized, (ox, oy))
        draw = ImageDraw.Draw(bg)
        font = get_font(20)
        draw.text((20, 20), "02: ISO 12233 標準光學解析度測試圖 (MTF / 空間頻率響應)", fill=(255, 255, 255), font=font)
        bg.save(os.path.join(OUTPUT_DIR, "02_ISO12233_Resolution_Chart_1080p.png"))
        print("Generated 02_ISO12233_Resolution_Chart_1080p.png")

# ==========================================
# 03. ANSI Contrast Checkerboard
# ==========================================
def gen_03_ansi_contrast():
    img = Image.new('RGB', (W, H), (0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    rows, cols = 4, 4
    cw, ch = W // cols, H // rows # 480 x 270
    
    for r in range(rows):
        for c in range(cols):
            x0, y0 = c * cw, r * ch
            x1, y1 = x0 + cw, y0 + ch
            # Alternating pattern
            is_white = (r + c) % 2 == 0
            color = (255, 255, 255) if is_white else (0, 0, 0)
            draw.rectangle([(x0, y0), (x1, y1)], fill=color)
            
            # Subtle center measurement mark
            mx, my = x0 + cw // 2, y0 + ch // 2
            mark_col = (128, 128, 128)
            draw.line([(mx - 15, my), (mx + 15, my)], fill=mark_col, width=1)
            draw.line([(mx, my - 15), (mx, my + 15)], fill=mark_col, width=1)
            
            # Label zone number
            zone_idx = r * cols + c + 1
            text_col = (128, 128, 128)
            font_zone = get_font(16)
            draw.text((x0 + 15, y0 + 15), f"P{zone_idx}", fill=text_col, font=font_zone)
            
    # Overlay banner at bottom center
    bw, bh = 600, 70
    bx, by = (W - bw) // 2, H - bh - 20
    draw.rectangle([(bx, by), (bx + bw, by + bh)], fill=(20, 20, 20), outline=(200, 200, 200))
    font = get_font(18)
    font_s = get_font(14)
    draw.text((bx + 20, by + 12), "03: ANSI 16分區棋盤格對比度測試 (ANSI Contrast Ratio)", fill=(255, 255, 255), font=font)
    draw.text((bx + 20, by + 38), "公式：CR = (8個白區平均照度) / (8個黑區平均照度) | 評估內部光學雜散光/眩光", fill=(200, 200, 200), font=font_s)
    
    img.save(os.path.join(OUTPUT_DIR, "03_ANSI_Contrast_Checkerboard_1080p.png"))
    print("Generated 03_ANSI_Contrast_Checkerboard_1080p.png")

# ==========================================
# 04. Grayscale 32-Step & Continuous Gamma
# ==========================================
def gen_04_grayscale_gamma():
    img = Image.new('RGB', (W, H), (15, 15, 15))
    draw = ImageDraw.Draw(img)
    
    font_t = get_font(22)
    font = get_font(15)
    font_xs = get_font(11)
    
    draw.text((40, 30), "04: 32階線性灰階階梯與連續 Gamma 漸層 (8-bit / 10-bit 動態範圍評估)", fill=(255, 255, 255), font=font_t)
    draw.text((40, 60), "檢視項目：灰階平滑度、色帶效應(Banding)、暗部與亮部階調分明度、色彩偏差(Color Tinting)", fill=(180, 180, 180), font=font)
    
    # 1. 32-step linear grayscale bar
    y1 = 110
    h1 = 180
    num_steps = 32
    step_w = (W - 80) // num_steps
    for i in range(num_steps):
        v = int(round(i * 255.0 / (num_steps - 1)))
        x0 = 40 + i * step_w
        x1 = x0 + step_w
        draw.rectangle([(x0, y1), (x1, y1 + h1)], fill=(v, v, v))
        # Step text
        txt_col = (0, 0, 0) if v > 128 else (255, 255, 255)
        draw.text((x0 + 2, y1 + h1 - 25), f"{v}", fill=txt_col, font=font_xs)
        draw.text((x0 + 2, y1 + 10), f"S{i+1}", fill=txt_col, font=font_xs)
        
    draw.text((40, y1 - 25), "【32階線性灰階階梯 (0 ~ 255)】", fill=(220, 220, 220), font=font)

    # 2. 16-step IRE scale (0% to 100%)
    y2 = 340
    h2 = 200
    ire_steps = 16
    ire_w = (W - 80) // ire_steps
    for i in range(ire_steps):
        v = int(round(i * 255.0 / (ire_steps - 1)))
        ire = round(i * 100.0 / (ire_steps - 1), 1)
        x0 = 40 + i * ire_w
        x1 = x0 + ire_w
        draw.rectangle([(x0, y2), (x1, y2 + h2)], fill=(v, v, v))
        txt_col = (0, 0, 0) if v > 128 else (255, 255, 255)
        draw.text((x0 + 10, y2 + 20), f"{ire}%", fill=txt_col, font=font)
        draw.text((x0 + 10, y2 + 50), f"RGB {v}", fill=txt_col, font=font)
        
    draw.text((40, y2 - 25), "【標準 16分段 IRE 刻度】", fill=(220, 220, 220), font=font)

    # 3. Continuous 256-level gradient ramp
    y3 = 600
    h3 = 180
    grad_w = W - 80
    for x in range(grad_w):
        val = int(x * 255.0 / (grad_w - 1))
        draw.line([(40 + x, y3), (40 + x, y3 + h3)], fill=(val, val, val), width=1)
        
    draw.text((40, y3 - 25), "【連續 256階平滑漸層 (檢測色階斷層 Banding / Dithering 雜訊)】", fill=(220, 220, 220), font=font)

    # 4. Gamma 2.2 vs 2.4 reference curves
    y4 = 840
    h4 = 160
    draw.text((40, y4 - 25), "【Gamma 2.2 / 2.4 曲線響應對照區】", fill=(220, 220, 220), font=font)
    for x in range(grad_w):
        norm_x = x / float(grad_w - 1)
        v_g22 = int((norm_x ** 2.2) * 255.0)
        v_g24 = int((norm_x ** 2.4) * 255.0)
        draw.line([(40 + x, y4), (40 + x, y4 + h4 // 2 - 5)], fill=(v_g22, v_g22, v_g22), width=1)
        draw.line([(40 + x, y4 + h4 // 2 + 5), (40 + x, y4 + h4)], fill=(v_g24, v_g24, v_g24), width=1)
    draw.text((50, y4 + 15), "Gamma 2.2 (標準多媒體 / sRGB)", fill=(255, 50, 50), font=font)
    draw.text((50, y4 + h4 // 2 + 25), "Gamma 2.4 (標準家庭劇院 / Rec.709 暗室標準)", fill=(50, 150, 255), font=font)

    img.save(os.path.join(OUTPUT_DIR, "04_Grayscale_32Step_Gamma_1080p.png"))
    print("Generated 04_Grayscale_32Step_Gamma_1080p.png")

# ==========================================
# 05. PLUGE Black & White Level Clipping
# ==========================================
def gen_05_pluge_clipping():
    img = Image.new('RGB', (W, H), (20, 20, 20))
    draw = ImageDraw.Draw(img)
    
    font_t = get_font(22)
    font = get_font(16)
    font_s = get_font(13)
    
    draw.text((40, 30), "05: PLUGE 暗部黑階與高光白階裁切測試 (Black & White Level Calibration)", fill=(255, 255, 255), font=font_t)
    draw.text((40, 60), "校正指南：調整『亮度(Brightness)』使黑階條恰好隱約可見(不死黑)；調整『對比度(Contrast)』使白階條清楚分界(不過曝)", fill=(200, 200, 200), font=font)

    # Left Box: Near-Black Field
    box_w = (W - 120) // 2
    box_h = 750
    bx1, by1 = 40, 110
    draw.rectangle([(bx1, by1), (bx1 + box_w, by1 + box_h)], fill=(0, 0, 0), outline=(80, 80, 80))
    
    draw.text((bx1 + 20, by1 + 20), "【暗部黑階校準 (Black Level - 調整顯示器亮度)】", fill=(255, 255, 255), font=font)
    draw.text((bx1 + 20, by1 + 50), "背景基準：RGB (0, 0, 0) 純黑", fill=(160, 160, 160), font=font_s)
    
    black_patches = [
        (1, "RGB 1 (0.4%)"),
        (2, "RGB 2 (0.8%)"),
        (3, "RGB 3 (1.2%)"),
        (4, "RGB 4 (1.6%)"),
        (6, "RGB 6 (2.4%)"),
        (8, "RGB 8 (3.1%)"),
        (12, "RGB 12 (4.7%)"),
        (16, "RGB 16 (6.3% Video Black)"),
        (20, "RGB 20 (7.8%)"),
        (24, "RGB 24 (9.4%)"),
    ]
    
    pw = (box_w - 40) // len(black_patches)
    for i, (val, label) in enumerate(black_patches):
        px0 = bx1 + 20 + i * pw
        px1 = px0 + pw - 4
        py0 = by1 + 100
        py1 = by1 + box_h - 40
        draw.rectangle([(px0, py0), (px1, py1)], fill=(val, val, val), outline=(50, 50, 50))
        # Label vertically or angled
        draw.text((px0 + 2, py0 + 20), f"{val}", fill=(200, 200, 200), font=font_s)
        draw.text((px0 + 2, py1 - 40), f"V:{val}", fill=(200, 200, 200), font=font_s)

    # Right Box: Near-White Field
    bx2 = bx1 + box_w + 40
    draw.rectangle([(bx2, by1), (bx2 + box_w, by1 + box_h)], fill=(255, 255, 255), outline=(180, 180, 180))
    
    draw.text((bx2 + 20, by1 + 20), "【亮部白階校準 (White Level - 調整顯示器對比度)】", fill=(0, 0, 0), font=font)
    draw.text((bx2 + 20, by1 + 50), "背景基準：RGB (255, 255, 255) 極限白", fill=(100, 100, 100), font=font_s)

    white_patches = [
        (230, "RGB 230 (90%)"),
        (235, "RGB 235 (Video White)"),
        (240, "RGB 240 (94%)"),
        (245, "RGB 245 (96%)"),
        (248, "RGB 248 (97%)"),
        (250, "RGB 250 (98%)"),
        (251, "RGB 251 (98.4%)"),
        (252, "RGB 252 (98.8%)"),
        (253, "RGB 253 (99.2%)"),
        (254, "RGB 254 (99.6%)"),
    ]
    
    pw2 = (box_w - 40) // len(white_patches)
    for i, (val, label) in enumerate(white_patches):
        px0 = bx2 + 20 + i * pw2
        px1 = px0 + pw2 - 4
        py0 = by1 + 100
        py1 = by1 + box_h - 40
        draw.rectangle([(px0, py0), (px1, py1)], fill=(val, val, val), outline=(200, 200, 200))
        draw.text((px0 + 2, py0 + 20), f"{val}", fill=(0, 0, 0), font=font_s)
        draw.text((px0 + 2, py1 - 40), f"V:{val}", fill=(0, 0, 0), font=font_s)

    # Bottom summary box
    draw.rectangle([(40, H - 180), (W - 40, H - 40)], fill=(30, 30, 30), outline=(100, 100, 100))
    draw.text((60, H - 165), "判定標準：", fill=(255, 200, 0), font=font)
    draw.text((60, H - 135), "1. 正常調校下，RGB 2 ~ 4 應能在暗室環境下與 RGB 0 背景分辨開來。若 RGB 8 以下全融為一體，代表『暗部細節死黑(Black Crush)』。", fill=(220, 220, 220), font=font_s)
    draw.text((60, H - 105), "2. 正常調校下，RGB 250 ~ 254 應能與 RGB 255 背景清晰區分。若 RGB 240 以上均融成純白，代表『亮部過曝爆白(Highlight Clipping)』。", fill=(220, 220, 220), font=font_s)
    draw.text((60, H - 75), "3. AR 眼鏡由於光機光學穿透率(See-Through)問題，需特別關注周圍環境光干擾下黑階是否浮起發灰或暗部細節流失。", fill=(180, 220, 255), font=font_s)

    img.save(os.path.join(OUTPUT_DIR, "05_PLUGE_Black_White_Clipping_1080p.png"))
    print("Generated 05_PLUGE_Black_White_Clipping_1080p.png")

# ==========================================
# 06. SMPTE HD Color Bars 1080p
# ==========================================
def gen_06_smpte_hd():
    # Use ffmpeg to generate the broadcast-standard SMPTE HD Bars directly
    out_path = os.path.join(OUTPUT_DIR, "06_SMPTE_HD_ColorBars_1080p.png")
    cmd = [
        "ffmpeg", "-f", "lavfi",
        "-i", "smptehdbars=size=1920x1080:rate=1",
        "-vframes", "1",
        "-pix_fmt", "rgb24",
        out_path, "-y"
    ]
    subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    print("Generated 06_SMPTE_HD_ColorBars_1080p.png via FFmpeg")

# ==========================================
# 07. Standard 24-Patch Macbeth ColorChecker
# ==========================================
def gen_07_colorchecker_24():
    img = Image.new('RGB', (W, H), (30, 30, 30))
    draw = ImageDraw.Draw(img)
    
    font_t = get_font(22)
    font = get_font(15)
    font_s = get_font(12)
    
    draw.text((40, 30), "07: 標準 24 色 Macbeth ColorChecker 色卡 (色彩還原度與膚色測試)", fill=(255, 255, 255), font=font_t)
    draw.text((40, 60), "評估指標：自然界記憶色(深/淺膚色、藍天、植物綠)、三原色與副原色飽和度、中性灰階中立性", fill=(200, 200, 200), font=font)
    
    # Classic 24 patches standard sRGB D65 values
    patches = [
        # Row 1 (Natural colors)
        [("Dark Skin (深膚色)", (115, 82, 68)),
         ("Light Skin (淺膚色)", (194, 150, 130)),
         ("Blue Sky (藍天)", (98, 122, 157)),
         ("Foliage (植物綠)", (87, 108, 67)),
         ("Blue Flower (藍花)", (133, 128, 177)),
         ("Bluish Green (水綠)", (103, 189, 170))],
        # Row 2 (Miscellaneous chromatic)
        [("Orange (橙色)", (214, 126, 44)),
         ("Purplish Blue (紫藍)", (80, 91, 166)),
         ("Moderate Red (溫和紅)", (193, 90, 99)),
         ("Purple (紫色)", (94, 60, 108)),
         ("Yellow Green (黃綠)", (157, 188, 64)),
         ("Orange Yellow (橙黃)", (224, 163, 46))],
        # Row 3 (Primary & Secondary)
        [("Blue (標準藍)", (56, 61, 150)),
         ("Green (標準綠)", (70, 148, 73)),
         ("Red (標準紅)", (175, 54, 60)),
         ("Yellow (標準黃)", (231, 199, 31)),
         ("Magenta (洋紅)", (187, 86, 149)),
         ("Cyan (青色)", (8, 133, 161))],
        # Row 4 (Grayscale)
        [("White 9.5 (純白)", (243, 243, 242)),
         ("Neutral 8 (中性灰8)", (200, 200, 200)),
         ("Neutral 6.5 (中性灰6.5)", (160, 160, 160)),
         ("Neutral 5 (中性灰5)", (122, 122, 121)),
         ("Neutral 3.5 (中性灰3.5)", (85, 85, 85)),
         ("Black 2 (黑階2)", (52, 52, 52))]
    ]
    
    top_y = 110
    card_w = W - 160
    card_h = H - 240
    cell_w = card_w // 6
    cell_h = card_h // 4
    
    # Outer frame
    draw.rectangle([(80, top_y), (80 + card_w, top_y + card_h)], fill=(18, 18, 18), outline=(100, 100, 100), width=2)
    
    margin = 15
    for r in range(4):
        for c in range(6):
            name, (red, green, blue) = patches[r][c]
            x0 = 80 + c * cell_w + margin
            y0 = top_y + r * cell_h + margin
            x1 = 80 + (c + 1) * cell_w - margin
            y1 = top_y + (r + 1) * cell_h - margin
            
            # Draw color patch
            draw.rectangle([(x0, y0), (x1, y1)], fill=(red, green, blue), outline=(0, 0, 0), width=1)
            
            # Information text
            luminance = 0.299 * red + 0.587 * green + 0.114 * blue
            txt_col = (0, 0, 0) if luminance > 120 else (255, 255, 255)
            
            draw.text((x0 + 8, y0 + 8), name, fill=txt_col, font=font_s)
            draw.text((x0 + 8, y1 - 22), f"({red}, {green}, {blue})", fill=txt_col, font=font_s)
            
    # Bottom note
    draw.text((80, H - 90), "測試方式：使用色彩儀(Colorimeter/Spectroradiometer)量測各色塊之 xyY / CIELAB 座標，計算 Delta E 色偏值。", fill=(220, 220, 220), font=font)
    draw.text((80, H - 65), "人眼評估：特別注意『深/淺膚色』是否泛紅或偏綠、『植物綠』是否失真發青，以及第四排灰階色塊是否中立無色偏。", fill=(180, 220, 255), font=font)

    img.save(os.path.join(OUTPUT_DIR, "07_ColorChecker_24_Patches_1080p.png"))
    print("Generated 07_ColorChecker_24_Patches_1080p.png")

# ==========================================
# 08. 6-Axis Color Saturation Gradients
# ==========================================
def gen_08_color_gradients():
    img = Image.new('RGB', (W, H), (15, 15, 15))
    draw = ImageDraw.Draw(img)
    
    font_t = get_font(22)
    font = get_font(15)
    font_s = get_font(13)
    
    draw.text((40, 30), "08: RGBCMY 六軸色彩平滑飽和度漸層 (Color Clipping & Linear Saturation)", fill=(255, 255, 255), font=font_t)
    draw.text((40, 60), "評估指標：單色通道飽和度線性度、高飽和度截波失真(Clipping)、色調過渡連續性", fill=(200, 200, 200), font=font)

    colors = [
        ("Red (紅色通道漸層)", lambda v: (v, 0, 0)),
        ("Green (綠色通道漸層)", lambda v: (0, v, 0)),
        ("Blue (藍色通道漸層)", lambda v: (0, 0, v)),
        ("Cyan (青色通道漸層)", lambda v: (0, v, v)),
        ("Magenta (洋紅通道漸層)", lambda v: (v, 0, v)),
        ("Yellow (黃色通道漸層)", lambda v: (v, v, 0)),
        ("White (全光譜灰白階漸層)", lambda v: (v, v, v))
    ]
    
    bar_w = W - 100
    bar_h = 75
    gap = 45
    start_y = 110
    
    for i, (name, col_fn) in enumerate(colors):
        y0 = start_y + i * (bar_h + gap)
        draw.text((50, y0 - 22), name, fill=(220, 220, 220), font=font_s)
        for x in range(bar_w):
            val = int(x * 255.0 / (bar_w - 1))
            draw.line([(50 + x, y0), (50 + x, y0 + bar_h)], fill=col_fn(val), width=1)
        draw.rectangle([(50, y0), (50 + bar_w, y0 + bar_h)], outline=(100, 100, 100), width=1)

    img.save(os.path.join(OUTPUT_DIR, "08_Color_Gradients_RGBMYC_1080p.png"))
    print("Generated 08_Color_Gradients_RGBMYC_1080p.png")

# ==========================================
# 09. 100% White Uniformity with ANSI 9-Point
# ==========================================
def gen_09_white_uniformity():
    img = Image.new('RGB', (W, H), (255, 255, 255))
    draw = ImageDraw.Draw(img)
    
    font = get_font(18)
    font_s = get_font(14)
    
    # ANSI 9-point coordinates (standard 10% / 50% / 90% in 3x3 layout)
    xs = [int(W * 0.1), int(W * 0.5), int(W * 0.9)]
    ys = [int(H * 0.1), int(H * 0.5), int(H * 0.9)]
    
    pt_idx = 1
    for y in ys:
        for x in xs:
            # Draw crosshair and circle
            draw.ellipse([(x - 25, y - 25), (x + 25, y + 25)], outline=(180, 180, 180), width=2)
            draw.line([(x - 35, y), (x + 35, y)], fill=(180, 180, 180), width=2)
            draw.line([(x, y - 35), (x, y + 35)], fill=(180, 180, 180), width=2)
            draw.text((x + 10, y - 45), f"P{pt_idx}", fill=(120, 120, 120), font=font)
            pt_idx += 1
            
    # Bottom instruction box
    bw, bh = 800, 90
    bx, by = (W - bw) // 2, H - bh - 30
    draw.rectangle([(bx, by), (bx + bw, by + bh)], fill=(240, 240, 240), outline=(180, 180, 180), width=1)
    draw.text((bx + 20, by + 12), "09: 100% 純白場與 ANSI 9點均勻度測試 (White Luminance Uniformity)", fill=(50, 50, 50), font=font)
    draw.text((bx + 20, by + 40), "量測方法：以照度計測量 P1 ~ P9 之照度值(Lux) | 均勻度計算公式：Uniformity = Min(L) / Max(L) * 100%", fill=(80, 80, 80), font=font_s)
    draw.text((bx + 20, by + 65), "目測要點：檢查投影鏡頭邊角暗角(Vignetting)衰減、色溫不均勻(如左偏紅/右偏綠)、光學塵點與波導漏光", fill=(100, 100, 100), font=font_s)

    img.save(os.path.join(OUTPUT_DIR, "09_Uniformity_100_White_1080p.png"))
    print("Generated 09_Uniformity_100_White_1080p.png")

# ==========================================
# 10. 50% Mid-Gray Uniformity
# ==========================================
def gen_10_midgray_uniformity():
    img = Image.new('RGB', (W, H), (128, 128, 128))
    draw = ImageDraw.Draw(img)
    
    font = get_font(18)
    font_s = get_font(14)
    
    # Subtle alignment guide
    draw.rectangle([(W*0.05, H*0.05), (W*0.95, H*0.95)], outline=(140, 140, 140), width=1)
    
    bw, bh = 760, 80
    bx, by = (W - bw) // 2, H - bh - 30
    draw.rectangle([(bx, by), (bx + bw, by + bh)], fill=(110, 110, 110), outline=(150, 150, 150), width=1)
    draw.text((bx + 20, by + 12), "10: 50% 中性灰場測試 (Dirty Screen Effect & Mura 斑駁效應)", fill=(240, 240, 240), font=font)
    draw.text((bx + 20, by + 42), "目測評估：檢測顯示晶片(DMD/Micro-OLED/LCoS)之空間亮度微小斑駁(Mura)、髒屏效應(DSE)、波導光柵條紋乾涉", fill=(220, 220, 220), font=font_s)

    img.save(os.path.join(OUTPUT_DIR, "10_Uniformity_50_MidGray_1080p.png"))
    print("Generated 10_Uniformity_50_MidGray_1080p.png")

# ==========================================
# 11. Chromatic Aberration & Convergence
# ==========================================
def gen_11_chromatic_aberration():
    img = Image.new('RGB', (W, H), (0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    font_t = get_font(22)
    font = get_font(15)
    font_s = get_font(13)
    
    draw.text((40, 30), "11: 橫向色差與 RGB 三色收斂測試 (Chromatic Aberration & Panel Convergence)", fill=(255, 255, 255), font=font_t)
    draw.text((40, 60), "專為 AR 眼鏡(波導色散/光學邊緣彩虹紋)與投影機(3LCD/3DLP/LCoS 三片式光機對準)設計", fill=(200, 200, 200), font=font)

    # 1px grid in outer border
    step = 40
    for x in range(0, W, step):
        draw.line([(x, 0), (x, H)], fill=(40, 40, 40), width=1)
    for y in range(0, H, step):
        draw.line([(0, y), (W, y)], fill=(40, 40, 40), width=1)

    # Crosshair generator with R, G, B, W components
    def draw_convergence_cross(cx, cy, arm_len=30):
        # 1px white cross
        draw.line([(cx - arm_len, cy), (cx + arm_len, cy)], fill=(255, 255, 255), width=1)
        draw.line([(cx, cy - arm_len), (cx, cy + arm_len)], fill=(255, 255, 255), width=1)
        # Dot boxes
        draw.rectangle([(cx - arm_len - 10, cy - 2), (cx - arm_len - 6, cy + 2)], fill=(255, 0, 0))
        draw.rectangle([(cx + arm_len + 6, cy - 2), (cx + arm_len + 10, cy + 2)], fill=(0, 255, 0))
        draw.rectangle([(cx - 2, cy - arm_len - 10), (cx + 2, cy - arm_len - 6)], fill=(0, 0, 255))
        draw.rectangle([(cx - 2, cy + arm_len + 6), (cx + 2, cy + arm_len + 10)], fill=(255, 255, 255))
        # Concentric rings
        for r in [10, 20, 30]:
            draw.ellipse([(cx - r, cy - r), (cx + r, cy + r)], outline=(120, 120, 120), width=1)

    # Place convergence targets at center, corners, and quadrant centers
    coords = [
        (W // 2, H // 2, "Center (中心基準)"),
        (80, 100, "Top-Left Corner (左上視角邊緣)"),
        (W - 80, 100, "Top-Right Corner (右上視角邊緣)"),
        (80, H - 100, "Bottom-Left Corner (左下視角邊緣)"),
        (W - 80, H - 100, "Bottom-Right Corner (右下視角邊緣)"),
        (W // 2, 100, "Top-Center"),
        (W // 2, H - 100, "Bottom-Center"),
        (80, H // 2, "Left-Center"),
        (W - 80, H // 2, "Right-Center")
    ]
    
    for cx, cy, label in coords:
        draw_convergence_cross(cx, cy, arm_len=35)
        draw.text((cx + 15, cy + 15), label, fill=(180, 180, 180), font=font_s)

    # High-contrast 1px alternating checkerboard patches at corners
    for bx, by in [(160, 140), (W - 240, 140), (160, H - 220), (W - 240, H - 220)]:
        # 80x80 checkerboard with 2px checks
        for r in range(0, 80, 4):
            for c in range(0, 80, 4):
                fill_c = (255, 255, 255) if ((r//4 + c//4) % 2 == 0) else (0, 0, 0)
                draw.rectangle([(bx + c, by + r), (bx + c + 3, by + r + 3)], fill=fill_c)
        draw.rectangle([(bx, by), (bx + 80, by + 80)], outline=(100, 100, 100), width=1)
        draw.text((bx, by + 85), "高對比黑白邊緣 (檢查色邊)", fill=(200, 200, 200), font=font_s)

    img.save(os.path.join(OUTPUT_DIR, "11_Chromatic_Aberration_Convergence_1080p.png"))
    print("Generated 11_Chromatic_Aberration_Convergence_1080p.png")

# ==========================================
# 12. AR Text Readability & PPD Legibility
# ==========================================
def gen_12_ar_text_ppd():
    img = Image.new('RGB', (W, H), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 384 -> 320 -> 256 -> 192 -> 256 -> 320 -> 384 (6:5:4:3:4:5:6 ratio)
    # Scaled by 0.5 to fit 1080p canvas with comfortable gaps
    sizes = [192, 160, 128, 96, 128, 160, 192]

    row_data = []
    for sz in sizes:
        f = ImageFont.truetype(SANS_BOLD_PATH, sz)
        line = ''
        c = 65  # Start from 'A' on every row
        while True:
            test = line + chr(c)
            if f.getlength(test) > (W - 20):
                break
            line = test
            c += 1
            if c > 90:
                c = 65
        bb = draw.textbbox((0, 0), line, font=f, anchor='lt')
        lw = bb[2] - bb[0]
        lh = bb[3] - bb[1]
        row_data.append({'sz': sz, 'font': f, 'line': line, 'w': lw, 'h': lh})

    total_h = sum(r['h'] for r in row_data)
    rem = H - total_h
    gaps = [15, 14, 12, 12, 14, 15]
    top_margin = (rem - sum(gaps)) // 2

    cur_y = top_margin
    for i, r in enumerate(row_data):
        x = (W - r['w']) // 2
        draw.text((x, cur_y), r['line'], fill=(0, 255, 0), font=r['font'], anchor='lt')
        if i < len(gaps):
            cur_y += r['h'] + gaps[i]
        else:
            cur_y += r['h']

    img.save(os.path.join(OUTPUT_DIR, "12_AR_Text_PPD_Legibility_1080p.png"))
    print("Generated 12_AR_Text_PPD_Legibility_1080p.png")

# ==========================================
# 18. AR Text PPD Legibility (Min size 128, 7 rows)
# ==========================================
def gen_18_ar_text_ppd():
    img = Image.new('RGB', (W, H), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 180 -> 160 -> 140 -> 128 -> 140 -> 160 -> 180
    sizes = [180, 160, 140, 128, 140, 160, 180]

    row_data = []
    for sz in sizes:
        f = ImageFont.truetype(SANS_BOLD_PATH, sz)
        line = ''
        c = 65  # Start from 'A' on every row
        while True:
            test = line + chr(c)
            if f.getlength(test) > (W - 20):
                break
            line = test
            c += 1
            if c > 90:
                c = 65
        bb = draw.textbbox((0, 0), line, font=f, anchor='lt')
        lw = bb[2] - bb[0]
        lh = bb[3] - bb[1]
        row_data.append({'sz': sz, 'font': f, 'line': line, 'w': lw, 'h': lh})

    total_h = sum(r['h'] for r in row_data)
    rem = H - total_h
    gaps = [10, 9, 8, 8, 9, 10]
    top_margin = (rem - sum(gaps)) // 2

    cur_y = top_margin
    for i, r in enumerate(row_data):
        x = (W - r['w']) // 2
        draw.text((x, cur_y), r['line'], fill=(0, 255, 0), font=r['font'], anchor='lt')
        if i < len(gaps):
            cur_y += r['h'] + gaps[i]
        else:
            cur_y += r['h']

    out_file = os.path.join(OUTPUT_DIR, "18_AR_Text_PPD_legibility_1080p.png")
    img.save(out_file)
    print("Generated 18_AR_Text_PPD_legibility_1080p.png")

# ==========================================
# 13-17. Real-world Photographic Images & Composite
# ==========================================
def process_photo_references():
    # Mapping kodak images to meaningful test filenames
    kodak_map = [
        ("kodim23.png", "13_Photo_SkinTone_Portrait.png", "人物膚色還原、布料紋理、羽毛飽和色"),
        ("kodim04.png", "14_Photo_Texture_Detail.png", "木屋瓦片高頻細節、結構線條、光學解像力"),
        ("kodim08.png", "15_Photo_Color_Discrimination.png", "粉筆與果物豐富色彩階調、細微色差鑑別"),
        ("kodim21.png", "16_Photo_Sky_Ocean_Gradients.png", "藍天海洋連續漸層、動態範圍、無色階斷層")
    ]
    
    pil_images = []
    for src_name, dst_name, desc in kodak_map:
        src_path = os.path.join(OUTPUT_DIR, src_name)
        dst_path = os.path.join(OUTPUT_DIR, dst_name)
        if os.path.exists(src_path):
            im = Image.open(src_path)
            # Make a padded/centered 1080p presentation version
            sw, sh = im.size
            scale = min((W - 80) / sw, (H - 120) / sh)
            nw, nh = int(sw * scale), int(sh * scale)
            resized = im.resize((nw, nh), Image.LANCZOS)
            
            canvas = Image.new('RGB', (W, H), (20, 20, 20))
            ox, oy = (W - nw) // 2, (H - nh) // 2 + 20
            canvas.paste(resized, (ox, oy))
            
            draw = ImageDraw.Draw(canvas)
            font_t = get_font(22)
            font_s = get_font(16)
            draw.text((40, 25), f"真實場景測試圖：{dst_name[:2]} - {desc}", fill=(255, 255, 255), font=font_t)
            canvas.save(dst_path)
            pil_images.append((im, desc, dst_name[:2]))
            print(f"Generated {dst_name}")

    # Generate 17: 2x2 Composite comparison 1080p
    if len(pil_images) == 4:
        comp = Image.new('RGB', (W, H), (15, 15, 15))
        draw = ImageDraw.Draw(comp)
        
        # 4 quadrants
        qw, qh = (W - 30) // 2, (H - 90) // 2
        coords = [
            (10, 50),
            (10 + qw + 10, 50),
            (10, 50 + qh + 10),
            (10 + qw + 10, 50 + qh + 10)
        ]
        font_t = get_font(20)
        font_s = get_font(14)
        draw.text((20, 15), "17: 真實世界綜合畫質評估 4合1 對照圖 (Real-World Image Quality Composite)", fill=(255, 255, 255), font=font_t)
        
        for idx, ((pim, pdesc, pnum), (qx, qy)) in enumerate(zip(pil_images, coords)):
            sw, sh = pim.size
            scale = min((qw - 20) / sw, (qh - 45) / sh)
            nw, nh = int(sw * scale), int(sh * scale)
            r_im = pim.resize((nw, nh), Image.LANCZOS)
            ox = qx + (qw - nw) // 2
            oy = qy + (qh - nh) // 2 + 10
            comp.paste(r_im, (ox, oy))
            draw.rectangle([(qx, qy), (qx + qw, qy + qh)], outline=(80, 80, 80), width=1)
            draw.text((qx + 15, qy + 10), f"[{pnum}] {pdesc}", fill=(220, 220, 220), font=font_s)
            
        comp.save(os.path.join(OUTPUT_DIR, "17_Photo_RealWorld_Composite_1080p.png"))
        print("Generated 17_Photo_RealWorld_Composite_1080p.png")

if __name__ == "__main__":
    gen_01_geometry_focus()
    gen_02_iso12233()
    gen_03_ansi_contrast()
    gen_04_grayscale_gamma()
    gen_05_pluge_clipping()
    gen_06_smpte_hd()
    gen_07_colorchecker_24()
    gen_08_color_gradients()
    gen_09_white_uniformity()
    gen_10_midgray_uniformity()
    gen_11_chromatic_aberration()
    gen_12_ar_text_ppd()
    process_photo_references()
    gen_18_ar_text_ppd()
    print("All test patterns successfully generated!")
