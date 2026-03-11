# test_cob_identify.py — 分辨两路 COB 灯带
# COB1 亮红色，COB2 亮蓝色，持续 5 秒后关闭

from machine import Pin, PWM
from time import sleep

# COB1: GPIO13=R, GPIO14=G, GPIO15=B
cob1_r = PWM(Pin(13)); cob1_r.freq(1000)
cob1_g = PWM(Pin(14)); cob1_g.freq(1000)
cob1_b = PWM(Pin(15)); cob1_b.freq(1000)

# COB2: GPIO7=R, GPIO8=G, GPIO9=B
cob2_r = PWM(Pin(7)); cob2_r.freq(1000)
cob2_g = PWM(Pin(8)); cob2_g.freq(1000)
cob2_b = PWM(Pin(9)); cob2_b.freq(1000)

print("=== COB 灯带识别测试 ===")
print("COB1 (GPIO13/14/15) = 红色")
print("COB2 (GPIO7/8/9)    = 蓝色")
print("持续 5 秒...")

# COB1 亮红色
cob1_r.duty_u16(65535)
cob1_g.duty_u16(0)
cob1_b.duty_u16(0)

# COB2 亮蓝色
cob2_r.duty_u16(0)
cob2_g.duty_u16(0)
cob2_b.duty_u16(65535)

sleep(5)

# 全部关闭
for p in (cob1_r, cob1_g, cob1_b, cob2_r, cob2_g, cob2_b):
    p.duty_u16(0)

print("=== 测试完成 ===")
