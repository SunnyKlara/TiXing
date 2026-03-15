#!/usr/bin/env python3
"""一键部署到 Pico：自动同步所有需要烧录的文件。

用法: py tools/deploy.py
"""

import subprocess
import sys
import os

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MPREMOTE = [sys.executable, '-m', 'mpremote']

# 串口设置（改成你的 Pico 串口号）
PICO_PORT = 'COM4'

# 需要烧录的目录和文件
DIRS = ['drivers', 'hal', 'ui', 'screens', 'services', 'assets', 'icons']
ROOT_FILES = ['main.py', 'app_state.py', 'screen_manager.py']


def run(cmd, check=True):
    """执行命令并打印。"""
    print(f"  > {' '.join(cmd)}")
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=PROJECT_ROOT)
    if r.stdout.strip():
        print(f"    {r.stdout.strip()}")
    if r.returncode != 0 and check:
        print(f"    ERROR: {r.stderr.strip()}")
    return r.returncode == 0


def mpremote(*args):
    return run(MPREMOTE + ['connect', PICO_PORT] + list(args))


def mkdir_pico(path):
    """在 Pico 上创建目录（忽略已存在错误）。"""
    run(MPREMOTE + ['mkdir', ':' + path], check=False)


def cp_file(local, remote):
    """复制单个文件到 Pico。"""
    return mpremote('cp', local, ':' + remote)


def deploy():
    print("=" * 50)
    print("Pico Turbo 一键部署")
    print("=" * 50)

    # 检查连接
    print("\n[1/4] 检查 Pico 连接 ({})...".format(PICO_PORT))
    if not mpremote('eval', '1'):
        print("ERROR: 无法连接 Pico ({}), 请确认 USB 连接且 Thonny 已关闭".format(PICO_PORT))
        sys.exit(1)
    print("    连接成功!")

    # 创建目录
    print("\n[2/4] 创建目录结构...")
    for d in DIRS:
        mkdir_pico(d)

    # 上传文件
    print("\n[3/4] 上传文件...")
    count = 0
    errors = 0

    # 根目录文件
    for f in ROOT_FILES:
        path = os.path.join(PROJECT_ROOT, f)
        if os.path.exists(path):
            if cp_file(path, f):
                count += 1
            else:
                errors += 1

    # 各子目录
    for d in DIRS:
        dir_path = os.path.join(PROJECT_ROOT, d)
        if not os.path.isdir(dir_path):
            continue
        for f in sorted(os.listdir(dir_path)):
            fpath = os.path.join(dir_path, f)
            if os.path.isfile(fpath) and (f.endswith('.py') or f.endswith('.bin')):
                remote = d + '/' + f
                if cp_file(fpath, remote):
                    count += 1
                else:
                    errors += 1

    # 完成
    print(f"\n[4/4] 部署完成")
    print(f"  上传: {count} 个文件")
    if errors:
        print(f"  失败: {errors} 个文件")
    else:
        print("  全部成功！")

    # 重启
    print("\n重启 Pico...")
    mpremote('reset')
    print("Done! Pico 应该已经开始运行了。")


if __name__ == '__main__':
    deploy()
