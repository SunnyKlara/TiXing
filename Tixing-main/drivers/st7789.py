# drivers/st7789.py — ST7789 SPI 驱动（精简版）
# 针对 RP2040 + 240×320 面板，rotation=1 横屏 284×76
# 引脚: GPIO0=DC, GPIO1=CS, GPIO2=SCK, GPIO3=MOSI, GPIO5=RST

import struct

try:
    from machine import Pin, SPI
    from time import sleep_ms
except ImportError:
    # PC 端测试兼容
    sleep_ms = lambda ms: None

# ST7789 命令
_SWRESET = const(0x01)
_SLPOUT  = const(0x11)
_NORON   = const(0x13)
_INVON   = const(0x21)
_DISPON  = const(0x29)
_CASET   = const(0x2A)
_RASET   = const(0x2B)
_RAMWR   = const(0x2C)
_COLMOD  = const(0x3A)
_MADCTL  = const(0x36)

# 填充缓冲区大小（像素数）
_BUF_SIZE = const(256)


class ST7789:
    """ST7789 SPI 驱动，精简版。

    固定配置: 240×320 面板, rotation=1 横屏 284×76,
    BGR 色序, 16bit RGB565, 反色开启。
    """

    WIDTH = 284
    HEIGHT = 76

    def __init__(self, spi=None, dc=None, cs=None, rst=None,
                 x_offset=18, y_offset=82, bl=None):
        """初始化 SPI 总线和引脚。

        Args:
            spi: machine.SPI 实例，默认 SPI(0, 20MHz)
            dc:  DC 引脚，默认 GPIO0
            cs:  CS 引脚，默认 GPIO1
            rst: RST 引脚，默认 GPIO5
            x_offset: 列偏移（rotation=1 时）
            y_offset: 行偏移（rotation=1 时）
            bl:  背光引脚号（如 GPIO4），None 表示无背光控制
        """
        if spi is None:
            spi = SPI(0, baudrate=62_500_000,
                       sck=Pin(2), mosi=Pin(3), miso=None)
        if dc is None:
            dc = Pin(0, Pin.OUT)
        if cs is None:
            cs = Pin(1, Pin.OUT)
        if rst is None:
            rst = Pin(5, Pin.OUT)

        self.spi = spi
        self.dc = dc
        self.cs = cs
        self.rst = rst
        self.x_off = x_offset
        self.y_off = y_offset

        # 背光控制（可选）
        if bl is not None:
            self.bl = Pin(bl, Pin.OUT)
            self.bl.off()  # 初始化期间关闭背光
        else:
            self.bl = None

        # 预分配命令缓冲
        self._cmd1 = bytearray(1)
        self._pos_buf = bytearray(4)

    def init(self, logo_data=None, logo_w=0, logo_h=0):
        """执行硬件复位 + 初始化序列。

        Args:
            logo_data: 可选 RGB565 Logo 数据，在 DISPON 前写入 RAM
            logo_w, logo_h: Logo 尺寸
        """
        self._hard_reset()
        self._init_seq(logo_data, logo_w, logo_h)

    def _hard_reset(self):
        """硬件复位时序。"""
        self.cs.off()
        self.rst.on()
        sleep_ms(10)
        self.rst.off()
        sleep_ms(10)
        self.rst.on()
        sleep_ms(120)
        self.cs.on()

    def _cmd(self, cmd, data=None):
        """发送命令 + 可选数据。"""
        self.cs.off()
        self.dc.off()
        self._cmd1[0] = cmd
        self.spi.write(self._cmd1)
        if data is not None:
            self.dc.on()
            self.spi.write(data)
        self.cs.on()

    def _init_seq(self, logo_data=None, logo_w=0, logo_h=0):
        """ST7789 初始化序列（rotation=1, BGR, 16bit, INVON）。"""
        self._cmd(_SWRESET)
        sleep_ms(50)

        self._cmd(_SLPOUT)
        sleep_ms(50)

        self._cmd(0x28)  # DISPOFF — 保持关闭，防止白屏

        self._cmd(_COLMOD, b'\x05')

        # MADCTL: rotation=1 → MX|MV = 0x60, RGB顺序(无BGR位)
        self._cmd(_MADCTL, b'\x60')

        self._cmd(_NORON)

        # 在 DISPON 之前写入 RAM：Logo 或黑色填充
        if logo_data and logo_w > 0 and logo_h > 0:
            self.blit_block(logo_data, 0, 0, logo_w, logo_h)
        else:
            self.fill(0x0000)

        # RAM 已就绪，安全开启显示
        self._cmd(_DISPON)

        # 打开背光（如果有）
        if self.bl is not None:
            self.bl.on()

    def set_window(self, x0, y0, x1, y1):
        """设置写入窗口区域（含偏移校正）。

        Args:
            x0, y0: 左上角坐标
            x1, y1: 右下角坐标（含）
        """
        buf = self._pos_buf

        # CASET: 列地址
        struct.pack_into('>HH', buf, 0,
                         x0 + self.x_off, x1 + self.x_off)
        self._cmd(_CASET, buf)

        # RASET: 行地址
        struct.pack_into('>HH', buf, 0,
                         y0 + self.y_off, y1 + self.y_off)
        self._cmd(_RASET, buf)

        # 准备写入 RAM
        self._cmd(_RAMWR)

    def write_data(self, buf):
        """批量写入像素数据到 RAM（在 set_window 之后调用）。

        Args:
            buf: RGB565 像素数据 bytearray/bytes
        """
        self.cs.off()
        self.dc.on()
        self.spi.write(buf)
        self.cs.on()

    def fill_rect(self, x, y, w, h, color):
        """矩形填充。

        Args:
            x, y: 左上角坐标
            w, h: 宽度和高度
            color: RGB565 颜色值（整数）
        """
        if w <= 0 or h <= 0:
            return
        self.set_window(x, y, x + w - 1, y + h - 1)

        # 像素字节（大端 RGB565）
        pixel = struct.pack('>H', color)
        total = w * h
        chunks, rest = divmod(total, _BUF_SIZE)

        self.cs.off()
        self.dc.on()
        if chunks:
            data = pixel * _BUF_SIZE
            for _ in range(chunks):
                self.spi.write(data)
        if rest:
            self.spi.write(pixel * rest)
        self.cs.on()

    def fill(self, color):
        """全屏填充。

        Args:
            color: RGB565 颜色值（整数）
        """
        self.fill_rect(0, 0, self.WIDTH, self.HEIGHT, color)

    def blit_block(self, buf, x, y, w, h):
        """一次性写入整块像素数据（单次 set_window + 单次 write）。

        比逐行 blit 减少 (h-1) 次 SPI 事务开销。
        buf 必须包含 w*h*2 字节 RGB565 数据。
        """
        if w <= 0 or h <= 0:
            return
        self.set_window(x, y, x + w - 1, y + h - 1)
        self.cs.off()
        self.dc.on()
        self.spi.write(buf)
        self.cs.on()
