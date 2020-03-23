#!/bin/bash
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

_DIST="$1"
_RELEASE="$2"

PKG_DIR=$(pwd)

function die()
{
  echo $*
  exit 1
}

[[ ! -z "$_DIST" || ! -z "$_RELEASE" ]] || die "Usage: $0 <dist-tarball> <ubuntu-release>"
[[ -f "$_DIST" ]] || die "Can't find dist package [$_DIST]."
[[ -d packaging ]] || die "Run me from the top-level packaging directory."
[[ "$_DIST" == "${_DIST%%.tar.gz}.tar.gz" ]] || die "Tarball name needs to end in .tar.gz."

pushd $(dirname $_DIST) >/dev/null || die "Can't enter dist dir."
_DIST=$(pwd)/$(basename $_DIST)
popd >/dev/null

mkdir -p packaging-output || die "Can't create output dir."

rm -rf packaging-build
mkdir -p packaging-build || die "Can't create build dir."
cd packaging-build || die "Can't enter build dir."

TAR_NAME=$(basename $_DIST)
TAR_NAME=${TAR_NAME%%.tar.gz}
VERSION=${TAR_NAME##*-}
TAR_NAME=${TAR_NAME%-*}
cp $_DIST ${TAR_NAME}_${VERSION}.orig.tar.gz || die "Failed to copy tarball."

tar xfz $_DIST || die "Failed to unpack tarball."
cd $(find . -mindepth 1 -maxdepth 1 -type d) || die "Failed to enter source dir."

cp -r $PKG_DIR/packaging/ubuntu-$_RELEASE ./debian || die "Can't copy packaging files."
cd debian || die "Can't enter packaging directory (debian)."

debuild -S -sa || die "debuild (source) failed."

cd $PKG_DIR

mv packaging-build/*.tar.[gx]z packaging-output/
mv packaging-build/*.dsc packaging-output/
mv packaging-build/*.build packaging-output/
mv packaging-build/*.buildinfo packaging-output/
mv packaging-build/*.changes packaging-output/

rm -rf packaging-build
