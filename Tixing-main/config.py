# config.py - 配置持久化模块
# 参考 f4 的 deng_update()/deng_init() 和 WriteRead[] 数组机制
# 使用 JSON 文件 + 脏标志延迟写入，替代 f4 的 W25Q128 Flash 扇区擦写

import json

# --------------------------
# 配置文件路径
# --------------------------
CONFIG_FILE = "settings.json"

# --------------------------
# 默认配置（新设备或配置损坏时使用）
# --------------------------
DEFAULT_CONFIG = {
    "fan_speed": 0,
    "smoke_speed": 0,
    "air_pump_speed": 0,
    "cob1_rgb": [0, 0, 0],
    "cob2_rgb": [0, 0, 0],
    "brightness": 100,
    "color_preset": 1,
    "led_mode": "normal",
    "link_mode": "auto",
    "fan_link_ratio": 0.8,
}

# --------------------------
# 配置值范围校验规则
# 元组 (min, max) 表示数值范围，callable 表示自定义校验
# --------------------------
CONFIG_VALIDATORS = {
    "fan_speed": (0, 100),
    "smoke_speed": (0, 100),
    "air_pump_speed": (0, 100),
    "brightness": (0, 100),
    "color_preset": (0, 11),
    "cob1_rgb": lambda v: isinstance(v, list) and len(v) == 3 and all(0 <= x <= 255 for x in v),
    "cob2_rgb": lambda v: isinstance(v, list) and len(v) == 3 and all(0 <= x <= 255 for x in v),
    "led_mode": lambda v: v in ("normal", "breathing", "gradient"),
    "link_mode": lambda v: v in ("auto", "manual"),
    "fan_link_ratio": (0.0, 1.0),
}


def _validate_value(key, value):
    """校验单个配置值是否在有效范围内"""
    validator = CONFIG_VALIDATORS.get(key)
    if validator is None:
        return True
    if isinstance(validator, tuple):
        lo, hi = validator
        return isinstance(value, (int, float)) and lo <= value <= hi
    # callable 校验（lambda）
    try:
        return validator(value)
    except Exception:
        return False

# --------------------------
# 模块级状态（参考 f4 的 WriteRead[] 全局数组）
# --------------------------
_config = {}
_dirty = False


def load_config():
    """
    从 settings.json 加载配置。
    文件不存在或损坏时使用默认值。
    新增的配置项自动从 DEFAULT_CONFIG 补全。
    """
    global _config, _dirty
    loaded = None
    try:
        with open(CONFIG_FILE, "r") as f:
            loaded = json.load(f)
    except (OSError, ValueError):
        # 文件不存在或 JSON 解析失败
        pass

    # 以默认值为基础，用已保存的值覆盖（确保新增 key 有默认值）
    _config = {}
    for k, v in DEFAULT_CONFIG.items():
        if loaded and k in loaded:
            _config[k] = loaded[k]
        else:
            # 列表类型需要拷贝，避免共享引用
            _config[k] = v[:] if isinstance(v, list) else v

    # 范围校验：超出范围的值替换为默认值
    for k in list(_config.keys()):
        if k in CONFIG_VALIDATORS:
            if not _validate_value(k, _config[k]):
                print("config: {} 值无效，使用默认值".format(k))
                v = DEFAULT_CONFIG[k]
                _config[k] = v[:] if isinstance(v, list) else v

    _dirty = False
    return _config


def save_config():
    """
    将配置写入 settings.json（仅在 dirty 时写入）。
    参考 f4 的 deng_update()：只在设置变更时写入 Flash，减少擦写次数。
    """
    global _dirty
    if not _dirty:
        return
    try:
        with open(CONFIG_FILE, "w") as f:
            json.dump(_config, f)
        _dirty = False
    except OSError:
        print("config: 写入 settings.json 失败")


def get(key):
    """获取配置值"""
    return _config.get(key)


def set(key, value):
    """设置配置值并标记脏标志"""
    global _dirty
    _config[key] = value
    _dirty = True


def is_dirty():
    """检查是否有未保存的变更"""
    return _dirty


def mark_dirty():
    """手动标记配置为已变更（用于批量修改后统一标记）"""
    global _dirty
    _dirty = True
