# test_black.py — 验证 MADCTL=0x60 (RGB, 无BGR) + 无INVON
# 用法: py -m mpremote connect COM6 run test_black.py

from machine import Pin, SPI
from time import sleep_ms
import struct

spi = SPI(0, baudrate=20_000_000, sck=Pin(2), mosi=Pin(3), miso=None)
dc = Pin(0, Pin.OUT)
cs = Pin(1, Pin.OUT)
rst = Pin(5, Pin.OUT)

def cmd(c, data=None):
    cs.off()
    dc.off()
    spi.write(bytes([c]))
    if data:
        dc.on()
        spi.write(data)
    cs.on()

def fill(color):
    buf4 = bytearray(4)
    struct.pack_into('>HH', buf4, 0, 18, 301)
    cmd(0x2A, buf4)
    struct.pack_into('>HH', buf4, 0, 82, 157)
    cmd(0x2B, buf4)
    cmd(0x2C)
    cs.off()
    dc.on()
    px = struct.pack('>H', color)
    chunk = px * 256
    total = 284 * 76
    for _ in range(total // 256):
        spi.write(chunk)
    spi.write(px * (total % 256))
    cs.on()

# 硬件复位
cs.off()
rst.on(); sleep_ms(10)
rst.off(); sleep_ms(10)
rst.on(); sleep_ms(120)
cs.on()

# 初始化 - MADCTL=0x60 (RGB顺序), 无INVON
cmd(0x01); sleep_ms(150)
cmd(0x11); sleep_ms(120)
cmd(0x3A, b'\x05'); sleep_ms(10)
cmd(0x36, b'\x60')  # MX|MV, RGB顺序
cmd(0x13); sleep_ms(10)
cmd(0x29); sleep_ms(10)

# 黑色
fill(0x0000)
print("1) BLACK 0x0000 (5秒)")
sleep_ms(5000)

# 红色
fill(0xF800)
print("2) RED 0xF800 - 应该是红色 (5秒)")
sleep_ms(5000)

# 绿色
fill(0x07E0)
print("3) GREEN 0x07E0 - 应该是绿色 (5秒)")
sleep_ms(5000)

# 蓝色
fill(0x001F)
print("4) BLUE 0x001F - 应该是蓝色 (5秒)")
sleep_ms(5000)

# 白色
fill(0xFFFF)
print("5) WHITE 0xFFFF (5秒)")
sleep_ms(5000)

# 回到黑色
fill(0x0000)
print("6) BLACK again")
print("完成! 颜色对吗?")
