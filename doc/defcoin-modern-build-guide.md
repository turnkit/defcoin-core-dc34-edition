# Defcoin Core 2026.1 Build Guide

Last updated: 2026-05-09

This document records the practical build steps used for Defcoin Core 2026.1 on
modern development machines. It is focused on the standard Defcoin Core build.

The source baseline is Litecoin Core `v0.21.5.5`, adapted for the Defcoin
network and Defcoin packaging. The standard build should not contain
experimental visualization features, alternate artwork, or alternate package
names.

## Apple Silicon macOS Tahoe

Verified target:

- macOS Tahoe `26.4.x`
- Apple Silicon / `arm64`
- Xcode command line tools and full Xcode installed
- Homebrew installed at `/opt/homebrew`
- Qt 5 from Homebrew
- Berkeley DB 4.8 from Homebrew
- Homebrew `boost@1.85`

Required tools and libraries:

```sh
brew install autoconf automake libtool pkg-config gnu-sed coreutils \
  qt@5 berkeley-db@4 sqlite qrencode openssl@3 cmake imagemagick \
  boost@1.85 fmt libevent
python3 -m pip install --user dmgbuild pillow
```

`dmgbuild` is used by the macOS packaging script to write Finder presentation
metadata for the installer DMG directly, including the background image and
icon positions. If it is not available, the script falls back to Finder
AppleScript metadata setup.

Configure the standard build:

```sh
export BOOST_ROOT=/opt/homebrew/opt/boost@1.85
export PKG_CONFIG_PATH=/opt/homebrew/opt/fmt/lib/pkgconfig:/opt/homebrew/opt/qt@5/lib/pkgconfig:/opt/homebrew/opt/libevent/lib/pkgconfig:/opt/homebrew/opt/openssl@3/lib/pkgconfig:/opt/homebrew/opt/sqlite/lib/pkgconfig:/opt/homebrew/opt/qrencode/lib/pkgconfig:/opt/homebrew/opt/miniupnpc/lib/pkgconfig:/opt/homebrew/lib/pkgconfig

./autogen.sh
./configure \
  --with-gui=qt5 \
  --enable-wallet \
  --with-sqlite=yes \
  --with-miniupnpc \
  --disable-zmq \
  --disable-tests \
  --disable-bench \
  --with-boost="${BOOST_ROOT}" \
  CPPFLAGS="-I${BOOST_ROOT}/include -I/opt/homebrew/opt/berkeley-db@4/include -I/opt/homebrew/opt/fmt/include -I/opt/homebrew/opt/libevent/include -I/opt/homebrew/opt/openssl@3/include -I/opt/homebrew/opt/sqlite/include -I/opt/homebrew/opt/qrencode/include -I/opt/homebrew/opt/miniupnpc/include" \
  LDFLAGS="-L${BOOST_ROOT}/lib -L/opt/homebrew/opt/berkeley-db@4/lib -L/opt/homebrew/opt/fmt/lib -L/opt/homebrew/opt/libevent/lib -L/opt/homebrew/opt/openssl@3/lib -L/opt/homebrew/opt/sqlite/lib -L/opt/homebrew/opt/qrencode/lib -L/opt/homebrew/opt/miniupnpc/lib" \
  BDB_CFLAGS="-I/opt/homebrew/opt/berkeley-db@4/include" \
  BDB_LIBS="-L/opt/homebrew/opt/berkeley-db@4/lib -ldb_cxx-4.8"
```

Build and package:

```sh
make -j"$(sysctl -n hw.ncpu)"
contrib/defcoin-smoke-test.sh
make appbundle
DEFCOIN_PACKAGE_THEMES=0 ARCH_LABEL=arm \
  DMG_NAME=Defcoin-Core-v2026.1-arm.dmg \
  DEFCOIN_DMG_BG_WIDTH=520 DEFCOIN_DMG_BG_HEIGHT=380 \
  ./contrib/defcoin-macos-package-dmg.sh v2026.1
```

The release DMG should be named:

```text
Defcoin-Core-v2026.1-arm.dmg
```

### Apple Silicon fixes needed on Tahoe

These were the build-environment changes needed beyond the Defcoin chain
conversion:

- Qt 5 must be selected explicitly. Current Homebrew installs Qt 6 by default
  unless `qt@5` is requested.
- Boost `1.85` is supported by this branch after compatibility updates for
  `boost::filesystem` copy options and recursive-directory iterator APIs.
- Berkeley DB 4.8 must be selected explicitly for legacy wallet compatibility.
- UPnP is compiled in with Homebrew miniUPnPc. The source carries the newer
  `UPNP_GetValidIGD` call signature and the macOS packaging script bundles
  `libminiupnpc` into the app so the release bundle has no `/opt/homebrew`
  runtime dependency.
