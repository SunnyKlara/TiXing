# screen_manager.py — 界面状态机
# F4 风格 ui/init_flag 双变量机制

# 界面 ID 常量
UI_BOOT = 0
UI_MENU = 1
UI_SPEED = 2
UI_SMOKE = 3
UI_PUMP = 4
UI_PRESET = 5
UI_RGB = 6
UI_BRIGHT = 7


class ScreenManager:
    """界面状态机：管理切换、事件分发、init_flag 机制。"""

    def __init__(self, ctx):
        self._screens = {}
        self._current = UI_BOOT
        self._init_flag = True
        self._ctx = ctx
        ctx.screen_manager = self

    def register(self, ui_id, screen):
        """注册界面实例。"""
        self._screens[ui_id] = screen

    def switch(self, ui_id):
        """切换界面。

        旧界面 on_exit() → 更新 _current + _init_flag → 新界面 on_enter()
        """
        if self._current in self._screens:
            self._screens[self._current].on_exit()
        self._current = ui_id
        self._init_flag = True
        if ui_id in self._screens:
            self._screens[ui_id].on_enter()

    def dispatch(self, event):
        """将输入事件分发给当前界面。"""
        if self._current in self._screens:
            self._screens[self._current].on_input(event)

    def render(self):
        """渲染当前界面。

        init_flag=True → clear + draw_full + 重置
        否则 → draw_update
        """
        if self._current not in self._screens:
            return
        screen = self._screens[self._current]
        r = self._ctx.renderer
        if self._init_flag:
            r.clear()
            screen.draw_full(r)
            self._init_flag = False
        else:
            screen.draw_update(r)

    @property
    def current_ui(self):
        """当前界面 ID。"""
        return self._current
