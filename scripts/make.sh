#!/usr/bin/env bash

# USAGES:
# ./scripts/make.sh ./stanza os-x
# ./scripts/make.sh lstanza windows

HERE="$(dirname "${BASH_SOURCE[0]}")"

set -e
set -o pipefail

if [ $# -lt 2 ]; then
    echo "Not enough arguments"
    exit 2
fi

STANZA="$1"
PLATFORM="$2"

case "$PLATFORM" in
    windows) PLATFORM_PREFIX="w" ;;
    linux)   PLATFORM_PREFIX="l" ;;
    os-x)    PLATFORM_PREFIX=""  ;;
    *) echo "Error: unsupported/unrecognized platform: $PLATFORM" 1>&2 && exit 2 ;;
esac

echo "Building Stanza for $PLATFORM"

#Pkg packages
PKGFILES="math arg-parser line-wrap stz/test-driver stz/mocker stz/arg-parser"
PKGDIR="${PLATFORM_PREFIX}pkgs"
STANZA_S="${PLATFORM_PREFIX}stanza.s"

#Delete pkg files
rm -rf "$PKGDIR"

#Create folders
mkdir -p "$PKGDIR"

#Clean
$STANZA clean

echo "Compiling $PLATFORM Stanza Pkgs"
$STANZA build-stanza.proj stz/driver $PKGFILES -pkg "$PKGDIR" -optimize -platform $PLATFORM

echo "Compiling $PLATFORM Stanza Executable"
$STANZA build-stanza.proj stz/driver -pkg "$PKGDIR" -s "$STANZA_S" -optimize -platform $PLATFORM

echo "Finishing $PLATFORM executable"
case "$PLATFORM" in
    windows) "$HERE"/wfinish.sh ;;
    linux)   "$HERE"/lfinish.sh ;;
    os-x)    "$HERE"/finish.sh  ;;
esac
