import os
import sys
from PIL import Image
from reportlab.lib.pagesizes import A4
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Image as RLImage, 
    Table, TableStyle, PageBreak, KeepTogether, HRFlowable
)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib import colors
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.fonts import addMapping
from reportlab.pdfgen import canvas

BASE_DIR = "/home/venom/Pictures/Test-materials"
FONT_PATH = "/usr/share/fonts/truetype/arphic/uming.ttc"
PDF_PATH = os.path.join(BASE_DIR, "投影裝置與AR眼鏡顯示品質測試指南.pdf")
THUMB_DIR = os.path.join(BASE_DIR, ".thumbs")
os.makedirs(THUMB_DIR, exist_ok=True)

# Register font with full mappings
pdfmetrics.registerFont(TTFont('Uming', FONT_PATH, subfontIndex=0))
pdfmetrics.registerFont(TTFont('Uming-Bold', FONT_PATH, subfontIndex=0))
addMapping('Uming', 0, 0, 'Uming')
addMapping('Uming', 1, 0, 'Uming-Bold')
addMapping('Uming', 0, 1, 'Uming')
addMapping('Uming', 1, 1, 'Uming-Bold')

# Create smaller thumbs for PDF embedding to ensure compact file size & fast rendering
def get_pdf_thumb(filename, max_w=720):
    src = os.path.join(BASE_DIR, filename)
    thumb_path = os.path.join(THUMB_DIR, "thumb_" + filename)
    if not os.path.exists(thumb_path):
        im = Image.open(src)
        w, h = im.size
        scale = max_w / float(w)
        nw, nh = int(w * scale), int(h * scale)
        rim = im.resize((nw, nh), Image.LANCZOS)
        rim.save(thumb_path, "PNG", optimize=True)
    return thumb_path

class NumberedCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        super(NumberedCanvas, self).__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_page_decorations(num_pages)
            super(NumberedCanvas, self).showPage()
        super(NumberedCanvas, self).save()

    def draw_page_decorations(self, page_count):
        if self._pageNumber == 1:
            return  # Suppress headers/footers on cover page
        
        self.saveState()
        self.setFont("Uming", 8)
        self.setFillColor(colors.HexColor("#666666"))
        
        # Header
        self.drawString(40, 810, "投影裝置與 AR 眼鏡顯示品質測試指南 (色彩與光學影像品質評估)")
        self.setStrokeColor(colors.HexColor("#CCCCCC"))
        self.setLineWidth(0.5)
        self.line(40, 804, 555, 804)
        
        # Footer
        page_str = f"第 {self._pageNumber} 頁 / 共 {page_count} 頁"
        self.drawRightString(555, 30, page_str)
        self.drawString(40, 30, "內部校準與評測專用 | 建議於原生解析度 1080p / 4K 1:1 點對點模式下測試")
        self.line(40, 42, 555, 42)
        self.restoreState()

