#!/bin/bash

# Убедимся, что Vulkan SDK установлен
if [ -z "$VULKAN_SDK" ]; then
  echo "Vulkan SDK not found. Make sure it is installed and environment variables are set."
  exit 1
fi

# Компилятор шейдеров
GLSLANG_VALIDATOR="$VULKAN_SDK/bin/glslangValidator"

# Проверяем наличие glslangValidator
if [ ! -f "$GLSLANG_VALIDATOR" ]; then
  echo "Compiler glslangValidator not found in path $GLSLANG_VALIDATOR."
  exit 1
fi

# Путь к текущей директории (где находится скрипт)
SCRIPT_DIR="$(dirname "$(realpath "$0")")"

# Удаляем все .spv файлы перед началом работы
echo "Removing all existing .spv files..."
find "$SCRIPT_DIR" -type f -name "*.spv" -exec rm {} \;
echo "Removal complete."

echo "Compiling GLSL shaders to SPIR-V..."

# Определяем нужные расширения файлов
SHADER_EXTENSIONS=("vert" "frag" "geom")

# Проходим по всем папкам в текущей директории
for MAT_DIR in "$SCRIPT_DIR"/*; do
  if [ -d "$MAT_DIR" ]; then
    echo "Processing folder \"$(basename "$MAT_DIR")\"..."

    # Формируем строку с шаблонами для globbing
    GLOB_PATTERNS=""
    for ext in "${SHADER_EXTENSIONS[@]}"; do
      GLOB_PATTERNS+="$MAT_DIR/*.${ext}"
      if [ ${#SHADER_EXTENSIONS[@]} -gt 1 ]; then
        GLOB_PATTERNS+=" "
      fi
    done

    # Проходим по всем файлам шейдеров в папке
    for SHADER_FILE in $GLOB_PATTERNS; do
      if [ -f "$SHADER_FILE" ]; then
        # Формируем пути входного и выходного файла
        INPUT_FILE="$SHADER_FILE"
        OUTPUT_FILE="${SHADER_FILE}.spv"

        echo "Compiling $(basename "$INPUT_FILE")..."
        "$GLSLANG_VALIDATOR" -V "$INPUT_FILE" -o "$OUTPUT_FILE"

        # Проверяем результат компиляции
        if [ $? -ne 0 ]; then
          echo "Compilation error in $(basename "$INPUT_FILE")!"
        else
          echo "Success: $(basename "$INPUT_FILE") -> $(basename "$OUTPUT_FILE")"
        fi
      fi
    done
  fi
done

echo "Compilation complete."