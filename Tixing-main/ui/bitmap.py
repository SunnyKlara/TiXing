# ui/bitmap.py — RGB565 位图和图标集加载器

try:
    from ustruct import unpack
except ImportError:
    from struct import unpack


class Bitmap:
    """RGB565 位图加载器。

    .bin 格式: Header(4B): width(2B LE) + height(2B LE)
    像素数据: width * height * 2 字节 (RGB565)
    """
    __slots__ = ('width', 'height', '_data')

    def __init__(self, path):
        with open(path, 'rb') as f:
            hdr = f.read(4)
            self.width, self.height = unpack('<HH', hdr)
            self._data = f.read()

    def blit_to(self, renderer, x, y):
        """将位图绘制到屏幕指定位置。"""
        renderer.blit_bitmap(self._data, x, y, self.width, self.height)


class IconSheet:
    """图标集：多个等尺寸图标打包在一个文件中。

    .bin 格式: Header(6B): icon_w(2B LE) + icon_h(2B LE) + count(2B LE)
    图标数据: count × (icon_w × icon_h × 2) 字节
    """
    __slots__ = ('icon_w', 'icon_h', 'count', '_data', '_icon_size')

    def __init__(self, path):
        with open(path, 'rb') as f:
            hdr = f.read(6)
            self.icon_w, self.icon_h, self.count = unpack('<HHH', hdr)
            self._icon_size = self.icon_w * self.icon_h * 2
            self._data = f.read()

    def blit_icon(self, renderer, index, x, y):
        """绘制指定索引的图标。"""
        if index < 0 or index >= self.count:
            return
        off = index * self._icon_size
        data = memoryview(self._data)[off:off + self._icon_size]
        renderer.blit_bitmap(data, x, y, self.icon_w, self.icon_h)
