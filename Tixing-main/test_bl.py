# test_bl.py — 探测背光引脚
# 部署: py -m mpremote cp test_bl.py :test_bl.py + exec "import test_bl"
# 测试 GPIO4 是否控制屏幕背光

from machine import Pin
from time import sleep_ms

bl = Pin(4, Pin.OUT)

print("=== 背光引脚探测 ===")
print("GPIO4 拉低 (关背光?)...")
bl.off()
sleep_ms(2000)

print("GPIO4 拉高 (开背光?)...")
bl.on()
sleep_ms(2000)

print("再次拉低...")
bl.off()
sleep_ms(2000)

print("拉高恢复")
bl.on()
print("done - 如果屏幕有明暗变化，GPIO4 就是背光引脚")
