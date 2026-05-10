Building Defcoin
================

See doc/build-*.md for instructions on building the various
elements of the Defcoin Core reference implementation of Defcoin.

For the current Apple Silicon GUI port on macOS Tahoe 26.4.1, start with a
fresh clone and Homebrew installed under `/opt/homebrew`:

```sh
git clone https://github.com/turnkit/defcoin-core.git
cd defcoin-core

brew install automake libtool pkg-config python libevent qrencode fmt sqlite berkeley-db@4 qt@5

./contrib/build-boost-arm64.sh

DEFCOIN_ROOT="$(pwd)"
DEFCOIN_BOOST_PREFIX="${DEFCOIN_ROOT}/.defcoin-build-deps/boost-1.84.0-arm64"

export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/qt@5/lib/pkgconfig:/opt/homebrew/opt/qrencode/lib/pkgconfig:/opt/homebrew/opt/sqlite/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export CPPFLAGS="-include ${DEFCOIN_ROOT}/contrib/apple-sdk-le32-shim.h -I/opt/homebrew/include"
export LDFLAGS="-L${DEFCOIN_BOOST_PREFIX}/lib -L/opt/homebrew/lib"
export BDB_CFLAGS="-I/opt/homebrew/opt/berkeley-db@4/include"
export BDB_LIBS="-L/opt/homebrew/opt/berkeley-db@4/lib -ldb_cxx-4.8"

./autogen.sh
./configure --with-gui=qt5 --enable-wallet --with-sqlite=yes --without-miniupnpc --disable-zmq --disable-tests --disable-bench --with-boost="${DEFCOIN_BOOST_PREFIX}" --with-boost-libdir="${DEFCOIN_BOOST_PREFIX}/lib"
make -j"$(sysctl -n hw.ncpu)"
make appbundle
./contrib/defcoin-smoke-test.sh
```

The known-good local build uses Homebrew dependencies from `/opt/homebrew`,
Berkeley DB 4.8 for legacy wallet support, Qt5 for `defcoin-qt`, and a local
Boost 1.84 tree because current Homebrew Boost is newer than this Litecoin Core
0.21.x build system expects.
