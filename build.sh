#!/bin/bash
set -e

# based off of: https://github.com/sys-bio/libantimonyjs/blob/develop/.github/workflows/build-libantimonyjs-github-actions.yml

INSTALL_DIR="$(pwd)/install"

activate-emsdk() {
    echo "##### Activating emsdk #####"
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh
}

# use () instead of {} to create a subshell and not propagate the `cd`
expat() (
    echo "##### Building libexpat #####"
    cd libexpat
    emcmake cmake . "-DCMAKE_INSTALL_PREFIX=$INSTALL_DIR/expat"
    emmake make
    emmake make install
)

sbml() (
    echo "##### Building libsbml #####"
    cd ./libsbml
    emcmake cmake . \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/sbml" \
        -DCMAKE_BUILD_TYPE=Release -DWITH_CPP_NAMESPACE=ON \
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
    cd ./libsbmlsim
    emcmake cmake . \
        "-DLIBSBML_INCLUDE_DIR=$INSTALL_DIR/sbml/include" \
        "-DLIBSBML_LIBRARY=$INSTALL_DIR/sbml"
    emmake make
    emmake make install
)

# main
mkdir -p "$INSTALL_DIR"

activate-emsdk
expat && sbml && sbml-sim