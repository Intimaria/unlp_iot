#!/bin/bash

# Entramos a la carpeta del proyecto para que encuentre 'data'
cd "$(dirname "$0")"

# --- Configuración de sistema (puede ser diferente en otros sistemas) ---
#puerto USB
PORT="/dev/ttyUSB0"

# baud rate
BAUD="921600"

# dirección de flash (puede cambiar por placa)
FLASH_ADDR="0x290000"

# tamaño de SPIFFS (puede cambiar por placa)
SPIFFS_SIZE="0x160000"

echo "Buscando herramientas..."

# 1. Intentar buscar en el PATH del sistema
MKSPIFFS_BIN=$(which mkspiffs 2>/dev/null)
ESPTOOL_BIN=$(which esptool 2>/dev/null || which esptool.py 2>/dev/null)

# 2. Si no están en el PATH, buscar en carpetas de Arduino (incluyendo Snap)
if [ -z "$MKSPIFFS_BIN" ]; then
    MKSPIFFS_BIN=$(find $HOME/snap/arduino $HOME/.arduino15 -name mkspiffs -type f -executable 2>/dev/null | head -n 1)
fi

if [ -z "$ESPTOOL_BIN" ]; then
    ESPTOOL_BIN=$(find $HOME/snap/arduino $HOME/.arduino15 -name esptool -type f -executable 2>/dev/null | head -n 1)
fi

# Validar si se encontraron
if [ -z "$MKSPIFFS_BIN" ] || [ -z "$ESPTOOL_BIN" ]; then
    echo "Error: No se encontró 'mkspiffs' o 'esptool' automáticamente."
    echo "Asegúrate de tener instalado el core de ESP32 en Arduino IDE."
    exit 1
fi

echo "Usando mkspiffs: $MKSPIFFS_BIN"
echo "Usando esptool:  $ESPTOOL_BIN"

echo ""
echo "====================================="
echo " Empaquetando la carpeta 'data'..."
echo "====================================="
"$MKSPIFFS_BIN" -c data -b 4096 -p 256 -s $SPIFFS_SIZE spiffs.bin

echo ""
echo "====================================="
echo " Subiendo la web al ESP32 en puerto $PORT..."
echo "====================================="
# Mantener presionado el botón BOOT si la placa lo requiere
"$ESPTOOL_BIN" --chip esp32 --port "$PORT" --baud "$BAUD" write_flash "$FLASH_ADDR" spiffs.bin

echo ""
echo "====================================="
echo " ¡Subida exitosa! "
echo "====================================="
