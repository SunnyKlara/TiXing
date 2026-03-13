# screens/boot.py — UI0: 开机动画界面

from screens import Screen
from ui.theme import SCREEN_W, SCREEN_H, WHITE, BLACK, ACCENT
from ui.font import get_font

try:
    from ui.bitmap import Bitmap
except ImportError:
    Bitmap = None


class BootScreen(Screen):
    """开机动画：Logo + 品牌文字 + COB 灯带动画。"""

    def __init__(self, ctx):
        super().__init__(ctx)
        self._logo = None

    def on_enter(self):
        # 尝试加载 Logo 位图
        if Bitmap is not None:
            try:
                self._logo = Bitmap('assets/logo.bin')
            except OSError:
                self._logo = None

    def draw_full(self, r):
        r.clear(BLACK)

        if self._logo:
            # 居中显示 Logo
            lx = (SCREEN_W - self._logo.width) // 2
            ly = (SCREEN_H - self._logo.height) // 2 - 8
            self._logo.blit_to(r, lx, ly)
        else:
            # 回退：纯文字 Logo
            font = get_font(16)
            text = "TURBO"
            tw = font.text_width(text)
            x = (SCREEN_W - tw) // 2
            y = (SCREEN_H - font.height) // 2 - 4
            r.draw_text(font, text, x, y, ACCENT, BLACK)

        # 底部品牌文字
        font8 = get_font(8)
        brand = "PicoTurbo v2.0"
        bw = font8.text_width(brand)
        r.draw_text(font8, brand, (SCREEN_W - bw) // 2,
                    SCREEN_H - 12, WHITE, BLACK)

    def draw_update(self, r):
        pass  # 开机动画不需要局部刷新

    def on_exit(self):
        self._logo = None  # 释放位图内存
