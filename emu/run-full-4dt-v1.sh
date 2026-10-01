#!/bin/sh

set -eu

: "${TMPDIR:?TMPDIR must be set}"

root="${TMPDIR}/pdp6-aap-v1-4dt-$$"
bindir="/usr/local/libexec/pdp6-aap-v1"
sharedir="/usr/local/share/pdp6-aap-v1"

cleanup()
{
        rm -rf "${root}"
}

trap cleanup EXIT HUP INT TERM
mkdir -p "${root}/emu"
ln -s "${sharedir}/art" "${root}/art"
ln -s "${sharedir}/init-full-4dt-v1.ini" "${root}/emu/init.ini"
cd "${root}/emu"
"${bindir}/pdp6-aap-v1" "$@"
