#!/bin/bash

_DIST=$1

PKG_DIR=$(pwd)

function die()
{
  echo $*
  exit 1
}

[[ ! -z "$_DIST" ]] || die "Usage: $0 <dist-tarball>"
[[ -f "$_DIST" ]] || die "Can't find dist package [$_DIST]."
[[ -d debian ]] || die "Run me from the top-level packaging directory."
[[ "$_DIST" == "${_DIST%%.tar.gz}.tar.gz" ]] || die "Tarball name needs to end in .tar.gz."

pushd $(dirname $_DIST) >/dev/null || die "Can't enter dist dir."
_DIST=$(pwd)/$(basename $_DIST)
popd >/dev/null

mkdir -p output || die "Can't create output dir."

rm -rf ubuntu-build
mkdir -p ubuntu-build || die "Can't create build dir."
cd ubuntu-build || die "Can't enter build dir."

TAR_NAME=$(basename $_DIST)
TAR_NAME=${TAR_NAME%%.tar.gz}
VERSION=${TAR_NAME##*-}
TAR_NAME=${TAR_NAME%-*}
cp $_DIST ${TAR_NAME}_${VERSION}.orig.tar.gz || die "Failed to copy tarball."

tar xfz $_DIST || die "Failed to unpack tarball."
cd $(find . -mindepth 1 -maxdepth 1 -type d) || die "Failed to enter source dir."

cp -r $PKG_DIR/debian . || die "Can't copy debian files."
cd debian || die "Can't enter debian"

debuild -S -sa || die "debuild (source) failed."

cd $PKG_DIR

mv ubuntu-build/*.tar.[gx]z output/
mv ubuntu-build/*.dsc output/
mv ubuntu-build/*.build output/
mv ubuntu-build/*.buildinfo output/
mv ubuntu-build/*.changes output/

rm -rf ubuntu-build
