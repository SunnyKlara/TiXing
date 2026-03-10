# ui/icon.py — 4-bit灰度RLE图标渲染器
# 支持抗锯齿alpha混合, 适配76×284小屏

try:
    from ustruct import unpack
except ImportError:
    from struct import unpack


class AlphaIcon:
    """单个4-bit灰度RLE压缩图标。"""
    __slots__ = ('width', 'height', '_data')

    def __init__(self, path):
        with open(path, 'rb') as f:
            hdr = f.read(4)
            self.width = hdr[0]
            self.height = hdr[1]
            self._data = f.read()

    def draw(self, display, x, y, fg=0xFFFF, bg=0x0000):
        """渲染图标, alpha混合前景色和背景色。"""
        _render_rle(display, self._data, 0, self.width, self.height,
                    x, y, fg, bg)


class AlphaIconSheet:
    """4-bit灰度RLE图标集。"""
    __slots__ = ('icon_w', 'icon_h', 'count', '_data', '_offsets')

    def __init__(self, path):
        with open(path, 'rb') as f:
            hdr = f.read(6)
            self.icon_w = hdr[0]
            self.icon_h = hdr[1]
            self.count = hdr[2]
            self._offsets = []
            for _ in range(self.count):
                ob = f.read(2)
                self._offsets.append(ob[0] | (ob[1] << 8))
            self._data = f.read()

    def draw(self, display, index, x, y, fg=0xFFFF, bg=0x0000):
        """渲染指定索引的图标。"""
        if index < 0 or index >= self.count:
            return
        _render_rle(display, self._data, self._offsets[index],
                    self.icon_w, self.icon_h, x, y, fg, bg)


def _render_rle(display, data, offset, w, h, x, y, fg, bg):
    """RLE解码 + alpha混合渲染核心函数。

    逐行解码, 每行blit一次, 最小化SPI调用。
    """
    # 预计算前景/背景RGB分量 (RGB565)
    fg_r = (fg >> 11) & 0x1F
    fg_g = (fg >> 5) & 0x3F
    fg_b = fg & 0x1F
    bg_r = (bg >> 11) & 0x1F
    bg_g = (bg >> 5) & 0x3F
    bg_b = bg & 0x1F

    # 预计算常用颜色的高低字节
    fgh = (fg >> 8) & 0xFF
    fgl = fg & 0xFF
    bgh = (bg >> 8) & 0xFF
    bgl = bg & 0xFF

    buf = bytearray(w * 2)
    di = offset
    dlen = len(data)

    # 全局像素计数器 (跨行连续)
    total_px = w * h
    px_done = 0
    run_remain = 0  # 当前RLE run剩余像素数
    run_val = 0     # 当前RLE run的灰度值

    for row in range(h):
        bi = 0
        col = 0

        while col < w:
            # 如果有上一个run的剩余
            if run_remain > 0:
                actual = min(run_remain, w - col)
                _fill_pixels(buf, bi, actual, run_val,
                             fg_r, fg_g, fg_b, bg_r, bg_g, bg_b,
                             fgh, fgl, bgh, bgl)
                bi += actual * 2
                col += actual
                run_remain -= actual
                px_done += actual
                continue

            # 读取新的RLE run
            if di >= dlen:
                # 数据耗尽, 填背景
                while col < w:
                    buf[bi] = bgh
                    buf[bi + 1] = bgl
                    bi += 2
                    col += 1
                break

            count = data[di]
            if count == 0:
                # 结束标记
                while col < w:
                    buf[bi] = bgh
                    buf[bi + 1] = bgl
                    bi += 2
                    col += 1
                di += 1
                break

            val = data[di + 1]
            di += 2

            actual = min(count, w - col)
            _fill_pixels(buf, bi, actual, val,
                         fg_r, fg_g, fg_b, bg_r, bg_g, bg_b,
                         fgh, fgl, bgh, bgl)
            bi += actual * 2
            col += actual
            px_done += actual

            if actual < count:
                run_remain = count - actual
                run_val = val

        # blit这一行
        display.blit(buf, x, y + row, w, 1)


def _fill_pixels(buf, bi, count, val,
                 fg_r, fg_g, fg_b, bg_r, bg_g, bg_b,
                 fgh, fgl, bgh, bgl):
    """填充count个像素到buf, 从bi位置开始。"""
    if val == 0:
        for _ in range(count):
            buf[bi] = bgh
            buf[bi + 1] = bgl
            bi += 2
    elif val >= 15:
        for _ in range(count):
            buf[bi] = fgh
            buf[bi + 1] = fgl
            bi += 2
    else:
        a = val
        ia = 15 - a
        r = (fg_r * a + bg_r * ia) // 15
        g = (fg_g * a + bg_g * ia) // 15
        b = (fg_b * a + bg_b * ia) // 15
        c = (r << 11) | (g << 5) | b
        ch = (c >> 8) & 0xFF
        cl = c & 0xFF
        for _ in range(count):
            buf[bi] = ch
            buf[bi + 1] = cl
            bi += 2
