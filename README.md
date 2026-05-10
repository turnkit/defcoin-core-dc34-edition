Defcoin Core
============

Defcoin Core is a full node and wallet implementation for the Defcoin network.
This release line is based on Litecoin Core v0.21.5.5 source and carries the
Defcoin chain, seed, wallet, branding, and packaging changes needed for native
Defcoin builds.

The current development release identity is `2026.1`, codename `Token Jester`.
Treat the repository as release-candidate work until the GitHub publication
checklist and mainnet wallet compatibility checks are complete.

Key network facts
-----------------

- Proof of work: Scrypt
- Target block time: 2 minutes
- Difficulty retarget interval: 720 blocks
- Subsidy halving interval: 840,000 blocks
- Initial block reward: 50 DFC
- Mainnet P2P/RPC ports: 1337 / 9332
- Mainnet seed hosts: `seed.defcoin.io`, `seed.defcoin.mikej.tech`,
  `seed.defcoin.dc903.org`
- Testnet P2P/RPC ports: 31337 / 19332
- Regtest P2P/RPC ports: 19444 / 19443
- macOS data directory: `~/Library/Application Support/Defcoin/`
- Config file: `defcoin.conf`
- Display units: `DFC`, `Packet`, `Tock`, `Mote`

Compatibility notes
-------------------

This port follows the Defcoin compatibility guidance from
`packetloss404/DefCoinCore`: preserve the historical chain rules first, and do
not blindly enable newer Litecoin mainnet consensus features.

- Mainnet should use legacy Base58 addresses.
- SegWit, CSV, CLTV, BIP66, Taproot, and MWEB are not treated as active Defcoin
  mainnet consensus features in this branch.
- Berkeley DB wallet support is enabled for legacy `wallet.dat` compatibility.
- Back up old wallets before testing them with this software.
- Signet is inherited from Litecoin Core internals but is not a supported
  Defcoin network target yet.

Build
-----

See [INSTALL.md](INSTALL.md), [doc/build-osx.md](doc/build-osx.md), and
[doc/defcoin-modern-build-guide.md](doc/defcoin-modern-build-guide.md).
The current known-good Apple Silicon GUI build was verified on macOS Tahoe
26.4.x with Homebrew under `/opt/homebrew`, Qt 5.15.18, Berkeley DB 4.8,
libevent 2.1.12, and Homebrew `boost@1.85`.

For Defcoin-specific consensus, wallet, GUI, and porting notes, see:

- [doc/defcoin-porting-notes.md](doc/defcoin-porting-notes.md)
- [doc/defcoin-core-vs-litecoin-core.md](doc/defcoin-core-vs-litecoin-core.md)
- [doc/defcoin-build-deviation-matrix.md](doc/defcoin-build-deviation-matrix.md)
- [doc/defcoin-litecoin-conversion-notes.md](doc/defcoin-litecoin-conversion-notes.md)
- [doc/defcoin-modern-build-guide.md](doc/defcoin-modern-build-guide.md)
- [doc/litecoin-upstream-comparison-standard.md](doc/litecoin-upstream-comparison-standard.md)
- [doc/github-publication-checklist.md](doc/github-publication-checklist.md)

Native Apple Silicon outputs from this branch are expected to be:

- `src/defcoind`
- `src/defcoin-cli`
- `src/defcoin-tx`
- `src/defcoin-wallet`
- `src/qt/defcoin-qt`
- `Defcoin-Qt.app`

Run the focused smoke test after building:

```sh
./contrib/defcoin-smoke-test.sh
```

To create a local Apple Silicon DMG after `make appbundle`:

```sh
./contrib/defcoin-macos-package-dmg.sh
```

The generated DMG is an ad-hoc signed developer distribution image, not a
Developer ID signed or notarized public release.

The Apple Silicon standard release DMG is named:

```text
Defcoin-Core-v2026.1-macOS-AppleSilicon.dmg
```

Release artifacts are attached to GitHub Releases rather than committed to
source history.

For the current design and porting notes, see:

- [doc/defcoin-core-help-manual.md](doc/defcoin-core-help-manual.md)
- [doc/defcoin-third-party-notices-standard.txt](doc/defcoin-third-party-notices-standard.txt)

Runtime Defcoin artwork and generated Qt/macOS assets needed by the standard
build are tracked in the repository under `src/qt/res/icons/`.

License
-------

Defcoin Core is released under the terms of the MIT license. See
[COPYING](COPYING) for more information or see
https://opensource.org/licenses/MIT.

Copyright (C) 2014-2026 The Defcoin Core developers.

Defcoin Core is derived from Litecoin Core and Bitcoin Core. Portions remain
credited to The Litecoin Core developers and The Bitcoin Core developers.
