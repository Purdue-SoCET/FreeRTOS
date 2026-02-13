#!/bin/bash

SRC="/home/asicfab/a/yang2775/FreeRTOS/FreeRTOS/Demo/AFTx08/build/gcc/output/RTOSDemo.bin"
DST="/home/asicfab/a/yang2775/test/AFT-dev/meminit.bin"

if [ ! -f "$SRC" ]; then
    echo "ERROR: Source file not found: $SRC"
    exit 1
fi

mkdir -p "$(dirname "$DST")"

cp "$SRC" "$DST"

echo "Done: RTOSDemo.bin -> meminit.bin"
echo "Location: $DST"

