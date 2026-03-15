# ui/lerp.py — Carousel 位移动画控制器
# 只插值 X 位置，不做尺寸插值（尺寸由离中心距离实时决定）
# 整数定点运算 (<<8)，零浮点

from ui.theme import SCREEN_W

# ── 布局常量 ──
ITEM_SPACING = 56
CENTER_X = SCREEN_W // 2

# ── 动画配置 ──
ANIM_FRAMES = 10         # 足够帧数让滑动过程可见
FP_SHIFT = 8
FP_HALF = 1 << (FP_SHIFT - 1)

# ── 最多同时可见的项数 ──
MAX_SLOTS = 5


class LerpAnimator:
    """只插值 X 位置的动画控制器。

    每个槽位只有一个值: cx (中心 X 坐标)。
    全整数定点运算。
    """

    def __init__(self):
        # 每个槽位: [cur_fp, delta_fp, tgt_fp]
        self._data = [0] * (MAX_SLOTS * 3)
        self._result = [0] * MAX_SLOTS
        self._frame = 0
        self._total_frames = 0
        self._active = False

    @property
    def is_active(self):
        return self._active

    def start(self, old_cxs, new_cxs, frames=ANIM_FRAMES):
        """启动动画。old_cxs/new_cxs: [cx0, cx1, cx2, cx3, cx4]"""
        if frames < 1:
            frames = 1
        d = self._data
        n = min(len(old_cxs), len(new_cxs), MAX_SLOTS)
        for i in range(n):
            base = i * 3
            cur = old_cxs[i] << FP_SHIFT
            tgt = new_cxs[i] << FP_SHIFT
            d[base] = cur
            d[base + 1] = (tgt - cur) // frames
            d[base + 2] = tgt
        self._frame = 0
        self._total_frames = frames
        self._active = True

    def tick(self):
        """推进一帧，返回 [cx0, cx1, ...] 或 None。"""
        if not self._active:
            return None
        self._frame += 1
        d = self._data
        r = self._result
        last = self._frame >= self._total_frames
        for i in range(MAX_SLOTS):
            base = i * 3
            if last:
                r[i] = d[base + 2] >> FP_SHIFT
            else:
                d[base] += d[base + 1]
                r[i] = (d[base] + FP_HALF) >> FP_SHIFT
        if last:
            self._active = False
        return r


def make_cx_list(selected_idx, total_count):
    """生成 5 个槽位的 cx 列表。"""
    result = []
    for slot in range(MAX_SLOTS):
        offset = slot - 2
        cx = CENTER_X + offset * ITEM_SPACING
        result.append(cx)
    return result