- Tests and benchmarks are disabled for the release packaging build to keep the
  local packaging loop focused on the GUI and daemon artifacts.

## Intel macOS

Verified target from this Apple Silicon build host:

- cross-built `x86_64-apple-darwin`
- same live Xcode macOS SDK used by the Apple Silicon build
- depends-built Qt `5.9.8` and supporting libraries
- Python `3.13` from `/Library/Frameworks/Python.framework/Versions/3.13/bin`

Important Intel-specific fixes:

- Put Python `3.13` before Homebrew Python in `PATH`. Homebrew Python `3.14`
  on this machine did not expose a compatible `setuptools.setup` namespace for
  `native_biplist`.
- Patch the Darwin depends builder so `x86_64-apple-darwin` compilers include
  `-arch x86_64`. Without that, Apple Silicon clang silently emits ARM64
  dependency libraries.
- Use a repo-local `TMPDIR`; large temporary builds on the system temp volume
  are easier to lose or collide with.
- The resulting Intel GUI is statically linked against the depends Qt stack, so
  DMG packaging must tolerate a static-Qt app and avoid injecting ARM64
  Homebrew Qt frameworks.

Build depends:

```sh
export CLEAN_PATH=/Library/Frameworks/Python.framework/Versions/3.13/bin:/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
mkdir -p .tmp-build

TMPDIR="$PWD/.tmp-build" PATH="$CLEAN_PATH" \
make -C depends \
  HOST=x86_64-apple-darwin \
  FORCE_USE_SYSTEM_CLANG=1 \
  OSX_SDK=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk \
  -j4
```

Configure, build, test, and package:

```sh
make distclean || true

TMPDIR="$PWD/.tmp-build" PATH="$CLEAN_PATH" \
CONFIG_SITE="$PWD/depends/x86_64-apple-darwin/share/config.site" \
./configure --prefix=/ --disable-tests --disable-bench

TMPDIR="$PWD/.tmp-build" PATH="$CLEAN_PATH" make -j"$(sysctl -n hw.ncpu)"

DEFCOIN_EXPECT_ARCH=x86_64 arch -x86_64 ./contrib/defcoin-smoke-test.sh
TMPDIR="$PWD/.tmp-build" PATH="$CLEAN_PATH" make appbundle

DEFCOIN_PACKAGE_THEMES=0 ARCH_LABEL=intel \
  DMG_NAME=Defcoin-Core-v2026.1-intel.dmg \
  DEFCOIN_DMG_BG_WIDTH=520 DEFCOIN_DMG_BG_HEIGHT=380 \
  ./contrib/defcoin-macos-package-dmg.sh v2026.1
```

The release DMG should be renamed to:

```text
Defcoin-Core-v2026.1-intel.dmg
```

## Windows 11 Cross-Build From macOS

The Windows build is cross-compiled from macOS using MinGW and the inherited
depends tree.

Install tools:

```sh
brew install mingw-w64 nsis gnu-sed coreutils automake autoconf libtool cmake
```

Important: use a clean `PATH` with no entries containing spaces. Old configure
scripts used by dependency packages can misparse paths such as
`/Applications/Topaz Photo AI.app/...`.

```sh
export CLEAN_PATH=/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
PATH="$CLEAN_PATH" make -C depends HOST=x86_64-w64-mingw32 -j1
```

Then configure and build:

```sh
PATH="$CLEAN_PATH" ./autogen.sh
CONFIG_SITE="$PWD/depends/x86_64-w64-mingw32/share/config.site" \
PATH="$CLEAN_PATH" ./configure \
  --prefix=/ \
  --disable-tests \
  --disable-bench \
  --disable-zmq \
  --disable-hardening

PATH="$CLEAN_PATH" make -j"$(sysctl -n hw.ncpu)"
```

`--disable-hardening` is currently required with the Homebrew MinGW 14 toolchain
used on this Mac. Without it, the final Windows link can fail on a duplicate
`__stack_chk_fail` symbol. Keep this in the Windows build notes until the
toolchain or hardening flags are revisited.

Package Windows artifacts with `zip -X` so macOS metadata is not included:

```sh
find staging-win64 -name .DS_Store -delete
find staging-win64 -name __MACOSX -type d -prune -exec rm -rf {} +
zip -r -X Defcoin-Core-v2026.1-win64.zip staging-win64
```

The Windows ZIP must not contain `.DS_Store` files or `__MACOSX` folders.

Before producing a Windows build, wait until the current Apple Silicon build has
been checked and approved. When staging Windows artifacts locally, use a
timestamped folder using this naming template:

```text
windows11-x86_64-standard-YYYYMMDD_HHMM/
```

## Standard Build Boundary

The public standard release uses the default build configuration. Do not enable
presentation or graphing feature flags when building the official standard
package.
