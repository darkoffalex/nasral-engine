@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: Убедимся, что Vulkan SDK установлен
if not defined VULKAN_SDK (
    echo Vulkan SDK not found. Make sure it is installed and environment variables are set.
    pause
    exit /b 1
)

:: Компилятор шейдеров
set GLSLANG_VALIDATOR=%VULKAN_SDK%\Bin\glslangValidator.exe

:: Проверяем наличие glslangValidator.exe
if not exist "%GLSLANG_VALIDATOR%" (
    echo Compiler glslangValidator.exe not found in path  %GLSLANG_VALIDATOR%.
    pause
    exit /b 1
)

:: Путь к текущей директории (где находится скрипт)
set SCRIPT_DIR=%~dp0

:: Удаляем все файлы .spv перед началом работы
echo Removing all existing .spv files...
for /r "%SCRIPT_DIR%" %%F in (*.spv) do (
    del "%%F"
)
echo Removal complete.

echo Compiling GLSL shaders to SPIR-V...

:: Проходим по всем папкам в текущей директории
for /d %%D in ("%SCRIPT_DIR%\*") do (
    echo Processing folder "%%~nD":

    :: Проходим по всем файлам в текущей папке
    for %%F in ("%%D\*.vert" "%%D\*.frag" "%%D\*.geom") do (
        :: Формируем путь для входного файла и выходного файла
        set INPUT_FILE=%%F
        set OUTPUT_FILE=%%F.spv
        :: Выполняем компиляцию
        "%GLSLANG_VALIDATOR%" -V "!INPUT_FILE!" -o "!OUTPUT_FILE!"
    )
)

echo Compilation complete.