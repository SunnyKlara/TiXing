@echo off
REM === 快速增量部署 ===
REM 只上传指定文件，用于日常开发迭代
REM 用法: deploy_quick.bat screens/boot.py ui/renderer.py
REM 或者: deploy_quick.bat screens\boot.py

if "%~1"=="" (
    echo 用法: deploy_quick.bat file1 [file2] [file3] ...
    echo 示例: deploy_quick.bat screens/boot.py assets/logo.bin
    exit /b 1
)

set CMD=py -m mpremote

:loop
if "%~1"=="" goto done
set FILE=%~1
REM 将反斜杠转为正斜杠
set FILE=%FILE:\=/%
echo 上传: %FILE%
set CMD=%CMD% + fs cp %FILE% :%FILE%
shift
goto loop

:done
%CMD% + reset
echo === 完成 ===
