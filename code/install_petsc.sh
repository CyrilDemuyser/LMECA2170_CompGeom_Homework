#!/bin/sh
# Downloads, builds and installs a local copy of PETSc in ./external/petsc,
# next to this script. Nothing is installed elsewhere on your system: to remove
# it, delete the ./external directory.
#
# PETSc is only needed by the fluid solver (src/project_solver.c). It is built
# sequential (no MPI) and without Fortran: a C compiler, make and python3 are
# enough. Expect 10 to 20 minutes.
#
# Usage: ./install_petsc.sh        (run it from anywhere)
set -e

PETSC_VERSION=3.25.6
URL=https://web.cels.anl.gov/projects/petsc/download/release-snapshots/petsc-$PETSC_VERSION.tar.gz

ROOT=$(cd "$(dirname "$0")" && pwd)
EXTERNAL=$ROOT/external
SRC=$EXTERNAL/petsc-$PETSC_VERSION
PREFIX=$EXTERNAL/petsc
ARCH=arch-lmeca2170

for tool in cc make python3; do
    command -v $tool >/dev/null 2>&1 || { echo "error: '$tool' not found, install it first"; exit 1; }
done

if [ -f "$PREFIX/include/petsc.h" ]; then
    echo "PETSc is already installed in $PREFIX (delete it to reinstall)"
    exit 0
fi

mkdir -p "$EXTERNAL"
cd "$EXTERNAL"

if [ ! -d "$SRC" ]; then
    echo "== Downloading PETSc $PETSC_VERSION"
    if command -v curl >/dev/null 2>&1; then
        curl -fL -o petsc.tar.gz "$URL"
    else
        wget -O petsc.tar.gz "$URL"
    fi
    tar xzf petsc.tar.gz
    rm petsc.tar.gz
fi

cd "$SRC"
echo "== Configuring PETSc"
python3 ./configure \
    PETSC_ARCH=$ARCH \
    --prefix="$PREFIX" \
    --with-mpi=0 \
    --with-fc=0 \
    --with-cxx=0 \
    --with-x=0 \
    --with-debugging=0 \
    --with-shared-libraries=1 \
    --download-f2cblaslapack \
    COPTFLAGS=-O2

echo "== Building PETSc"
make PETSC_DIR="$SRC" PETSC_ARCH=$ARCH all
make PETSC_DIR="$SRC" PETSC_ARCH=$ARCH install

echo "== Testing the installation"
make PETSC_DIR="$PREFIX" PETSC_ARCH= check

# Only the installed copy is needed from now on
cd "$EXTERNAL"
rm -rf "$SRC"

echo
echo "PETSc $PETSC_VERSION is installed in $PREFIX"
echo "Now (re)run cmake in your build directory: it finds this copy by itself."