def build_pdf():
    doc = SimpleDocTemplate(
        PDF_PATH,
        pagesize=A4,
        leftMargin=40,
        rightMargin=40,
        topMargin=48,
        bottomMargin=48
    )
    
    styles = getSampleStyleSheet()
    
    # Custom colors
    c_primary = colors.HexColor("#1A365D")   # Deep navy
    c_secondary = colors.HexColor("#2B6CB0") # Medium blue
    c_dark = colors.HexColor("#2D3748")      # Dark text
    c_accent = colors.HexColor("#C53030")    # Accent red
    
    style_cover_title = ParagraphStyle(
        'CoverTitle',
        parent=styles['Normal'],
        fontName='Uming',
        fontSize=24,
        leading=34,
        textColor=c_primary,
        alignment=1, # Center
        spaceAfter=15
    )
    
    style_cover_sub = ParagraphStyle(
        'CoverSub',
        parent=styles['Normal'],
        fontName='Uming',
        fontSize=12,
        leading=19,
        textColor=c_secondary,
        alignment=1,
        spaceAfter=25
    )
    
    style_h1 = ParagraphStyle(
        'SectionH1',
        parent=styles['Normal'],
        fontName='Uming',
        fontSize=15,
        leading=21,
        textColor=c_primary,
        spaceBefore=14,
        spaceAfter=8,
        keepWithNext=True
    )

    style_h2 = ParagraphStyle(
        'SectionH2',
        parent=styles['Normal'],
        fontName='Uming',
        fontSize=12,
        leading=17,
        textColor=c_secondary,
        spaceBefore=10,
        spaceAfter=5,
        keepWithNext=True
    )
    
    style_body = ParagraphStyle(
        'Body',
        parent=styles['Normal'],
        fontName='Uming',
        fontSize=9.5,
        leading=14.5,
        textColor=c_dark,
        spaceAfter=4
    )

    style_body_bold = ParagraphStyle(
        'BodyBold',
        parent=style_body,
        fontName='Uming',
        textColor=c_primary
    )

    story = []

    # ========================================================
    # COVER PAGE
    # ========================================================
    story.append(Spacer(1, 35))
    story.append(Paragraph("<b>投影裝置與 AR 眼鏡顯示品質測試指南</b>", style_cover_title))
    story.append(Paragraph("Display Quality Calibration & Testing Guide for Projectors & AR Glasses<br/><b>色彩表現・動態範圍・幾何失真・光學解析度全方位檢測手冊</b>", style_cover_sub))
    story.append(HRFlowable(width="100%", thickness=2, color=c_secondary, spaceAfter=22))
    
    # Overview Box
    meta_data = [
        [Paragraph("<b>測試素材版本</b>", style_body), Paragraph("V1.0 (含 17 組 1080p 專業校準圖與真實場景照片)", style_body)],
        [Paragraph("<b>適用硬體範疇</b>", style_body), Paragraph("投影機 (DLP / 3LCD / 雷射投影) 及 AR 眼鏡 (Micro-OLED / Micro-LED / 光波導 / BirdBath)", style_body)],
        [Paragraph("<b>主要評估面向</b>", style_body), Paragraph("幾何聚焦、ANSI 對比度、32階灰階動態、PLUGE 黑白階裁切、BT.709 色彩、色差收斂、PPD 文字辨識", style_body)],
        [Paragraph("<b>測試環境要求</b>", style_body), Paragraph("100% 原始比例 (Native 1:1 Pixel Mapping)、關閉梯形縮放與數位銳化、暗室環境 (投影) / 受控光照 (AR)", style_body)]
    ]
    meta_table = Table(meta_data, colWidths=[110, 405])
    meta_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), colors.HexColor("#F7FAFC")),
        ('BOX', (0, 0), (-1, -1), 1, colors.HexColor("#CBD5E0")),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#E2E8F0")),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 6),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
        ('LEFTPADDING', (0, 0), (-1, -1), 10),
        ('RIGHTPADDING', (0, 0), (-1, -1), 10),
    ]))
    story.append(meta_table)
    story.append(Spacer(1, 18))
    
    # Table of Contents Preview
    story.append(Paragraph("<b>目錄架構導覽</b>", style_h2))
    toc_data = [
        [Paragraph("<b>第一章</b>", style_body_bold), Paragraph("測試前置設定與投影 / AR 核心差異觀念 (環境光、1:1 點對點、PPD、光波導)", style_body)],
        [Paragraph("<b>第二章</b>", style_body_bold), Paragraph("光學對焦、幾何失真與解析度極限 (01 幾何對焦網格圖、02 ISO 12233 空間頻率圖)", style_body)],
        [Paragraph("<b>第三章</b>", style_body_bold), Paragraph("對比度、灰階層次與動態範圍 (03 ANSI 棋盤格、04 32階灰階Gamma、05 PLUGE 黑白階裁切)", style_body)],
        [Paragraph("<b>第四章</b>", style_body_bold), Paragraph("色彩準確度、飽和度與色階過渡 (06 SMPTE 彩條、07 24色色卡、08 六軸平滑漸層)", style_body)],
        [Paragraph("<b>第五章</b>", style_body_bold), Paragraph("光學均勻度、暗角與色差收斂 (09 9點純白場、10 50%灰場Mura、11 橫向色差與收斂)", style_body)],
        [Paragraph("<b>第六章</b>", style_body_bold), Paragraph("AR 眼鏡專用：微文字可讀性與角解析度 (12 正顯/反顯微文字與 Nyquist 極限)", style_body)],
        [Paragraph("<b>第七章</b>", style_body_bold), Paragraph("真實世界攝影影像綜合驗證 (13~16 Kodak 參考照片、17 4合1 綜合對照圖)", style_body)],
        [Paragraph("<b>第八章</b>", style_body_bold), Paragraph("光學顯示品質快速檢核評分表 (檢查項目、判定標準與常見異常排查矩陣)", style_body)]
    ]
    t_toc = Table(toc_data, colWidths=[55, 460])
    t_toc.setStyle(TableStyle([
        ('LINEBELOW', (0, 0), (-1, -1), 0.5, colors.HexColor("#EDF2F7")),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
    ]))
    story.append(t_toc)
    story.append(PageBreak())

    # ========================================================
    # SECTION 1: Setup & Differences
    # ========================================================
    story.append(Paragraph("第一章：測試前置設定與投影 / AR 核心差異", style_h1))
    story.append(HRFlowable(width="100%", thickness=1, color=c_secondary, spaceAfter=10))
    
    intro_p1 = """
    評估投影裝置（長焦/短焦投影機、微型投影機）與擴增實境眼鏡（AR Glasses / NED 近眼顯示系統）時，必須先了解其成像光學本質的關鍵差異：
    <br/><br/>
    <b>1. 投影機（Projector）</b>：由發光引擎（LED / Laser / 燈泡）經過調光晶片（DMD、3LCD、LCoS），再透過投影物鏡將光束放大投射至遠處屏幕（漫反射介質）。主要挑戰在於<b>投影鏡頭中心與邊角聚焦均勻度</b>、<b>梯形校正導致的像素重採樣模糊</b>、<b>光機內反射造成的 ANSI 棋盤對比衰減</b>，以及<b>環境光（Ambient Light）對黑階的洗白效應</b>。
    <br/><br/>
    <b>2. AR 眼鏡（AR Smart Glasses）</b>：由微型顯示器（Micro-OLED、Micro-LED、LCoS）結合近眼光學系統（光波導 Waveguide 或 BirdBath 光學透鏡）將虛擬圖像投射入人眼瞳孔（Eyebox）。主要挑戰在於<b>角解析度（PPD, Pixels Per Degree）</b>而非單純解析度、<b>波導繞射導致的邊緣色散（彩虹紋/橫向色差）</b>、<b>視瞳範圍（Eyebox）移動時的亮度與色溫漂移</b>，以及<b>穿透式（See-through）黑階本質為透明，需對抗現實環境強光</b>。
    """
    story.append(Paragraph(intro_p1, style_body))
    story.append(Spacer(1, 8))
    
    rules = [
        [Paragraph("<b>關鍵前置規範</b>", style_body_bold), Paragraph("<b>具體操作與要求</b>", style_body_bold), Paragraph("<b>違規後果</b>", style_body_bold)],
        [
            Paragraph("1. 原生 1:1 點對點<br/>(Pixel-Perfect)", style_body),
            Paragraph("輸入訊號端（PC/播放器）必須輸出顯示設備的原生解析度（如 1080p 1920x1080），並將顯示比例設為『點對點 (Just Scan / 1:1)』，關閉超頻掃描 (Overscan)。", style_body),
            Paragraph("任何數位縮放將破壞 1 像素黑白交錯線對，導致測試圖出現莫爾波紋 (Moiré) 與假性模糊。", style_body)
        ],
        [
            Paragraph("2. 關閉數位銳化<br/>(Sharpness = 0)", style_body),
            Paragraph("在投影機或 AR 軟體設定中，將銳利度（Sharpness）降至中性值（通常為 0 或 50，無邊緣白邊為準）。", style_body),
            Paragraph("過度銳化會產生假邊緣白邊 (Ringing / Halo Artifacts)，誤導光學 MTF 與聚焦判斷。", style_body)
        ],
        [
            Paragraph("3. 測試環境控制", style_body),
            Paragraph("<b>投影機</b>：測試對比與黑階時務必全暗室環境（&lt; 0.1 Lux 照度）。<br/><b>AR 眼鏡</b>：需於全黑環境評估虛擬畫質，另於 300~500 Lux 室內辦公光下評估光學穿透對比度。", style_body),
            Paragraph("環境漏光會直接摧毀黑階，導致 PLUGE 與對比度量測失去參考價值。", style_body)
        ]
    ]
    t_rules = Table(rules, colWidths=[100, 260, 155])
    t_rules.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#E2E8F0")),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('TOPPADDING', (0, 0), (-1, -1), 5),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
    ]))
    story.append(t_rules)
    story.append(PageBreak())

    # Helper function for pattern card layout
    def add_pattern_card(filename, title, purpose, step_guide, proj_check, ar_check, pass_criteria):
        card_story = []
        card_story.append(Paragraph(title, style_h1))
        card_story.append(HRFlowable(width="100%", thickness=1, color=c_secondary, spaceAfter=8))
        
        # Embed Image
        thumb_p = get_pdf_thumb(filename)
        img_obj = RLImage(thumb_p, width=515, height=289)
        card_story.append(img_obj)
        card_story.append(Spacer(1, 8))
        
        # Details Table
        card_data = [
            [Paragraph("<b>測試目的 (What it tests)</b>", style_body_bold), Paragraph(purpose, style_body)],
            [Paragraph("<b>測試操作步驟 (How to use)</b>", style_body_bold), Paragraph(step_guide, style_body)],
            [Paragraph("<b>投影機檢驗要點</b>", style_body_bold), Paragraph(proj_check, style_body)],
            [Paragraph("<b>AR 眼鏡檢驗要點</b>", style_body_bold), Paragraph(ar_check, style_body)],
            [Paragraph("<b>合格標準與異常排查</b>", style_body_bold), Paragraph(pass_criteria, style_body)]
        ]
        t_card = Table(card_data, colWidths=[120, 395])
        t_card.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (0, -1), colors.HexColor("#F7FAFC")),
            ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#E2E8F0")),
            ('VALIGN', (0, 0), (-1, -1), 'TOP'),
            ('TOPPADDING', (0, 0), (-1, -1), 4),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
            ('LEFTPADDING', (0, 0), (-1, -1), 6),
            ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ]))
        card_story.append(t_card)
        card_story.append(PageBreak())
        return card_story

    # ========================================================
    # SECTION 2: Geometry & Resolution
    # ========================================================
    story.extend(add_pattern_card(
        filename="01_Geometry_Focus_Grid_1080p.png",
        title="01: 幾何失真、光學聚焦與線對解析度測試圖 (Geometry & Focus Grid)",
        purpose="評估光學鏡頭的對焦清晰度（中心 vs 四角邊緣）、1:1 點對點像素對齊、幾何畸變（枕狀/桶狀/梯形失真）以及過度掃描（Overscan）裁切狀態。",
        step_guide="1. 將顯示裝置設為 1080p 1:1 模式；<br/>2. 調整投影機手動/電動對焦環或 AR 屈光度旋鈕，使畫面中心的 Siemens Star（西門子星圖）尖端分界最收斂；<br/>3. 隨後檢查四個角落的星圖與 1 像素線對（Line Pairs）是否同時清晰。",
        proj_check="・<b>對焦均勻性</b>：平價投影鏡頭常有『中心清楚、四角模糊』的光學場曲（Field Curvature）。<br/>・<b>梯形失真</b>：若未使用水平垂直正投，檢查外圍白框是否方正，網格有無彎曲。數位梯形校正會導致局部網格粗細不均。<br/>・<b>安全框</b>：確認最外圍 1 像素白色邊框完整顯現，若看不見代表被螢幕 Overscan 裁切。",
        ar_check="・<b>邊緣 MTF 衰減</b>：觀察視場角（FOV）最邊緣的線對與星圖，光波導或 BirdBath 邊緣常出現散光或重影。<br/>・<b>瞳孔微移反應</b>：輕微移動眼睛位置，觀察網格是否產生劇烈幾何形變（動態畸變）。",
        pass_criteria="<b>合格標準</b>：中心與四角之 1px 線對均能清晰辨識單一黑白線條，無灰化黏連；幾何格線筆直無波浪紋。<br/><b>排查建議</b>：若角落模糊，可將焦點微調至『中央稍過焦、四角稍欠焦』的最佳平衡點。"
    ))

    story.extend(add_pattern_card(
        filename="02_ISO12233_Resolution_Chart_1080p.png",
        title="02: ISO 12233 標準空間頻率與光學解像力測試圖 (Optical Resolution Chart)",
        purpose="依據國際標準 ISO 12233，利用不同空間頻率的楔形收斂線（Wedges）、斜邊（Slanted Edges）與多重刻度，定量/定性評估光學調製轉換函數（MTF）與空間解像力極限。",
        step_guide="1. 全螢幕點對點投射本圖；<br/>2. 觀察中心與四角的水平、垂直與 45 度收斂楔形線，找出線條由『黑白分明』轉變為『灰成一片或出現假信號交錯（混疊 Aliasing）』的刻度數值；<br/>3. 比較中心區域與角落區域的解像極限差異。",
        proj_check="・<b>鏡頭像散與色散</b>：若垂直楔形線比水平楔形線先模糊，代表鏡頭具有明顯散光或像散（Astigmatism）。<br/>・<b>數位處理過衝</b>：斜邊黑白交界處若出現亮白光邊，代表內部畫質晶片強加了無法關閉的邊緣增強演算法。",
        ar_check="・<b>波導繞射極限</b>：AR 波導光柵在特定高空間頻率下容易產生多階繞射光斑（Ghosting），導致高頻楔形線提早混疊崩潰。<br/>・<b>虛擬成像面深度</b>：確認注視目標距離（Virtual Image Distance, 通常為 2m~3m）是否能完全銳利成像。",
        pass_criteria="<b>合格標準</b>：中心解像力需達到 1080p 極限刻度，邊緣解像力衰減不超過中心的 25%。<br/><b>排查建議</b>：若畫面出現嚴重的混疊莫爾紋，請檢查訊號線頻寬或顯示卡解析度縮放設定。"
    ))

    # ========================================================
    # SECTION 3: Contrast & Dynamic Range
    # ========================================================
    story.extend(add_pattern_card(
        filename="03_ANSI_Contrast_Checkerboard_1080p.png",
        title="03: ANSI 16 分區棋盤格對比度測試圖 (ANSI Contrast Checkerboard)",
        purpose="評估顯示器同屏動態範圍（Intra-scene Contrast）與光機內部光學散射/雜散光抑制能力。相較於全白/全黑開關對比度（FOFO Contrast），ANSI 對比更能如實反映真實觀影時的通透感與暗部立體感。",
        step_guide="1. 在全暗室環境下投射本圖；<br/>2. 使用照度計（Lux Meter）或光度計，分別量測 8 個白色方塊中心（P1~P16）的照度值取平均（Avg White），以及 8 個黑色方塊中心的照度值取平均（Avg Black）；<br/>3. 計算公式：<b>ANSI Contrast Ratio = Avg White / Avg Black</b>。",
        proj_check="・<b>光機鏡頭雜散光 (Flare / Glare)</b>：白色方塊的光線容易透過投影鏡頭反彈滲入相鄰黑色方塊，使黑色方塊發灰。<br/>・DLP 投影機通常 ANSI 對比極高（可達 300:1 ~ 600:1）；而 3LCD 投影機因偏光板漏光，ANSI 對比通常在 150:1 ~ 250:1 之間。",
        ar_check="・<b>近眼雜散光與護目鏡反射</b>：AR 眼鏡在顯示高對比棋盤格時，需檢驗鏡片內表面是否產生眩光光暈（Glare）或人眼眼眶反光。<br/>・<b>穿透式對比</b>：在室內光環境下，觀察黑色方塊與外界環境疊加後的透明度感知。",
        pass_criteria="<b>合格標準</b>：優質家庭劇院投影機 ANSI 對比 &gt; 250:1；商務投影機 &gt; 120:1；AR 眼鏡暗室對比需黑白分明無明顯光霧溢出。<br/><b>排查建議</b>：若黑區照度過高，檢查投影機鏡頭是否有指紋灰塵，或房間牆壁白色漫反射過重。"
    ))

    story.extend(add_pattern_card(
        filename="04_Grayscale_32Step_Gamma_1080p.png",
        title="04: 32階線性灰階與連續 Gamma 響應測試圖 (Grayscale & Gamma)",
        purpose="檢測顯示設備從暗到亮的色階過渡連續性、8-bit/10-bit 訊號處理能力、色階斷層（Banding）、抖動演算法（Dithering）雜訊，以及 Gamma 2.2 / 2.4 曲線響應是否線性中立。",
        step_guide="1. 檢視上方 32 階灰階階梯：確認每一階與相鄰階梯均有肉眼可辨的亮度跳階；<br/>2. 檢視中間 16 階 IRE 刻度：對照 0% 到 100% 的亮度躍升是否平緩平滑；<br/>3. 檢視下方連續平滑漸層區：確認無縱向條紋斷層（Banding）或綠/洋紅偏色；<br/>4. 對照底部的 Gamma 2.2 與 2.4 參考梯度。",
        proj_check="・<b>灰階中立性 (Gray Neutrality)</b>：觀察灰階在暗部到亮部過程中，色溫是否發生偏移（例如暗部偏藍、中灰偏綠、高光偏黃）。<br/>・<b>DLP 抖動雜訊</b>：單片式 DLP 在極暗階（S1~S3）常使用空間時間抖動（Dither），貼近觀察是否產生劇烈綠色噪點。",
        ar_check="・<b>Micro-OLED 灰階響應</b>：Micro-OLED 在超低灰階時電壓響應敏感，檢查最低數階是否過早沉沒或突亮跳階。<br/>・<b>色彩不均</b>：觀察長條漸層橫貫視野時，左右邊緣色溫是否一致。",
        pass_criteria="<b>合格標準</b>：32 階階梯肉眼均能全數分辨；連續漸層區無跳階垂直斷線；全段中性灰無任何彩度偏移。<br/><b>排查建議</b>：若有色階斷層，檢查播放裝置之輸出色深（8-bit / 10-bit）與 RGB 範圍設定（Full 0-255 vs Limited 16-235）。"
    ))

    story.extend(add_pattern_card(
        filename="05_PLUGE_Black_White_Clipping_1080p.png",
        title="05: PLUGE 暗部黑階與高光白階裁切測試圖 (Black & White Calibration)",
        purpose="專為校正顯示器『亮度 (Brightness / Black Level)』與『對比度 (Contrast / White Level)』設計。防止暗部細節被壓死（Black Crush）或高光細節被過曝裁切（Highlight Blowout）。",
        step_guide="1. <b>校正黑階（左側）</b>：調高顯示器『亮度』直到看見 RGB 1~4 直條，接著慢慢降低亮度，直到 RGB 1 恰好沉入純黑背景，而 <b>RGB 2 ~ 4 仍隱約可見</b>。<br/>2. <b>校正白階（右側）</b>：調低顯示器『對比度』使 RGB 250~254 均能看見，再緩步調高對比度，使 <b>RGB 250 ~ 253 與 RGB 255 純白背景仍有分界</b>，但畫面維持足夠亮度。",
        proj_check="・<b>暗部死黑 (Black Crush)</b>：平價投影機常為了拉高對比數據，將暗部伽瑪拉得很陡，導致 RGB 8 以下全沉入死黑，夜景細節盡失。<br/>・<b>白階過曝</b>：會議簡報模式常將對比推得極高，導致 Excel 淺灰格線或天空雲彩全數過曝消失。",
        ar_check="・<b>環境光補償</b>：在 AR 光學穿透（See-through）下，因真實環境背景有光，黑階在白天會完全透明化，需觀察有效可辨識的最低灰階值是多少。<br/>・<b>高光飽和發光</b>：Micro-OLED 峰值亮度推至極限時，檢查白階條邊緣有無發光擴散暈開現象。",
        pass_criteria="<b>合格標準</b>：在標準觀影環境下，左側 RGB 2~4 能清晰辨別；右側 RGB 250~253 能清楚區分不溢出。<br/><b>排查建議</b>：若黑階怎麼調都是死黑一片，請確認訊號端與顯示端之 HDMI 量化範圍（全幅 0-255 / 有限 16-235）是否匹配。"
    ))

    # ========================================================
    # SECTION 4: Color Accuracy & Saturation
    # ========================================================
    story.extend(add_pattern_card(
        filename="06_SMPTE_HD_ColorBars_1080p.png",
        title="06: SMPTE HD 標準廣播級彩色測試條 (SMPTE HD Color Bars)",
        purpose="廣播電視與數位電影國際標準（ITU-R BT.709）。用於校正三原色（RGB）與三副原色（CMY）的色度（Chrominance）、色相（Hue / Tint）、飽和度（Saturation），以及下層專用 PLUGE 參考階。",
        step_guide="1. 投射標準 SMPTE 彩條；<br/>2. 檢視上層 75% 飽和度色塊（白、黃、青、綠、洋紅、紅、藍）順序與純度；<br/>3. <b>藍色濾鏡模式測試</b>：若設備支援『純藍模式 (Blue-Only Mode)』或使用藍色濾鏡觀察，黃、青、洋紅、紅條對應之藍色分量應與純藍條完全等亮；<br/>4. 檢視最下方 PLUGE 次黑條（-2% 超黑、0% 基準黑、+2% 近黑）以確認訊號電平。",
        proj_check="・<b>色相與矩陣解碼</b>：若 Cyan（青色）發藍或 Yellow（黃色）偏綠，代表投影機色相或 YCbCr 轉 RGB 顏色矩陣解碼有誤。<br/>・<b>色輪色偏 (DLP)</b>：四段或六段色輪投影機在黃色與青色過渡帶常因色輪白色段（W segment）稀釋飽和度而泛白。",
        ar_check="・<b>波導色彩平衡</b>：光波導鏡片對紅綠藍波長的繞射效率不同，檢查整條彩條在視場不同位置是否顏色一致。<br/>・<b>Micro-LED 綠光優勢 vs 紅光短板</b>：若是 Micro-LED AR 光機，特別關注紅色條與洋紅條的純淨度與發光效率。",
        pass_criteria="<b>合格標準</b>：色彩純度高，無串色；在藍色濾鏡模式下，對應色條亮度完全吻合；底部 PLUGE +2% 明顯可見且 -2% 不可見。<br/><b>排查建議</b>：調整投影選單中的『色彩飽和度 (Color)』與『色相 (Tint/Hue)』數值。"
    ))

    story.extend(add_pattern_card(
        filename="07_ColorChecker_24_Patches_1080p.png",
        title="07: 標準 24 色 Macbeth ColorChecker 專業色卡 (ColorChecker 24 Patches)",
        purpose="評估真實世界的色彩還原能力，涵蓋人類最敏感的記憶色（深/淺膚色、藍天、植物綠、藍花）、光譜原色與副原色，以及底層中性灰階階段階梯。是光度儀進行 Delta E 量化色差測量的黃金標準。",
        step_guide="1. 投射本 24 色卡，避免開啟任何動態鮮豔模式（Dynamic/Vivid Mode），切換至『影院模式 (Cinema / Filmmaker Mode)』；<br/>2. <b>目測首要焦點</b>：觀察第一排第 1、2 格的『深膚色 (Dark Skin)』與『淺膚色 (Light Skin)』是否自然紅潤，有無假性慘白、死黃或偏紫發青；<br/>3. 觀察第 3 格『藍天』與第 4 格『植物綠』是否符合自然真實感；<br/>4. 檢視第四排 6 個中性灰階塊，不可帶有任何彩度色偏。",
        proj_check="・<b>膚色泛紅/偏綠</b>：投影機燈泡或雷射光譜常缺乏特定波長（如純紅），常導致膚色塑膠感過重。<br/>・<b>儀器量測</b>：可使用校色器（如 Spyder / i1 Display Pro）依序讀取各色塊 xyY 值，計算 Delta E，專業級應 &lt; 3。",
        ar_check="・<b>虛擬與現實疊加自然度</b>：AR 眼鏡經常顯示人臉虛擬替身（Avatar）或透視導航，膚色與植被色在透視疊加下不可失真。<br/>・<b>透射鏡片色溫偏置</b>：許多 AR 眼鏡鏡片本身帶有淺灰或淺茶色偏光鍍膜，需評估此鍍膜對白平衡之影響。",
        pass_criteria="<b>合格標準</b>：人眼辨識膚色柔和真實；中性灰階塊乾淨無偏色；色彩飽和度適中不溢色。<br/><b>排查建議</b>：進入顯示器色彩管理系統（CMS），微調 2 點或 10 點白平衡（Gain / Bias）及六色 RGBCMY 色調與飽和度。"
    ))

    story.extend(add_pattern_card(
        filename="08_Color_Gradients_RGBMYC_1080p.png",
        title="08: RGBCMY 六軸色彩飽和度平滑漸層圖 (6-Axis Color Gradients)",
        purpose="檢驗紅、綠、藍、青、洋紅、黃六個色彩通道在由黑（0% 飽和）漸變至極限飽和（100% 飽和）過程中的平滑度、飽和度截波失真（Color Clipping）與通道非線性響應。",
        step_guide="1. 逐條觀察 Red、Green、Blue、Cyan、Magenta、Yellow 與 White 漸層長條；<br/>2. 觀察長條最右端（高飽和度區）：確認漸層是否能持續細膩過渡至最後 1 像素，或是在 80%~90% 處就提早飽和變成同一片色塊（代表飽和度過推導致色彩裁切）；<br/>3. 觀察長條中央段：確認有無階梯狀的縱向色帶跳階（Color Banding）。",
        proj_check="・<b>廣色域映射 (Gamut Mapping)</b>：當投影機色域（如 Rec.709）小於訊號源色域（如 DCI-P3）時，若色調映射演算法粗糙，高飽和色條尾端會大面積截波（Clipping）。<br/>・<b>單色通光飽和度</b>：檢查三片式 3LCD 投影機在純藍與純紅通光下的熱漂移飽和現象。",
        ar_check="・<b>Micro-OLED 次像素驅動</b>：Micro-OLED 採用 White-OLED + RGB 彩色濾光片架構，檢查純色高飽和度下白色次像素（W subpixel）是否會洗淡色彩純度。<br/>・<b>波導各色衰減不均</b>：觀察藍色與紅色長條在光波導視場兩側的亮度平衡衰減速率。",
        pass_criteria="<b>合格標準</b>：六個色彩條由黑至極限飽和全程漸變平滑，無突兀斷崖；最右端高光飽和區無提早死結。<br/><b>排查建議</b>：若高飽和度提早截波，請適度降低顯示選單中的『色彩飽和度 (Saturation / Color)』數值。"
    ))

    # ========================================================
    # SECTION 5: Uniformity & Aberrations
    # ========================================================
    story.extend(add_pattern_card(
        filename="09_Uniformity_100_White_1080p.png",
        title="09: 100% 純白場與 ANSI 9 點均勻度測試圖 (100% White Uniformity)",
        purpose="評估顯示畫面亮度均勻度（Luminance Uniformity）、暗角衰減（Vignetting / Falloff）、色溫均勻度（Color Temperature Uniformity，例如左邊偏紅、右邊偏綠），以及鏡頭與光機內部的光學塵點髒污。",
        step_guide="1. 全螢幕投射 100% 純白場，全場維持最高亮度；<br/>2. <b>ANSI 9 點量測</b>：使用照度計（Lux Meter）緊貼屏幕或使用光度計，依序量測 9 個標示圓圈（P1 ~ P9）之中心照度；<br/>3. 計算均勻度公式：<b>均勻度 (%) = (P1~P9 之最小值 / P1~P9 之最大值) * 100%</b>；<br/>4. 裸眼後退觀察全畫面，檢視四個邊角是否有肉眼可見的暗角墜落或彩斑光斑。",
        proj_check="・<b>鏡頭暗角 (Lens Vignetting)</b>：一般投影機中心亮度最高，四個角落（P1, P3, P7, P9）亮度必然衰減，優質劇院機應維持 80% 以上均勻度，微型機通常在 65%~75% 之間。<br/>・<b>光學色斑與塵點</b>：光機內部若有灰塵掉落在 LCD/DMD 焦平面，在純白場下會呈現明顯的綠色、藍色或暗灰色散焦圓斑。",
        ar_check="・<b>Eyebox 視瞳亮度與色溫漂移</b>：AR 光波導在不同入瞳位置常產生嚴重的局部黃斑或藍偏（Rainbow non-uniformity），眼球上下左右移動時觀察白色是否變色。<br/>・<b>波導暗角</b>：檢查視場角兩側是否呈現對稱性暗角。",
        pass_criteria="<b>合格標準</b>：投影機 ANSI 9 點均勻度 &gt; 80%（商務投影 &gt; 70%）；全白場無肉眼可見的明顯色溫偏向或光學塵斑。<br/><b>排查建議</b>：若四角過暗，檢查鏡頭光學變焦（Zoom）是否處於最廣角極端位置，適度縮小變焦環可改善鏡頭邊緣通光量。"
    ))

    story.extend(add_pattern_card(
        filename="10_Uniformity_50_MidGray_1080p.png",
        title="10: 50% 中性灰場測試圖 (50% Mid-Gray & Mura Detection)",
        purpose="中性灰是人眼視覺神經對亮度微小差異最敏感的區間。專門用於抓出純白場或純黑場無法發現的<b>髒屏效應 (Dirty Screen Effect, DSE)</b>、<b>晶片斑駁 (Mura)</b>、光柵條紋乾涉與空間雜訊。",
        step_guide="1. 投射 50% 中性灰畫面（RGB 128, 128, 128）；<br/>2. 保持眼睛與螢幕/鏡片正常觀看距離，輕微晃動視線（動態凝視）；<br/>3. 檢查畫面是否有如同隔了一層磨砂玻璃或髒布的斑駁紋路、直條紋（Vertical Banding）或微弱亮暗塊；<br/>4. 檢查四角邊緣過渡是否平順。",
        proj_check="・<b>3LCD 液晶板老化與灼傷</b>：3LCD 長期受藍光高溫照射，若液晶板老化會在中灰場浮現大片黃褐色不均勻斑塊。<br/>・<b>DMD 散熱微應力</b>：DLP 晶片若散熱不均，受熱變形會在特定灰階下顯露內部應力光斑。",
        ar_check="・<b>光波導光柵乾涉條紋</b>：繞射光波導表面若微結構加工公差不佳，在 50% 灰色下會產生微弱的同心圓或平行乾涉條紋（Fringes）。<br/>・<b>Micro-OLED Mura 斑駁</b>：Micro-OLED 矽基驅動背板各像素電晶體閾值電壓微小差異會形成細微雲霧狀斑點（Mura）。",
        pass_criteria="<b>合格標準</b>：全平面的 50% 灰色均勻一致，無明顯條紋、色塊突兀跳躍或髒屏斑駁感。<br/><b>排查建議</b>：高階 AR 光機通常內建出廠『De-Mura』校正補償矩陣，若斑駁嚴重應更新固件校正檔。"
    ))

    story.extend(add_pattern_card(
        filename="11_Chromatic_Aberration_Convergence_1080p.png",
        title="11: 橫向色差與 RGB 三色收斂測試圖 (Chromatic Aberration & Convergence)",
        purpose="檢測光學鏡頭的<b>橫向色差（Lateral Chromatic Aberration / Color Fringing）</b>，以及多片式顯示系統（如 3LCD、3DLP、3-LCoS 投影機）的 <b>RGB 三色面板空間對齊收斂精度（Panel Convergence）</b>。",
        step_guide="1. 投射高對比 1 像素十字收斂線與黑白交錯棋盤區；<br/>2. 首先走近畫面觀察<b>正中心基準十字線</b>：白色 1 像素十字線邊緣是否有紅邊或藍邊（理想狀態下應完美重合為純白線）；<br/>3. 依序檢查<b>四個角落與邊緣十字線</b>：確認各色像素偏差距離；<br/>4. 檢視外圍的黑白小方格邊界有無嚴重的紫邊或彩虹色擴散。",
        proj_check="・<b>三片式光機對準</b>：3LCD 或三片式雷射投影機由紅、綠、藍三塊獨立晶片透過稜鏡合成，常因熱膨脹或震動導致晶片錯位。若中心或邊角出現 &gt; 0.5 像素錯位，字體邊緣將產生紅藍毛邊。<br/>・<b>投影鏡頭色散 (紫色/綠色散色)</b>：平價鏡頭在畫面極邊緣處，高對比白線會色散分離成一側紅色、一側青色。",
        ar_check="・<b>近眼透鏡與波導色差</b>：AR 鏡片（無論 Pancake、BirdBath 或波導）在邊緣視場不可避免存在橫向色散。檢查視野最邊角之白字是否分離成彩虹色光譜。<br/>・<b>色差軟體預補償</b>：觀察系統驅動是否對邊角 RGB 進行了逆向幾何反向扭曲補償。",
        pass_criteria="<b>合格標準</b>：畫面中心收斂偏差 &lt; 0.3 像素（肉眼無感知色邊）；畫面最邊緣收斂偏差 &lt; 0.8 像素。<br/><b>排查建議</b>：進入投影機選單尋找『面板對齊 / 像素調整 (Panel Alignment)』功能，以 1/16 像素精細度進行軟體數位分區對齊。"
    ))

    # ========================================================
    # SECTION 6: AR Text & PPD
    # ========================================================
    story.extend(add_pattern_card(
        filename="12_AR_Text_PPD_Legibility_1080p.png",
        title="12: AR 眼鏡微文字可讀性與角解析度測試圖 (AR Text Legibility & PPD)",
        purpose="專門為 AR 智慧眼鏡、抬頭顯示器（HUD）與近眼顯示器量身打造。以正顯（黑字白底）與反顯（白字黑底 - 標記 HUD 模式）自 28pt 遞減至 6pt 微小字體，綜合檢驗光機發光眩光溢出（Bloom/Glare）、鬼影（Ghosting）與角解析度（PPD）極限。",
        step_guide="1. 佩戴 AR 眼鏡或將近眼相機對準眼線，將本圖投射至虛擬螢幕；<br/>2. <b>測試下半部反顯模式（HUD 模式）</b>：白色文字在全黑背景下發光，觀察文字邊緣是否有發光向外擴散的光暈（Bloom）將相鄰筆畫糊住；<br/>3. 尋找肉眼能輕鬆閱讀的最小字號（通常 10pt/8pt/6pt 是分水嶺）；<br/>4. <b>測試邊界視角</b>：轉動眼球至視場邊界，確認邊界微文字是否因波導或透鏡模糊而失去可讀性；<br/>5. <b>對比上半部正顯模式</b>：評估在白色背景下文字筆畫的黑度與邊緣俐落感。",
        proj_check="・<b>簡報小字清晰度</b>：在商務或教學投影時，評估台下觀眾在遠距離能否清晰看見簡報中 8pt/10pt 的附註標註與細小符號。<br/>・<b>像素抖動對文字影響</b>：DLP 搖粒（XPR 抖動技術）或 LCD 像素柵格是否會讓 6pt 中文字體筆畫黏連。",
        ar_check="・<b>角解析度 (PPD = Pixels Per Degree) 實測</b>：AR 眼鏡視場角 FOV 越大，若總像素固定（如 1080p），PPD 會越低。PPD &gt; 45 才能輕鬆辨識 8pt 微文字；若 PPD &lt; 30，微小字體會嚴重鋸齒像素化。<br/>・<b>波導光學鬼影 (Ghost Images)</b>：高對比白字在光波導反射多次後，常在正下方或右方產生微弱的重影複製品，嚴重干擾文字閱讀。",
        pass_criteria="<b>合格標準</b>：反顯 HUD 模式下，10pt 中英文文字筆畫分明、無明顯眩光黏連；8pt 文字可辨認；光學鬼影亮度 &lt; 主影像之 5%。<br/><b>排查建議</b>：適度調低 AR 眼鏡顯示亮度以抑制發光眩光（Bloom）；字體渲染引擎應開啟灰階抗鋸齒（Anti-aliasing）。"
    ))

    # ========================================================
    # SECTION 7: Real-world Photographic
    # ========================================================
    story.append(Paragraph("第七章：真實世界攝影影像綜合驗證 (Real-World Verification)", style_h1))
    story.append(HRFlowable(width="100%", thickness=1, color=c_secondary, spaceAfter=8))
    
    real_intro = """
    幾何與色彩色條圖能提供客觀的校準基準，但最終人類大腦對顯示設備好壞的感受，仍取決於<b>自然連續調攝影場景（Continuous-tone Real-world Photography）</b>的綜合表現。
    本套件精選國際影像處理權威標準——<b>Kodak Lossless True Color 基準影像庫</b>中的四幅經典代表作（包含 13~16 號獨立大圖與 17 號 4合1 綜合圖）：
    """
    story.append(Paragraph(real_intro, style_body))
    story.append(Spacer(1, 4))

    # Embed 17_Photo_RealWorld_Composite_1080p.png
    thumb_comp = get_pdf_thumb("17_Photo_RealWorld_Composite_1080p.png")
    story.append(RLImage(thumb_comp, width=515, height=289))
    story.append(Spacer(1, 8))

    photo_details = [
        [Paragraph("<b>編號與影像</b>", style_body_bold), Paragraph("<b>主要評估面向與視覺特徵</b>", style_body_bold), Paragraph("<b>投影機 / AR 眼鏡觀察重點</b>", style_body_bold)],
        [
            Paragraph("<b>13. 肖像與膚色還原</b><br/>(kodim23)", style_body),
            Paragraph("・年輕女性自然細膩之臉部與肩膀膚色。<br/>・金黃色編織草帽之立體纖維紋理。<br/>・金剛鸚鵡羽毛極高飽和度純紅、鮮藍、黃綠色。", style_body),
            Paragraph("膚色不可泛黃或偏死白；金黃色草帽高光不可過曝；鸚鵡羽毛在展現強烈飽和度時，羽管與羽絨微小結構仍應根根分明而不糊成色塊。", style_body)
        ],
        [
            Paragraph("<b>14. 高頻建築紋理</b><br/>(kodim04)", style_body),
            Paragraph("・經典紅木木屋之密集屋頂木瓦片（Shingles）。<br/>・直條木板牆壁之深淺木紋與接縫陰影。<br/>・前景青翠灌木與綠葉之微小幾何細節。", style_body),
            Paragraph("考驗光學鏡頭與調光晶片的高頻解像極限。若對焦或鏡頭解像差，密集的屋頂瓦片會融為模糊的一片灰紅；若銳化過度，瓦片邊緣會產生刺眼雜訊。", style_body)
        ],
        [
            Paragraph("<b>15. 豐富粉彩與細微色差</b><br/>(kodim08)", style_body),
            Paragraph("・排列成堆的彩色粉筆、顏料粉與新鮮水果。<br/>・粉彩（Pastel colors）與超飽和純色共存。<br/>・極其接近的同色系細微色調階梯差異。", style_body),
            Paragraph("考驗顯示器色彩空間覆蓋率（Color Gamut）與色相辨別力。劣質設備會將多種不同色調的藍綠色或粉紅色合併成同一種單調顏色。", style_body)
        ],
        [
            Paragraph("<b>16. 藍天漸層與動態範圍</b><br/>(kodim21)", style_body),
            Paragraph("・白色燈塔聳立於海岸岩石，迎面陽光的高光反射。<br/>・由深藍漸變至天藍再到地平線淺藍之平滑天空。<br/>・海面波浪的動態反光與深色岩石陰影細節。", style_body),
            Paragraph("天空區域最易暴露色階斷層（Color Banding）與色彩噪點；白色燈塔向光面不可死白過曝，背光岩石細節不可沉入死黑，考驗綜合動態範圍。", style_body)
        ]
    ]
    t_photos = Table(photo_details, colWidths=[110, 200, 205])
    t_photos.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#E2E8F0")),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
    ]))
    story.append(t_photos)
    story.append(PageBreak())

    # ========================================================
    # SECTION 8: Checklist & Troubleshooting Matrix
    # ========================================================
    story.append(Paragraph("第八章：快速測試評分檢核表與異常排查矩陣", style_h1))
    story.append(HRFlowable(width="100%", thickness=1, color=c_secondary, spaceAfter=8))
    
    story.append(Paragraph("工程師或評測人員可依照下表之步驟，在 10~15 分鐘內對任何投影機或 AR 眼鏡完成標準化評估：", style_body))
    story.append(Spacer(1, 4))
    
    chk_data = [
        [Paragraph("<b>測試順序與維度</b>", style_body_bold), Paragraph("<b>使用測試圖檔</b>", style_body_bold), Paragraph("<b>標準驗收條件 (Pass Criteria)</b>", style_body_bold), Paragraph("<b>常見問題與排查/調整處方</b>", style_body_bold)],
        [
            Paragraph("Step 1<br/>點對點與聚焦", style_body),
            Paragraph("01 幾何對焦圖<br/>02 ISO12233", style_body),
            Paragraph("・外圍 1px 白框完整無裁切<br/>・中心與四角 Siemens 星圖收斂<br/>・1px 線對單線清晰無黏連", style_body),
            Paragraph("<b>問題</b>：線條模糊出現莫爾波紋。<br/><b>處方</b>：關閉訊號端超頻掃描(Overscan)，確認輸出解析度為原生 1080p 1:1。", style_body)
        ],
        [
            Paragraph("Step 2<br/>黑白階與對比", style_body),
            Paragraph("05 PLUGE 裁切<br/>03 ANSI 棋盤格", style_body),
            Paragraph("・暗室下 RGB 2~4 能區分<br/>・高光 RGB 250~253 能區分<br/>・棋盤格黑白交界無漫射光霧", style_body),
            Paragraph("<b>問題</b>：暗部細節死黑(Black Crush)。<br/><b>處方</b>：調高顯示器亮度設定；確認 HDMI RGB 範圍(Full/Limited)兩端匹配一致。", style_body)
        ],
        [
            Paragraph("Step 3<br/>灰階與 Gamma", style_body),
            Paragraph("04 32階灰階Gamma<br/>10 50%中性灰場", style_body),
            Paragraph("・32 階階梯肉眼全數可辨<br/>・連續漸層平滑無條紋斷層<br/>・全灰場無骯髒斑駁(Mura)", style_body),
            Paragraph("<b>問題</b>：漸層出現一圈圈階梯色彩斷層。<br/><b>處方</b>：檢查輸出端色深是否降至 6-bit；更換高頻寬優質 HDMI 2.0/2.1 線材。", style_body)
        ],
        [
            Paragraph("Step 4<br/>色彩與還原度", style_body),
            Paragraph("06 SMPTE 彩條<br/>07 24色卡<br/>08 六軸漸層", style_body),
            Paragraph("・膚色自然紅潤不偏青<br/>・六軸漸層頂端無提早截波<br/>・純藍模式下各色條亮度吻合", style_body),
            Paragraph("<b>問題</b>：人物膚色假白、鮮豔溢色。<br/><b>處方</b>：退出動態/鮮豔模式，切換至標準 Cinema / Filmmaker 模式，降低飽和度。", style_body)
        ],
        [
            Paragraph("Step 5<br/>均勻度與光學", style_body),
            Paragraph("09 100%純白場<br/>11 色差收斂圖", style_body),
            Paragraph("・ANSI 9 點均勻度 &gt; 80%<br/>・白場無黃斑/綠斑/塵點<br/>・白十字線邊緣無紅藍色邊", style_body),
            Paragraph("<b>問題</b>：四角亮度暗角嚴重、色溫左偏紅右偏藍。<br/><b>處方</b>：避免極端廣角端投影；AR 眼鏡需校準波導各色繞射調諧補償檔。", style_body)
        ],
        [
            Paragraph("Step 6<br/>AR 微文字與鬼影", style_body),
            Paragraph("12 AR文字PPD<br/>17 真實世界綜合", style_body),
            Paragraph("・HUD 反顯 10pt/8pt 文字分明<br/>・無強烈發光眩暈(Bloom)<br/>・照片毛髮與瓦片紋理細膩", style_body),
            Paragraph("<b>問題</b>：白字邊緣發光嚴重糊住字形筆畫。<br/><b>處方</b>：調降 Micro-OLED 驅動亮度至適當水平；開啟次像素字體抗鋸齒渲染。", style_body)
        ]
    ]
    t_chk = Table(chk_data, colWidths=[80, 95, 170, 170])
    t_chk.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#E2E8F0")),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
    ]))
    story.append(t_chk)
    story.append(Spacer(1, 15))
    
    concl = [
        [Paragraph("<b>總結與實務建議</b>", style_body_bold)],
        [Paragraph("顯示品質的量測與調校是一門平衡的藝術。投影機與 AR 眼鏡由於光學通路的限制（投影鏡頭、投射距離、環境反射光、光波導折射率、眼盒大小），無法像直視型 OLED 電視一般具備絕對完美的各項指標。然而，透過本套測試素材的系統化檢測，工程師與測試者能精確定位是『前端訊號設定』、『顯示驅動色階』還是『後端光學透鏡』造成的畫質瓶頸，進而針對亮度、對比、Gamma、飽和度與光學焦點進行最佳化匹配。", style_body)]
    ]
    t_concl = Table(concl, colWidths=[515])
    t_concl.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), colors.HexColor("#F7FAFC")),
        ('BOX', (0, 0), (-1, -1), 1, colors.HexColor("#CBD5E0")),
        ('TOPPADDING', (0, 0), (-1, -1), 6),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
        ('LEFTPADDING', (0, 0), (-1, -1), 10),
        ('RIGHTPADDING', (0, 0), (-1, -1), 10),
    ]))
    story.append(t_concl)

    # Build Document with NumberedCanvas
    doc.build(story, canvasmaker=NumberedCanvas)
    print(f"PDF successfully built: {PDF_PATH}")

if __name__ == "__main__":
    build_pdf()
