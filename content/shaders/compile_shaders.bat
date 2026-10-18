@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: Убедимся, что Vulkan SDK установлен
if not defined VULKAN_SDK (
    echo Vulkan SDK not found. Make sure it is installed and environment variables are set.
    pause
    exit /b 1
)

:: Компилятор шейдеров (SLANG -> SPIR-V)
set SLANGC=%VULKAN_SDK%\Bin\slangc.exe

:: Проверяем наличие glslangValidator.exe
if not exist "%SLANGC%" (
    echo Compiler glslangValidator.exe not found in path  %SLANGC%.
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

echo Compiling SLANG shaders to SPIR-V...

:: Проходим по всем папкам в текущей директории
for /d %%D in ("%SCRIPT_DIR%\*") do (
    echo Processing folder "%%~nD":

    :: Проходим по всем файлам в текущей папке
    for %%F in ("%%D\*.slang") do (
        :: Формируем путь для входного файла и выходного файла
        set INPUT_FILE=%%F
        set OUTPUT_FILE=%%F.spv

        echo - Compiling %%~nxF -^> %%~nxF.spv...

        :: Выполняем компиляцию
        "%SLANGC%" "!INPUT_FILE!" -target spirv -fvk-use-entrypoint-name -o "!OUTPUT_FILE!"
    )
)

echo Compilation complete.