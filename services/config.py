# services/config.py — 配置持久化：JSON + 脏标志
# 文件损坏/缺失时使用默认值

try:
    import ujson as json
except ImportError:
    import json

from app_state import clamp

CONFIG_PATH = 'settings.json'

DEFAULT_CONFIG = {
    "fan_speed": 0,
    "smoke_speed": 0,
    "pump_speed": 0,
    "cob1_rgb": [0, 0, 0],
    "cob2_rgb": [0, 0, 0],
    "brightness": 100,
    "preset_idx": 0,
    "breath_mode": False,
    "gradient_mode": False,
}

# 校验范围
_RANGES = {
    "fan_speed": (0, 100),
    "smoke_speed": (0, 100),
    "pump_speed": (0, 100),
    "brightness": (0, 100),
    "preset_idx": (0, 11),
}


class Config:
    """JSON 配置持久化，脏标志机制。"""

    def __init__(self, path=None):
        self._path = path or CONFIG_PATH
        self._dirty = False
        self._data = dict(DEFAULT_CONFIG)

    def load(self):
        """从文件加载配置，损坏时使用默认值。"""
        try:
            with open(self._path, 'r') as f:
                raw = json.load(f)
            if isinstance(raw, dict):
                self._data = raw
        except (OSError, ValueError):
            print('WARN: config load failed, using defaults')
            self._data = dict(DEFAULT_CONFIG)
        self._validate()
        return self

    def _validate(self):
        """校验所有值范围，超出用默认值替换。"""
        for key, default in DEFAULT_CONFIG.items():
            if key not in self._data:
                self._data[key] = default
            elif key in _RANGES:
                lo, hi = _RANGES[key]
                v = self._data[key]
                if not isinstance(v, int) or v < lo or v > hi:
                    self._data[key] = default
            elif key in ("cob1_rgb", "cob2_rgb"):
                v = self._data[key]
                if not isinstance(v, list) or len(v) != 3:
                    self._data[key] = list(default)
                else:
                    for i in range(3):
                        if not isinstance(v[i], int) or v[i] < 0 or v[i] > 255:
                            v[i] = 0
            elif key in ("breath_mode", "gradient_mode"):
                if not isinstance(self._data[key], bool):
                    self._data[key] = default

    def save(self):
        """仅在 dirty=True 时写入文件。"""
        if not self._dirty:
            return
        try:
            with open(self._path, 'w') as f:
                json.dump(self._data, f)
            self._dirty = False
        except OSError:
            print('WARN: config save failed')

    def mark_dirty(self):
        """标记配置已修改。"""
        self._dirty = True

    @property
    def dirty(self):
        return self._dirty

    def get(self, key, default=None):
        return self._data.get(key, default)

    def set(self, key, value):
        if self._data.get(key) != value:
            self._data[key] = value
            self._dirty = True

    def load_to_state(self, state):
        """加载配置到 AppState，对每个值范围校验。"""
        self.load()
        state.fan_speed = clamp(self._data["fan_speed"], 0, 100)
        state.smoke_speed = clamp(self._data["smoke_speed"], 0, 100)
        state.pump_speed = clamp(self._data["pump_speed"], 0, 100)
        state.brightness = clamp(self._data["brightness"], 0, 100)
        state.preset_idx = clamp(self._data["preset_idx"], 0, 11)
        state.breath_mode = bool(self._data.get("breath_mode", False))
        state.gradient_mode = bool(self._data.get("gradient_mode", False))
        c1 = self._data.get("cob1_rgb", [0, 0, 0])
        c2 = self._data.get("cob2_rgb", [0, 0, 0])
        state.cob1_rgb = [clamp(c1[i], 0, 255) for i in range(3)]
        state.cob2_rgb = [clamp(c2[i], 0, 255) for i in range(3)]
        return state

    def save_from_state(self, state):
        """从 AppState 保存配置。"""
        self._data["fan_speed"] = state.fan_speed
        self._data["smoke_speed"] = state.smoke_speed
        self._data["pump_speed"] = state.pump_speed
        self._data["brightness"] = state.brightness
        self._data["preset_idx"] = state.preset_idx
        self._data["breath_mode"] = state.breath_mode
        self._data["gradient_mode"] = state.gradient_mode
        self._data["cob1_rgb"] = list(state.cob1_rgb)
        self._data["cob2_rgb"] = list(state.cob2_rgb)
        self._dirty = True
        self.save()
