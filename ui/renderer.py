# ui/renderer.py — 渲染管线：行缓冲 + 文本/图形绘制
# 284×8 行缓冲 = 4,544 字节，适配 RP2040 内存约束

try:
    from ustruct import pack
except ImportError:
    from struct import pack

from ui.theme import SCREEN_W


class Renderer:
    """渲染管线：行缓冲 + 基础绘制接口。"""

    LINE_BUF_ROWS = 8

    def __init__(self, display):
        self._display = display
        self._buf = bytearray(SCREEN_W * self.LINE_BUF_ROWS * 2)

    def clear(self, color=0x0000):
        """全屏清除。"""
        self._display.fill(color)

    def fill_rect(self, x, y, w, h, color):
        """填充矩形（直接写屏）。"""
        self._display.fill_rect(x, y, w, h, color)

    def blit_bitmap(self, data, x, y, w, h):
        """将 RGB565 位图数据写入屏幕。"""
        self._display.blit(data, x, y, w, h)

    def draw_text(self, font, text, x, y, fg, bg=0x0000):
        """使用位图字体绘制文本，返回绘制宽度。

        整串文字合并到一个缓冲区, 单次 blit 写入。
        支持 4-bit 抗锯齿 (alpha混合) 和 1-bit 字体。
        """
        if not text:
            return 0
        fw = font.width
        fh = font.height
        bpp = getattr(font, '_bpp', 1)
        fg_hi = (fg >> 8) & 0xFF
        fg_lo = fg & 0xFF
        bg_hi = (bg >> 8) & 0xFF
        bg_lo = bg & 0xFF
        text_len = len(text)
        total_w = text_len * fw

        # 尝试用整块缓冲 (total_w * fh * 2 字节)
        # 如果太大就回退到逐字符
        block_size = total_w * fh * 2
        use_block = block_size <= len(self._buf)

        if use_block:
            buf = self._buf
            # 逐字符渲染到连续缓冲区 (逐行交错排列)
            for i, ch in enumerate(text):
                glyph = font.get_glyph(ch)
                if bpp == 4:
                    fg_r = (fg >> 11) & 0x1F
                    fg_g = (fg >> 5) & 0x3F
                    fg_b = fg & 0x1F
                    bg_r = (bg >> 11) & 0x1F
                    bg_g = (bg >> 5) & 0x3F
                    bg_b = bg & 0x1F
                    half_w = fw // 2
                    for row in range(fh):
                        idx = (row * total_w + i * fw) * 2
                        if glyph is None:
                            for _ in range(fw):
                                buf[idx] = bg_hi
                                buf[idx + 1] = bg_lo
                                idx += 2
                        else:
                            gi = row * half_w
                            for col in range(half_w):
                                byte = glyph[gi + col]
                                for v in ((byte >> 4) & 0xF, byte & 0xF):
                                    if v == 0:
                                        buf[idx] = bg_hi
                                        buf[idx + 1] = bg_lo
                                    elif v >= 15:
                                        buf[idx] = fg_hi
                                        buf[idx + 1] = fg_lo
                                    else:
                                        iv = 15 - v
                                        r = (fg_r * v + bg_r * iv) // 15
                                        g = (fg_g * v + bg_g * iv) // 15
                                        b = (fg_b * v + bg_b * iv) // 15
                                        c = (r << 11) | (g << 5) | b
                                        buf[idx] = (c >> 8) & 0xFF
                                        buf[idx + 1] = c & 0xFF
                                    idx += 2
                else:
                    col_bytes = (fh + 7) // 8
                    for row in range(fh):
                        idx = (row * total_w + i * fw) * 2
                        if glyph is None:
                            for _ in range(fw):
                                buf[idx] = bg_hi
                                buf[idx + 1] = bg_lo
                                idx += 2
                        else:
                            for col in range(fw):
                                byte_idx = col * col_bytes + row // 8
                                bit = (glyph[byte_idx] >> (row % 8)) & 1
                                if bit:
                                    buf[idx] = fg_hi
                                    buf[idx + 1] = fg_lo
                                else:
                                    buf[idx] = bg_hi
                                    buf[idx + 1] = bg_lo
                                idx += 2

            # 单次 blit 整串文字
            blit_block = getattr(self._display, 'blit_block', None)
            if blit_block:
                blit_block(buf[:block_size], x, y, total_w, fh)
            else:
                self._display.blit(buf[:block_size], x, y, total_w, fh)
        else:
            # 回退: 逐字符 blit (文本太长放不下缓冲区)
            for i, ch in enumerate(text):
                glyph = font.get_glyph(ch)
                cx = x + i * fw
                if glyph is None:
                    self._display.fill_rect(cx, y, fw, fh, bg)
                    continue
                buf = self._buf
                idx = 0
                if bpp == 4:
                    fg_r = (fg >> 11) & 0x1F
                    fg_g = (fg >> 5) & 0x3F
                    fg_b = fg & 0x1F
                    bg_r = (bg >> 11) & 0x1F
                    bg_g = (bg >> 5) & 0x3F
                    bg_b = bg & 0x1F
                    half_w = fw // 2
                    for row in range(fh):
                        gi = row * half_w
                        for col in range(half_w):
                            byte = glyph[gi + col]
                            for v in ((byte >> 4) & 0xF, byte & 0xF):
                                if v == 0:
                                    buf[idx] = bg_hi
                                    buf[idx + 1] = bg_lo
                                elif v >= 15:
                                    buf[idx] = fg_hi
                                    buf[idx + 1] = fg_lo
                                else:
                                    iv = 15 - v
                                    r = (fg_r * v + bg_r * iv) // 15
                                    g = (fg_g * v + bg_g * iv) // 15
                                    b = (fg_b * v + bg_b * iv) // 15
                                    c = (r << 11) | (g << 5) | b
                                    buf[idx] = (c >> 8) & 0xFF
                                    buf[idx + 1] = c & 0xFF
                                idx += 2
                else:
                    col_bytes = (fh + 7) // 8
                    for row in range(fh):
                        for col in range(fw):
                            byte_idx = col * col_bytes + row // 8
                            bit = (glyph[byte_idx] >> (row % 8)) & 1
                            if bit:
                                buf[idx] = fg_hi
                                buf[idx + 1] = fg_lo
                            else:
                                buf[idx] = bg_hi
                                buf[idx + 1] = bg_lo
                            idx += 2
                self._display.blit(buf[:fw * fh * 2], cx, y, fw, fh)

        return total_w

    def draw_gradient_text(self, font, text, x, y, c1, c2, bg=0x0000):
        """逐字符渐变色绘制文本（仿 Logo 青蓝渐变风格）。

        c1, c2: RGB565 起止颜色，沿文字方向线性插值。
        """
        n = max(len(text) - 1, 1)
        # 拆解 RGB565 分量
        r1 = (c1 >> 11) & 0x1F
        g1 = (c1 >> 5) & 0x3F
        b1 = c1 & 0x1F
        r2 = (c2 >> 11) & 0x1F
        g2 = (c2 >> 5) & 0x3F
        b2 = c2 & 0x1F
        for i, ch in enumerate(text):
            t = i * 256 // n if n > 0 else 0
            it = 256 - t
            r = (r1 * it + r2 * t) >> 8
            g = (g1 * it + g2 * t) >> 8
            b = (b1 * it + b2 * t) >> 8
            fg = (r << 11) | (g << 5) | b
            self.draw_text(font, ch, x + i * font.width, y, fg, bg)
        return len(text) * font.width

    def draw_number(self, value, cx, cy, font, fg, bg=0x0000):
        """居中绘制数字（大数字显示专用）。

        固定 4 字符宽度（最多 "100%"），避免位数变化时残影。
        """
        text = str(value)
        # 固定宽度：右对齐补空格，确保覆盖旧内容
        padded = ' ' * (4 - len(text)) + text if len(text) < 4 else text
        tw = font.text_width(padded)
        x = cx - tw // 2
        y = cy - font.height // 2
        self.draw_text(font, padded, x, y, fg, bg)

    def draw_hbar(self, x, y, w, h, pct, fg, bg):
        """绘制水平进度条（圆角胶囊形）。

        pct: 0-100，前景宽度 = w * pct // 100
        """
        pct = max(0, min(100, pct))
        r = h // 2  # 圆角半径

        # 背景
        self._display.fill_rect(x + r, y, w - 2 * r, h, bg)
        self._draw_half_circle(x + r, y + r, r, bg)
        self._draw_half_circle(x + w - r - 1, y + r, r, bg, right=True)

        # 前景
        fw = w * pct // 100
        if fw > 0:
            if fw < 2 * r:
                fw = max(fw, 2)
            inner = max(0, fw - 2 * r)
            self._display.fill_rect(x + r, y, inner, h, fg)
            self._draw_half_circle(x + r, y + r, r, fg)
            if fw > 2 * r:
                self._draw_half_circle(x + fw - r - 1, y + r, r, fg, right=True)

    def _draw_half_circle(self, cx, cy, r, color, right=False):
        """绘制半圆（用于胶囊进度条两端）。"""
        for dy in range(-r, r + 1):
            # 简化：用矩形近似圆弧
            dx = int((r * r - dy * dy) ** 0.5) if r > 0 else 0
            py = cy + dy
            if right:
                self._display.fill_rect(cx, py, dx + 1, 1, color)
            else:
                self._display.fill_rect(cx - dx, py, dx + 1, 1, color)

    def draw_gradient_bar(self, x, y, w, h, c1, c2):
        """绘制渐变色条（RGB888 元组输入，逐列渐变）。"""
        r1, g1, b1 = c1
        r2, g2, b2 = c2
        for col in range(w):
            t = col * 256 // max(w - 1, 1)
            r = (r1 * (256 - t) + r2 * t) >> 8
            g = (g1 * (256 - t) + g2 * t) >> 8
            b = (b1 * (256 - t) + b2 * t) >> 8
            c565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            self._display.fill_rect(x + col, y, 1, h, c565)

    def draw_dot(self, cx, cy, r, color):
        """绘制实心圆点。"""
        for dy in range(-r, r + 1):
            dx = int((r * r - dy * dy) ** 0.5)
            self._display.fill_rect(cx - dx, cy + dy, 2 * dx + 1, 1, color)
