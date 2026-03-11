@echo off
echo === 清理 Pico 旧文件 ===
py -m mpremote rm -r :hal 2>nul
py -m mpremote rm -r :screens 2>nul
py -m mpremote rm -r :drivers 2>nul
py -m mpremote rm -r :ui 2>nul
py -m mpremote rm -r :services 2>nul
py -m mpremote rm -r :assets 2>nul

echo === 上传核心文件 ===
py -m mpremote mkdir :hal
py -m mpremote mkdir :screens
py -m mpremote mkdir :drivers
py -m mpremote mkdir :ui
py -m mpremote mkdir :services
py -m mpremote mkdir :assets

echo --- HAL 层 ---
py -m mpremote cp hal/__init__.py :hal/__init__.py
py -m mpremote cp hal/pwm_devices.py :hal/pwm_devices.py
py -m mpremote cp hal/input.py :hal/input.py
py -m mpremote cp hal/display.py :hal/display.py

echo --- 驱动 ---
py -m mpremote cp drivers/__init__.py :drivers/__init__.py
py -m mpremote cp drivers/st7789.py :drivers/st7789.py

echo --- 界面 ---
py -m mpremote cp screens/__init__.py :screens/__init__.py
py -m mpremote cp screens/boot.py :screens/boot.py
py -m mpremote cp screens/menu.py :screens/menu.py
py -m mpremote cp screens/speed.py :screens/speed.py
py -m mpremote cp screens/smoke.py :screens/smoke.py
py -m mpremote cp screens/pump.py :screens/pump.py
py -m mpremote cp screens/preset.py :screens/preset.py
py -m mpremote cp screens/rgb.py :screens/rgb.py
py -m mpremote cp screens/brightness.py :screens/brightness.py

echo --- UI 组件 ---
for %%f in (ui\*.py) do py -m mpremote cp %%f :ui/%%~nxf

echo --- 根目录模块 ---
py -m mpremote cp main.py :main.py
py -m mpremote cp app_state.py :app_state.py
py -m mpremote cp screen_manager.py :screen_manager.py

echo --- 资源文件 ---
for %%f in (assets\*.bin) do py -m mpremote cp %%f :assets/%%~nxf

echo === 部署完成，重启 Pico ===
py -m mpremote reset
