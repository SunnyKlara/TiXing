# test_config.py - config.py 单元测试
# 可在 CPython 上运行验证逻辑正确性

import os
import json
import config

TEST_FILE = config.CONFIG_FILE


def cleanup():
    """删除测试产生的配置文件"""
    try:
        os.remove(TEST_FILE)
    except OSError:
        pass


def test_load_defaults_when_no_file():
    """无配置文件时应加载默认值"""
    cleanup()
    cfg = config.load_config()
    assert cfg["fan_speed"] == 0
    assert cfg["smoke_speed"] == 0
    assert cfg["air_pump_speed"] == 0
    assert cfg["cob1_rgb"] == [0, 0, 0]
    assert cfg["cob2_rgb"] == [0, 0, 0]
    assert cfg["brightness"] == 100
    assert cfg["color_preset"] == 1
    assert cfg["led_mode"] == "normal"
    assert cfg["link_mode"] == "auto"
    assert cfg["fan_link_ratio"] == 0.8
    assert config.is_dirty() == False
    print("PASS: test_load_defaults_when_no_file")


def test_save_and_load_roundtrip():
    """保存后重新加载应恢复设置"""
    cleanup()
    config.load_config()
    config.set("fan_speed", 75)
    config.set("cob1_rgb", [255, 128, 0])
    config.save_config()

    # 重新加载
    cfg = config.load_config()
    assert cfg["fan_speed"] == 75
    assert cfg["cob1_rgb"] == [255, 128, 0]
    # 未修改的值保持默认
    assert cfg["brightness"] == 100
    print("PASS: test_save_and_load_roundtrip")


def test_dirty_flag():
    """脏标志应在 set 后为 True，save 后为 False"""
    cleanup()
    config.load_config()
    assert config.is_dirty() == False

    config.set("brightness", 50)
    assert config.is_dirty() == True

    config.save_config()
    assert config.is_dirty() == False
    print("PASS: test_dirty_flag")


def test_save_skips_when_not_dirty():
    """未修改时 save 不应写入文件"""
    cleanup()
    config.load_config()
    config.save_config()
    # 文件不应存在（因为没有脏标志）
    file_exists = False
    try:
        with open(TEST_FILE, "r") as f:
            f.read()
        file_exists = True
    except OSError:
        pass
    assert file_exists == False
    print("PASS: test_save_skips_when_not_dirty")


def test_mark_dirty():
    """mark_dirty 应手动设置脏标志"""
    cleanup()
    config.load_config()
    assert config.is_dirty() == False
    config.mark_dirty()
    assert config.is_dirty() == True
    print("PASS: test_mark_dirty")


def test_corrupt_file_uses_defaults():
    """损坏的 JSON 文件应回退到默认值"""
    cleanup()
    with open(TEST_FILE, "w") as f:
        f.write("{invalid json!!!")
    cfg = config.load_config()
    assert cfg["fan_speed"] == 0
    assert cfg["brightness"] == 100
    print("PASS: test_corrupt_file_uses_defaults")


def test_merge_with_new_keys():
    """旧配置文件缺少新增 key 时应补全默认值"""
    cleanup()
    # 模拟旧版本配置（缺少 air_pump_speed）
    old_config = {"fan_speed": 50, "smoke_speed": 30}
    with open(TEST_FILE, "w") as f:
        json.dump(old_config, f)

    cfg = config.load_config()
    assert cfg["fan_speed"] == 50
    assert cfg["smoke_speed"] == 30
    assert cfg["air_pump_speed"] == 0  # 新 key 用默认值
    assert cfg["brightness"] == 100
    print("PASS: test_merge_with_new_keys")


def test_get_and_set():
    """get/set 基本读写"""
    cleanup()
    config.load_config()
    config.set("led_mode", "breathing")
    assert config.get("led_mode") == "breathing"
    assert config.get("nonexistent_key") is None
    print("PASS: test_get_and_set")


def test_list_values_are_independent():
    """列表类型配置应独立于 DEFAULT_CONFIG（不共享引用）"""
    cleanup()
    config.load_config()
    config.set("cob1_rgb", [100, 200, 50])
    # 默认值不应被修改
    assert config.DEFAULT_CONFIG["cob1_rgb"] == [0, 0, 0]
    print("PASS: test_list_values_are_independent")


