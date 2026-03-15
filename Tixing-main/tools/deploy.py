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
ROOT_FILES = ['boot.py', 'main.py', 'app_state.py', 'screen_manager.py']


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
    print("Pico Turbo 一键部署 (快速模式)")
    print("=" * 50)

    # 收集所有需要上传的文件
    files = []

    # 根目录文件
    for f in ROOT_FILES:
        path = os.path.join(PROJECT_ROOT, f)
        if os.path.exists(path):
            files.append((path, f))

    # 各子目录
    for d in DIRS:
        dir_path = os.path.join(PROJECT_ROOT, d)
        if not os.path.isdir(dir_path):
            continue
        for f in sorted(os.listdir(dir_path)):
            fpath = os.path.join(dir_path, f)
            if os.path.isfile(fpath) and (f.endswith('.py') or f.endswith('.bin')):
                files.append((fpath, d + '/' + f))

    print(f"\n共 {len(files)} 个文件待上传")

    # 构建 mpremote 命令链: 单次连接完成所有操作
    cmd = MPREMOTE + ['connect', PICO_PORT]

    # 用 exec 在设备端创建目录（已存在不报错）
    mkdir_code = "import os\n"
    for d in DIRS:
        mkdir_code += f"try:\n os.mkdir('{d}')\nexcept: pass\n"
    cmd += ['+', 'exec', mkdir_code]

    # 上传所有文件
    for local, remote in files:
        cmd += ['+', 'cp', local, ':' + remote]

    # 重启
    cmd += ['+', 'reset']

    print("单次连接上传中...")

    r = subprocess.run(cmd, capture_output=True, text=True, cwd=PROJECT_ROOT)
    if r.stdout.strip():
        for line in r.stdout.strip().split('\n'):
            print(f"  {line}")
    if r.returncode != 0:
        print(f"ERROR: {r.stderr.strip()}")
        sys.exit(1)

    print(f"\n部署完成! 上传 {len(files)} 个文件, Pico 已重启。")



if __name__ == '__main__':
    deploy()
