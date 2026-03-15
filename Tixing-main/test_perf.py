# test_perf.py
import gc
gc.collect()
print("=== perf ===")
print("free:", gc.mem_free())

from machine import Pin, SPI
from utime import ticks_ms, ticks_diff

spi = SPI(0, baudrate=62_500_000, sck=Pin(2), mosi=Pin(3))
print("spi ok")

from drivers.st7789 import ST7789
disp = ST7789(spi=spi)
disp.init()
print("disp ok")

buf40 = bytearray(3200)
print("buf ok")

# test 1
t0 = ticks_ms()
for _ in range(20):
    disp.fill_rect(100, 4, 40, 40, 0)
print("fill40:", ticks_diff(ticks_ms(), t0)//20)

# test 2
t0 = ticks_ms()
for _ in range(20):
    disp.blit_block(buf40, 100, 4, 40, 40)
print("blit40:", ticks_diff(ticks_ms(), t0)//20)

# test 3: 5x blit
t0 = ticks_ms()
for _ in range(20):
    for x in (10, 66, 122, 178, 234):
        disp.blit_block(buf40, x, 4, 40, 40)
print("5xblit:", ticks_diff(ticks_ms(), t0)//20)

# test 4: 5x fill narrow
t0 = ticks_ms()
for _ in range(20):
    for x in (50, 106, 162, 218, 274):
        disp.fill_rect(x, 4, 6, 40, 0)
print("5xfill6:", ticks_diff(ticks_ms(), t0)//20)

# test 5: big blit
gc.collect()
fb = bytearray(284*49*2)
print("fb alloc:", gc.mem_free())
t0 = ticks_ms()
for _ in range(10):
    disp.blit_block(fb, 0, 0, 284, 49)
print("bigblit:", ticks_diff(ticks_ms(), t0)//10)

# test 6: fb clear
z = bytes(len(fb))
t0 = ticks_ms()
for _ in range(20):
    fb[:] = z
print("fbclr:", ticks_diff(ticks_ms(), t0)//20)

# test 7: slice copy
src = bytearray(80)
t0 = ticks_ms()
for _ in range(1000):
    fb[200:280] = src
print("slice1k:", ticks_diff(ticks_ms(), t0))

# test 8: 6 icon blit in fb (python loop)
icon = bytearray(3200)
t0 = ticks_ms()
for _ in range(10):
    for i in range(6):
        ix = i * 56 - 40
        for row in range(40):
            dx0 = max(0, ix)
            dx1 = min(284, ix + 40)
            if dx0 >= dx1:
                continue
            sx0 = dx0 - ix
            n = (dx1 - dx0) * 2
            s = (row * 40 + sx0) * 2
            d = (4 + row) * 568 + dx0 * 2
            fb[d:d+n] = icon[s:s+n]
print("6icon_fb:", ticks_diff(ticks_ms(), t0)//10)

del fb, z
gc.collect()
print("done, free:", gc.mem_free())
