# ui/font.py — 位图字体引擎
# 支持 4-bit 抗锯齿字体 (16级灰度) 和 1-bit 回退字体

try:
    from ustruct import unpack
except ImportError:
    from struct import unpack


class Font:
    """4-bit 抗锯齿位图字体加载器。

    .bin 格式 v2: Header(5B) = width + height + first_char + char_count + bpp
    字形: 4-bit 逐行, 每字节2像素 (高4位=左, 低4位=右)
    兼容旧 1-bit 格式 (4B header, bpp=1)
    """
    __slots__ = ('width', 'height', '_first', '_count', '_data',
                 '_glyph_size', '_bpp')

    def __init__(self, path):
        with open(path, 'rb') as f:
            hdr = f.read(5)
            self.width = hdr[0]
            self.height = hdr[1]
            self._first = hdr[2]
            self._count = hdr[3]
            if len(hdr) >= 5 and hdr[4] == 4:
                self._bpp = 4
                self._glyph_size = self.width * self.height // 2
                self._data = f.read()
            else:
                # 旧 1-bit 格式兼容
                self._bpp = 1
                self._glyph_size = self.width * ((self.height + 7) // 8)
                # 第5字节可能是字形数据的一部分，需要回退
                rest = f.read()
                self._data = bytes([hdr[4]]) + rest if len(hdr) >= 5 else rest

    def get_glyph(self, ch):
        idx = ord(ch) - self._first
        if idx < 0 or idx >= self._count:
            return None
        off = idx * self._glyph_size
        return memoryview(self._data)[off:off + self._glyph_size]

    def text_width(self, text):
        return len(text) * self.width


# 内置 8x8 回退字体（1-bit, ASCII 32-126）
_BUILTIN_8X8 = None

def _create_builtin_8px():
    global _BUILTIN_8X8
    if _BUILTIN_8X8 is not None:
        return _BUILTIN_8X8

    class BuiltinFont:
        __slots__ = ('width', 'height', '_first', '_count', '_data',
                     '_glyph_size', '_bpp')
        def __init__(self):
            self.width = 8
            self.height = 8
            self._first = 32
            self._count = 95
            self._glyph_size = 8
            self._bpp = 1
            self._data = bytearray(95 * 8)
            digits = [
                bytes([0x3E,0x51,0x49,0x45,0x3E,0x00,0x00,0x00]),
                bytes([0x00,0x42,0x7F,0x40,0x00,0x00,0x00,0x00]),
                bytes([0x42,0x61,0x51,0x49,0x46,0x00,0x00,0x00]),
                bytes([0x21,0x41,0x45,0x4B,0x31,0x00,0x00,0x00]),
                bytes([0x18,0x14,0x12,0x7F,0x10,0x00,0x00,0x00]),
                bytes([0x27,0x45,0x45,0x45,0x39,0x00,0x00,0x00]),
                bytes([0x3C,0x4A,0x49,0x49,0x30,0x00,0x00,0x00]),
                bytes([0x01,0x71,0x09,0x05,0x03,0x00,0x00,0x00]),
                bytes([0x36,0x49,0x49,0x49,0x36,0x00,0x00,0x00]),
                bytes([0x06,0x49,0x49,0x29,0x1E,0x00,0x00,0x00]),
            ]
            for i, d in enumerate(digits):
                off = (48 - 32 + i) * 8
                self._data[off:off+8] = d
            pct = bytes([0x23,0x13,0x08,0x64,0x62,0x00,0x00,0x00])
            off = (37 - 32) * 8
            self._data[off:off+8] = pct

        def get_glyph(self, ch):
            idx = ord(ch) - self._first
            if idx < 0 or idx >= self._count:
                return None
            off = idx * self._glyph_size
            return memoryview(self._data)[off:off + self._glyph_size]

        def text_width(self, text):
            return len(text) * self.width

    _BUILTIN_8X8 = BuiltinFont()
    return _BUILTIN_8X8


# 字体缓存
_font_cache = {}

def get_font(size):
    """获取指定尺寸字体（带缓存）。支持 8(内置), 16, 32。"""
    if size not in _font_cache:
        if size == 8:
            _font_cache[8] = _create_builtin_8px()
        else:
            try:
                _font_cache[size] = Font('assets/font_{}.bin'.format(size))
            except OSError:
                print('WARN: font_{}.bin not found, fallback to 8px'.format(size))
                _font_cache[size] = _create_builtin_8px()
    return _font_cache[size]
