# hal/display.py — ST7789 显示封装
# 提供统一的绘制接口，UI 层无需关心 SPI 细节

from drivers.st7789 import ST7789


class Display:
    """ST7789 显示封装，管理 SPI 总线和局部刷新。"""

    WIDTH = 284
    HEIGHT = 76

    def __init__(self):
        self._st = ST7789()
        self._st.init()

    def fill(self, color):
        """全屏填充 RGB565 颜色。"""
        self._st.fill(color)

    def fill_rect(self, x, y, w, h, color):
        """矩形填充 RGB565 颜色。"""
        self._st.fill_rect(x, y, w, h, color)

    def blit(self, buf, x, y, w, h):
        """将 RGB565 缓冲区写入指定区域。"""
        self._st.set_window(x, y, x + w - 1, y + h - 1)
        self._st.write_data(buf)

    def hline(self, x, y, w, color):
        """水平线。"""
        self._st.fill_rect(x, y, w, 1, color)

    def vline(self, x, y, h, color):
        """垂直线。"""
        self._st.fill_rect(x, y, 1, h, color)

    def pixel(self, x, y, color):
        """单像素。"""
        self._st.fill_rect(x, y, 1, 1, color)
