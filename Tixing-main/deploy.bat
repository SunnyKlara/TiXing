@echo off
echo === 快速部署 (单次连接) ===

REM mpremote 命令链: 用 + 连接，只建立一次 USB 连接
REM fs cp -r 递归拷贝整个目录，比逐文件快得多

py -m mpremote ^
    exec "import os; [os.remove(f) if os.stat(f)[0] & 0x8000 else None for f in os.listdir('/')]" ^
    + fs rm -r :hal ^
    + fs rm -r :screens ^
    + fs rm -r :drivers ^
    + fs rm -r :ui ^
    + fs rm -r :services ^
    + fs rm -r :assets ^
    + fs rm -r :icons ^
    + fs mkdir :hal ^
    + fs mkdir :screens ^
    + fs mkdir :drivers ^
    + fs mkdir :ui ^
    + fs mkdir :services ^
    + fs mkdir :assets ^
    + fs mkdir :icons ^
    + fs cp -r hal/ : ^
    + fs cp -r screens/ : ^
    + fs cp -r drivers/ : ^
    + fs cp -r ui/ : ^
    + fs cp -r assets/ : ^
    + fs cp -r icons/ : ^
    + fs cp main.py :main.py ^
    + fs cp app_state.py :app_state.py ^
    + fs cp screen_manager.py :screen_manager.py ^
    + fs cp coordinator.py :coordinator.py ^
    + fs cp config.py :config.py ^
    + reset

echo === 部署完成 ===
