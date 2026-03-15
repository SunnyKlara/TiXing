# boot.py — MicroPython 最早执行的文件
# 在 main.py 之前尽早初始化屏幕，写入 Logo 消除白屏
# 注意: 这里不能有复杂 import，要尽量快

import struct
from machine import Pin, SPI

# 最小化 ST7789 初始化 — 内联实现，不 import 任何项目模块
spi = SPI(0, baudrate=62_500_000, sck=Pin(2), mosi=Pin(3), miso=None)
dc = Pin(0, Pin.OUT)
cs = Pin(1, Pin.OUT)
rst = Pin(5, Pin.OUT)

_cmd1 = bytearray(1)
_pos = bytearray(4)
X_OFF = 18
Y_OFF = 82

def _cmd(c, data=None):
    cs.off(); dc.off()
    _cmd1[0] = c; spi.write(_cmd1)
    if data:
        dc.on(); spi.write(data)
    cs.on()

# 硬件复位
cs.off(); rst.on()
from time import sleep_ms
sleep_ms(10); rst.off(); sleep_ms(10); rst.on(); sleep_ms(120)
cs.on()

# 初始化序列 — DISPOFF 先行
_cmd(0x01); sleep_ms(50)       # SWRESET
_cmd(0x11); sleep_ms(50)       # SLPOUT
_cmd(0x28)                     # DISPOFF
_cmd(0x3A, b'\x05')            # COLMOD 16bit
_cmd(0x36, b'\x60')            # MADCTL rotation=1
_cmd(0x13)                     # NORON

# 加载 Logo 写入 RAM
try:
    with open('assets/logo.bin', 'rb') as f:
        hdr = f.read(4)
        w, h = struct.unpack('<HH', hdr)
        logo = f.read()
    # set_window
    struct.pack_into('>HH', _pos, 0, X_OFF, X_OFF + w - 1)
    _cmd(0x2A, _pos)
    struct.pack_into('>HH', _pos, 0, Y_OFF, Y_OFF + h - 1)
    _cmd(0x2B, _pos)
    _cmd(0x2C)
    # write data
    cs.off(); dc.on(); spi.write(logo); cs.on()
    del logo
except:
    # 填黑
    struct.pack_into('>HH', _pos, 0, X_OFF, X_OFF + 283)
    _cmd(0x2A, _pos)
    struct.pack_into('>HH', _pos, 0, Y_OFF, Y_OFF + 75)
    _cmd(0x2B, _pos)
    _cmd(0x2C)
    blk = b'\x00\x00' * 256
    cs.off(); dc.on()
    for _ in range(284 * 76 // 256):
        spi.write(blk)
    spi.write(b'\x00\x00' * (284 * 76 % 256))
    cs.on()

# RAM 就绪，开显示
_cmd(0x29)  # DISPON

# 清理临时变量，不污染 main.py 的命名空间
del _cmd, _cmd1, _pos, dc, cs, rst, spi, struct, sleep_ms, X_OFF, Y_OFF
import gc; gc.collect()
