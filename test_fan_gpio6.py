# test_fan_gpio6.py — 主风扇 GPIO6 独立测试
# 上传到 Pico 后通过 REPL 执行: exec(open('test_fan_gpio6.py').read())
# 或用 mpremote: mpremote run test_fan_gpio6.py

from machine import Pin, PWM
from time import sleep

fan = PWM(Pin(6))
fan.freq(1000)

print("=== GPIO6 主风扇测试 ===")
print("50% 转速，持续 3 秒...")
fan.duty_u16(65535 * 50 // 100)
sleep(3)

print("100% 转速，持续 3 秒...")
fan.duty_u16(65535)
sleep(3)

print("停止")
fan.duty_u16(0)
print("=== 测试完成 ===")
