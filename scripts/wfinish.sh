#!/usr/bin/env bash

"$CC" $CCFLAGS -std=gnu99 -c core/sha256.c -O3 -o sha256.o -fPIC
"$CC" $CCFLAGS -std=gnu99 -c compiler/cvm.c -O3 -o cvm.o -fPIC
"$CC" $CCFLAGS -std=gnu99 -c runtime/linenoise-win32.c -O3 -o linenoise.o -fPIC
"$CC" $CCFLAGS -std=gnu99 \
    runtime/driver.c \
    linenoise.o \
    cvm.o \
    sha256.o \
    wstanza.s \
    -o wstanza \
    -DPLATFORM_WINDOWS \
    -lm \
    -lpthread \
    -fPIC
