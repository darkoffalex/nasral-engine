#!/bin/bash

# Убедимся, что Vulkan SDK установлен
if [ -z "$VULKAN_SDK" ]; then
  echo "Vulkan SDK not found. Make sure it is installed and environment variables are set."
  exit 1
fi

# Компилятор шейдеров
SLANGC="$VULKAN_SDK/bin/slangc"

# Проверяем наличие slangc
if [ ! -f "$SLANGC" ]; then
  echo "Compiler slangc not found in path $SLANGC."
  exit 1
fi

# Путь к текущей директории (где находится скрипт)
SCRIPT_DIR="$(dirname "$(realpath "$0")")"

# Удаляем все файлы .spv перед началом работы
echo "Removing all existing .spv files..."
find "$SCRIPT_DIR" -type f -name "*.spv" -delete
echo "Removal complete."

echo "Compiling SLANG shaders to SPIR-V..."

# Проходим по всем папкам в текущей директории
for DIR in "$SCRIPT_DIR"/*/; do
  [ -d "$DIR" ] || continue
  FOLDER_NAME="$(basename "$DIR")"
  echo "Processing folder \"$FOLDER_NAME\":"

  # Проходим по всем файлам в текущей папке
  for INPUT_FILE in "$DIR"*.slang; do
    [ -f "$INPUT_FILE" ] || continue
    FILE_NAME="$(basename "$INPUT_FILE")"

    # Пропускаем файлы, начинающиеся с символа '_'
    if [[ "$FILE_NAME" != _* ]]; then
      OUTPUT_FILE="${INPUT_FILE}.spv"

      echo "- Compiling $FILE_NAME -> $FILE_NAME.spv..."

      # Выполняем компиляцию
      "$SLANGC" "$INPUT_FILE" -target spirv -fvk-use-entrypoint-name -o "$OUTPUT_FILE"
    fi
  done
done

echo "Compilation complete."

