# macOS Build Instructions and Notes

The commands in this guide should be executed in a Terminal application.
The built-in one is located in
```
/Applications/Utilities/Terminal.app
```

## Preparation
Install the macOS command line tools:

```shell
xcode-select --install
```

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

## Dependencies
```shell
brew install automake libtool pkg-config python libevent qrencode fmt sqlite berkeley-db@4 qt@5
```

If you run into issues, check [Homebrew's troubleshooting page](https://docs.brew.sh/Troubleshooting).
See [dependencies.md](dependencies.md) for a complete overview.

If you want to build the disk image with `make deploy` (.dmg / optional), you need RSVG:
```shell
brew install librsvg
```

The wallet support requires one or both of the dependencies ([*SQLite*](#sqlite) and [*Berkeley DB*](#berkeley-db)) in the sections below.
To build Bitcoin Core without wallet, see [*Disable-wallet mode*](#disable-wallet-mode).

#### SQLite

Usually, macOS installation already has a suitable SQLite installation.
Also, the Homebrew package could be installed:

```shell
brew install sqlite
```

In that case the Homebrew package will prevail.

#### Berkeley DB

It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use [this](/contrib/install_db4.sh) script to install it
like so:

```shell
./contrib/install_db4.sh .
```

from the root of the repository.

Also, the Homebrew package could be installed:

```shell
brew install berkeley-db@4
```

## Build Defcoin Core

### Native Apple Silicon GUI build used for this port

These commands were used on Apple Silicon with Homebrew installed under
`/opt/homebrew`. They keep Berkeley DB wallet support enabled and build the Qt
GUI. This path was verified on macOS Tahoe 26.4.x.

The current detailed Defcoin build guide is
[`defcoin-modern-build-guide.md`](defcoin-modern-build-guide.md). The short
Apple Silicon path below uses Homebrew Qt 5, Berkeley DB 4.8, libevent, fmt,
and `boost@1.85`.

```shell
git clone https://github.com/turnkit/defcoin-core.git
cd defcoin-core

brew install automake libtool pkg-config python libevent qrencode fmt sqlite berkeley-db@4 qt@5 boost@1.85

DEFCOIN_ROOT="$(pwd)"
DEFCOIN_BOOST_PREFIX="/opt/homebrew/opt/boost@1.85"

export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/fmt/lib/pkgconfig:/opt/homebrew/opt/qt@5/lib/pkgconfig:/opt/homebrew/opt/libevent/lib/pkgconfig:/opt/homebrew/opt/openssl@3/lib/pkgconfig:/opt/homebrew/opt/sqlite/lib/pkgconfig:/opt/homebrew/opt/qrencode/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export CPPFLAGS="-I${DEFCOIN_BOOST_PREFIX}/include -I/opt/homebrew/opt/berkeley-db@4/include -I/opt/homebrew/opt/fmt/include -I/opt/homebrew/opt/libevent/include -I/opt/homebrew/opt/openssl@3/include -I/opt/homebrew/opt/sqlite/include -I/opt/homebrew/opt/qrencode/include"
export LDFLAGS="-L${DEFCOIN_BOOST_PREFIX}/lib -L/opt/homebrew/opt/berkeley-db@4/lib -L/opt/homebrew/opt/fmt/lib -L/opt/homebrew/opt/libevent/lib -L/opt/homebrew/opt/openssl@3/lib -L/opt/homebrew/opt/sqlite/lib -L/opt/homebrew/opt/qrencode/lib"
export BDB_CFLAGS="-I/opt/homebrew/opt/berkeley-db@4/include"
export BDB_LIBS="-L/opt/homebrew/opt/berkeley-db@4/lib -ldb_cxx-4.8"

./autogen.sh
./configure --with-gui=qt5 --enable-wallet --with-sqlite=yes --without-miniupnpc --disable-zmq --disable-tests --disable-bench --with-boost="${DEFCOIN_BOOST_PREFIX}"
make -j"$(sysctl -n hw.ncpu)"
make appbundle
./contrib/defcoin-smoke-test.sh
```

To verify the app is native arm64:

```shell
file ./Defcoin-Qt.app/Contents/MacOS/Defcoin-Qt
file ./src/defcoind ./src/defcoin-cli ./src/qt/defcoin-qt
```

The smoke test uses temporary datadirs only and does not touch the default
wallet or `~/Library/Application Support/Defcoin/`.

Expected GUI outputs:

- `src/qt/defcoin-qt`
- `Defcoin-Qt.app`

Expected command-line outputs:

- `src/defcoind`
- `src/defcoin-cli`
- `src/defcoin-tx`
- `src/defcoin-wallet`

The app bundle is unsigned and locally ad-hoc signed by the normal macOS build
tooling. For redistribution, add a separate signing, notarization, and DMG
release process.

### Apple Silicon DMG packaging

After `make appbundle` succeeds, this branch can create a local Apple Silicon
DMG with a bundled Qt runtime:

```shell
./contrib/defcoin-macos-package-dmg.sh
```

The script creates:

```text
Defcoin-Core-v2026.1-arm-adhoc.dmg
```

What the script does:

- Runs `contrib/macdeploy/macdeployqtplus` against `Defcoin-Qt.app`.
- Uses `pt_BR` rather than `pt` in the Qt translation list, because the
  Homebrew Qt 5.15.18 package does not ship `qt_pt.qm`.
- Prunes Qt plugin families not used by the wallet, including QtQuick3D,
  WebGL, generic touch, and renderer plugins.
- Normalizes the copied Qt framework layout so modern macOS `codesign` accepts
  the bundle.
- Ad-hoc signs the nested frameworks, plugins, binaries, and `.app`.
- Verifies no Mach-O dependency still points at `/opt/homebrew`.
- Creates a drag-to-Applications DMG and verifies it with `hdiutil verify`.

This is a developer distribution image. It is not notarized and is not signed
with a Developer ID certificate. A public release should be signed with an
Apple Developer ID Application identity and submitted for notarization before
distribution.

Do not use the inherited `make deploy` fancy DMG path for this local Apple
Silicon package. It can hang in Finder/AppleScript automation on newer macOS
versions and, with Homebrew Qt 5.15.18, can fail when looking for the missing
`qt_pt.qm` translation file.

The current Apple Silicon release DMG should be staged outside the source tree
and uploaded to a GitHub Release as:

```text
Defcoin-Core-v2026.1-macOS-AppleSilicon.dmg
```

### Troubleshooting

- If Qt is not found, confirm `PATH` contains `/opt/homebrew/opt/qt@5/bin` and
  `PKG_CONFIG_PATH` contains `/opt/homebrew/opt/qt@5/lib/pkgconfig`.
- If Berkeley DB is not found, reinstall `berkeley-db@4` and keep `BDB_CFLAGS`
  and `BDB_LIBS` set as shown above.
- If Boost is not found, confirm `brew --prefix boost@1.85` resolves and that
  `CPPFLAGS` / `LDFLAGS` point at the matching include and library paths.
- If the binary reports `x86_64`, your shell or Homebrew install is likely
  running under Rosetta. `arch` should print `arm64`, and `brew --prefix`
  should print `/opt/homebrew`.

1. Clone the Defcoin Core source code:
    ```shell
    git clone https://github.com/turnkit/defcoin-core
    cd defcoin-core
    ```

2.  Build Defcoin Core:

    Configure and build the headless Defcoin Core binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.
    ```shell
    ./autogen.sh
    ./configure
    make
    ```

3.  It is recommended to build and run the unit tests:
    ```shell
    make check
    ```

4.  You can also create a  `.dmg` that contains the `.app` bundle (optional):
    ```shell
    make deploy
    ```

## Disable-wallet mode
When the intention is to run only a P2P node without a wallet, Defcoin Core may be
compiled in disable-wallet mode with:
```shell
./configure --disable-wallet
```

In this case there is no dependency on [*Berkeley DB*](#berkeley-db) and [*SQLite*](#sqlite).

Mining is also possible in disable-wallet mode using the `getblocktemplate` RPC call.

## Running
Defcoin Core is now available at `./src/defcoind`

Before running, you may create an empty configuration file:
```shell
mkdir -p "$HOME/Library/Application Support/Defcoin"

touch "$HOME/Library/Application Support/Defcoin/defcoin.conf"

chmod 600 "$HOME/Library/Application Support/Defcoin/defcoin.conf"
```

The first time you run defcoind, it will start downloading the blockchain. This process could
take many hours, or even days on slower than average systems.

You can monitor the download process by looking at the debug.log file:
```shell
tail -f $HOME/Library/Application\ Support/Defcoin/debug.log
```

## Other commands:
```shell
./src/defcoind -daemon      # Starts the defcoin daemon.
./src/defcoin-cli --help    # Outputs a list of command-line options.
./src/defcoin-cli help      # Outputs a list of RPC commands when the daemon is running.
```

## Notes
* The Defcoin Apple Silicon instructions above were verified on macOS Tahoe
26.4.1 arm64.
* The older upstream note about OS X 10.14 Mojave through macOS 11 Big Sur on
64-bit Intel applies to inherited Litecoin/Bitcoin documentation, not to this
new Defcoin Apple Silicon port.
* Building with downloaded Qt binaries is not officially supported. See the
notes in [#7714](https://github.com/bitcoin/bitcoin/issues/7714).
