# screens/__init__.py — Screen 基类
# 所有界面继承此类，实现五个生命周期方法


class Context:
    """共享上下文对象，所有界面通过 ctx 访问共享资源。"""
    __slots__ = ('renderer', 'devices', 'config', 'state',
                 'effects', 'coordinator', 'screen_manager', 'input')

    def __init__(self, renderer, devices, config, state,
                 effects, coordinator, screen_manager=None, inp=None):
        self.renderer = renderer
        self.devices = devices
        self.config = config
        self.state = state
        self.effects = effects
        self.coordinator = coordinator
        self.screen_manager = screen_manager
        self.input = inp


class Screen:
    """界面基类。所有 screens/*.py 继承此类。"""

    def __init__(self, ctx):
        self.ctx = ctx

    def on_enter(self):
        """进入界面时调用。"""
        pass

    def on_input(self, event):
        """处理输入事件。"""
        pass

    def draw_full(self, r):
        """完整绘制（清屏后调用）。"""
        pass

    def draw_update(self, r):
        """局部刷新（仅更新变化区域）。"""
        pass

    def on_exit(self):
        """离开界面时调用。"""
        pass