def test_validate_numeric_range():
    """数值超出范围时应替换为默认值"""
    cleanup()
    bad_config = {"fan_speed": 150, "brightness": -10, "color_preset": 20}
    with open(TEST_FILE, "w") as f:
        json.dump(bad_config, f)
    cfg = config.load_config()
    assert cfg["fan_speed"] == 0, "fan_speed should be default (0), got {}".format(cfg["fan_speed"])
    assert cfg["brightness"] == 100, "brightness should be default (100), got {}".format(cfg["brightness"])
    assert cfg["color_preset"] == 1, "color_preset should be default (1), got {}".format(cfg["color_preset"])
    print("PASS: test_validate_numeric_range")


def test_validate_string_enum():
    """字符串枚举值无效时应替换为默认值"""
    cleanup()
    bad_config = {"led_mode": "invalid_mode", "link_mode": "unknown"}
    with open(TEST_FILE, "w") as f:
        json.dump(bad_config, f)
    cfg = config.load_config()
    assert cfg["led_mode"] == "normal", "led_mode should be default, got {}".format(cfg["led_mode"])
    assert cfg["link_mode"] == "auto", "link_mode should be default, got {}".format(cfg["link_mode"])
    print("PASS: test_validate_string_enum")


def test_validate_rgb_list():
    """RGB 列表格式无效时应替换为默认值"""
    cleanup()
    bad_config = {"cob1_rgb": [300, 0, 0], "cob2_rgb": "not_a_list"}
    with open(TEST_FILE, "w") as f:
        json.dump(bad_config, f)
    cfg = config.load_config()
    assert cfg["cob1_rgb"] == [0, 0, 0], "cob1_rgb should be default, got {}".format(cfg["cob1_rgb"])
    assert cfg["cob2_rgb"] == [0, 0, 0], "cob2_rgb should be default, got {}".format(cfg["cob2_rgb"])
    print("PASS: test_validate_rgb_list")


def test_validate_fan_link_ratio():
    """fan_link_ratio 超出 0.0-1.0 范围时应替换为默认值"""
    cleanup()
    bad_config = {"fan_link_ratio": 1.5}
    with open(TEST_FILE, "w") as f:
        json.dump(bad_config, f)
    cfg = config.load_config()
    assert cfg["fan_link_ratio"] == 0.8, "fan_link_ratio should be default (0.8), got {}".format(cfg["fan_link_ratio"])
    print("PASS: test_validate_fan_link_ratio")


def test_validate_valid_values_pass():
    """有效值应保持不变"""
    cleanup()
    good_config = {
        "fan_speed": 50,
        "brightness": 100,
        "cob1_rgb": [128, 64, 255],
        "led_mode": "breathing",
        "link_mode": "manual",
        "fan_link_ratio": 0.5,
    }
    with open(TEST_FILE, "w") as f:
        json.dump(good_config, f)
    cfg = config.load_config()
    assert cfg["fan_speed"] == 50
    assert cfg["brightness"] == 100
    assert cfg["cob1_rgb"] == [128, 64, 255]
    assert cfg["led_mode"] == "breathing"
    assert cfg["link_mode"] == "manual"
    assert cfg["fan_link_ratio"] == 0.5
    print("PASS: test_validate_valid_values_pass")


def test_validate_non_numeric_type():
    """非数值类型的数值配置项应替换为默认值"""
    cleanup()
    bad_config = {"fan_speed": "fast", "brightness": None}
    with open(TEST_FILE, "w") as f:
        json.dump(bad_config, f)
    cfg = config.load_config()
    assert cfg["fan_speed"] == 0, "fan_speed should be default, got {}".format(cfg["fan_speed"])
    assert cfg["brightness"] == 100, "brightness should be default, got {}".format(cfg["brightness"])
    print("PASS: test_validate_non_numeric_type")


if __name__ == "__main__":
    test_load_defaults_when_no_file()
    test_save_and_load_roundtrip()
    test_dirty_flag()
    test_save_skips_when_not_dirty()
    test_mark_dirty()
    test_corrupt_file_uses_defaults()
    test_merge_with_new_keys()
    test_get_and_set()
    test_list_values_are_independent()
    test_validate_numeric_range()
    test_validate_string_enum()
    test_validate_rgb_list()
    test_validate_fan_link_ratio()
    test_validate_valid_values_pass()
    test_validate_non_numeric_type()
    cleanup()
    print("\n全部测试通过!")
