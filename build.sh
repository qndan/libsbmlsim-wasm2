#!/bin/bash
set -e

# based off of: https://github.com/sys-bio/libantimonyjs/blob/develop/.github/workflows/build-libantimonyjs-github-actions.yml

INSTALL_DIR="$(pwd)/install"
BUILD_DIR="$(pwd)/build"

activate-emsdk() {
    echo "##### Activating emsdk #####"
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh
    cd ..
}

# use () instead of {} to create a subshell and not propagate the `cd`
expat() (
    echo "##### Building libexpat #####"
    cd libexpat/expat
    emcmake cmake . \
        "-DCMAKE_INSTALL_PREFIX=$INSTALL_DIR/expat" \
        -DEXPAT_SHARED_LIBS=OFF
    emmake make
    emmake make install
)

sbml() (
    echo "##### Building libsbml #####"
    cd libsbml
    # in-source builds of libsbml are disabled
    mkdir -p build
    cd build
    emcmake cmake .. \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/sbml" \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_CPP_NAMESPACE=OFF \
        -DWITH_EXPAT=ON \
        -DWITH_LIBXML=OFF \
        -DLIBSBML_SKIP_SHARED_LIBRARY=ON \
        -DENABLE_ARRAYS=ON \
        -DENABLE_COMP=ON \
        -DENABLE_DISTRIB=ON \
        -DENABLE_FBC=ON \
        -DENABLE_GROUPS=ON \
        -DENABLE_MULTI=ON \
        -DENABLE_QUAL=ON \
        -DWITH_STABLE_PACKAGES=ON \
        -DWITH_SWIG=OFF \
        -DWITH_ZLIB=OFF \
        "-DEXPAT_INCLUDE_DIR=$INSTALL_DIR/expat/include" \
        "-DEXPAT_LIBRARY=$INSTALL_DIR/expat/lib/libexpat.a"
    emmake make
    emmake make install
)

sbml-sim() (
    echo "##### Building libsbmlsim #####"
    cd libsbmlsim
    emcmake cmake . \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/sbmlsim" \
        "-DLIBSBML_INCLUDE_DIR=$INSTALL_DIR/sbml/include" \
        "-DLIBSBML_LIBRARY=$INSTALL_DIR/sbml/lib/libsbml-static.a" \
        "-DLIBEXPAT_LIBRARY=$INSTALL_DIR/expat/lib/libexpat.a"
    emmake make
    emmake make install
)

# make a wasm from the libsbmlsim
wasm-lib() {
    echo "##### Creating WASM #####"
    emcc -Oz \
        -sDISABLE_EXCEPTION_CATCHING=0 \
        -sMODULARIZE=1 \
        -sSINGLE_FILE=1 \
        -sEXPORT_NAME=libsbmlsim \
        -sALLOW_MEMORY_GROWTH=1 \
        "-I$INSTALL_DIR/sbmlsim/include" \
        "-I$INSTALL_DIR/sbml/include" \
        "-I$INSTALL_DIR/expat/include" \
        "$INSTALL_DIR/sbmlsim/lib/libsbmlsim-static.a" \
        "$INSTALL_DIR/sbml/lib/libsbml-static.a" \
        "$INSTALL_DIR/expat/lib/libexpat.a" \
        "wrapper.cpp" \
        -o libsbmlsim.js \
        -lembind
}

# main
echo "##### Starting build #####"
mkdir -p "$INSTALL_DIR"
mkdir -p "$BUILD_DIR"

activate-emsdk
# expat && sbml && sbml-sim
wasm-lib

echo "##### Build complete #####"
