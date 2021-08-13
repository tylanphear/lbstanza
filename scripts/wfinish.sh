#!/usr/bin/env bash

if [[ -z "$CC" ]]; then
    echo 'Need to set $CC to a valid compiler or cross-compiler' >/dev/stderr
    exit 2
fi

CCFLAGS="$CCFLAGS -DPLATFORM_WINDOWS"

"$CC" $CCFLAGS -std=gnu99 -c core/sha256.c -O3 -o sha256.o -fPIC
"$CC" $CCFLAGS -std=gnu99 -c compiler/cvm.c -O3 -o cvm.o -fPIC
"$CC" $CCFLAGS -std=gnu99 -c runtime/linenoise-win32.c -O3 -o linenoise.o -fPIC
"$CC" $CCFLAGS -std=gnu99 -c runtime/driver.c -O3 -o driver.o -fPIC
"$CC" $CCFLAGS -std=gnu99 \
    driver.o \
    linenoise.o \
    cvm.o \
    sha256.o \
    wstanza.s \
    -o wstanza \
    -lm \
    -lpthread \
    -fPIC
