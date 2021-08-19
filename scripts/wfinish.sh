#!/usr/bin/env bash

set -e

if [[ -z "$CC" ]]; then
    echo 'Need to set $CC to a valid compiler or cross-compiler' >/dev/stderr
    exit 2
fi

CCFLAGS="-DPLATFORM_WINDOWS -std=gnu99 -O3 -fPIC -Wall $CCFLAGS"
CFILES=(
    core/sha256.c
    compiler/cvm.c
    runtime/linenoise-win32.c
    runtime/driver.c
    runtime/process-win32.c
)

[[ ! -d "build" ]] && mkdir build

for file in "${CFILES[@]}"; do
    "$CC" $CCFLAGS -c "$file" -o "build/$(basename "$file" .c).o"
done

mapfile -t OFILES < <(
    for file in "${CFILES[@]}"; do
        echo "build/$(basename "$file" .c).o"
    done)

"$CC" "${OFILES[@]}" wstanza.s -o build/wstanza -lm -lpthread -fPIC
