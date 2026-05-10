# Defcoin Porting Notes

## Base

- Modern base: Litecoin Core `v0.21.5.5` from `litecoin-project/litecoin`
  tag commit `d8c8adc0b46cf96cc713455ba4aeb135efabac86`.
- Current Defcoin release identity: `2026.1`, codename `Token Jester`.
- Target platform for this phase: native Apple Silicon macOS arm64.
- Build system: autotools with Qt5 GUI support.
- Reference repositories:
  - https://github.com/packetloss404/DefCoinCore
  - https://github.com/mspicer/defcoin

## First Defcoin Build Scope

The first Defcoin build focuses on native GUI and command-line binaries only.
The Local Mining UI is intentionally deferred until this build passes smoke
tests.

Expected outputs:

- `src/defcoind`
- `src/defcoin-cli`
- `src/defcoin-tx`
- `src/defcoin-wallet`
- `src/qt/defcoin-qt`
- `Defcoin-Qt.app`

## Consensus And Network Parameters

Values currently patched from the Defcoin compatibility references:

- Proof of work: Scrypt
- Target block spacing: 2 minutes
- Retarget interval: 720 blocks
- Subsidy halving interval: 840,000 blocks
- Initial subsidy: 50 DFC
- Mainnet P2P/RPC ports: 1337 / 9332
- Mainnet seed hosts:
  - `seed.defcoin.io`
  - `seed.defcoin.mikej.tech`
  - `seed.defcoin.dc903.org` currently represented in source as
    `seed.defcoin.dc903.org:10332`
  - `seed.defcoincore.org` (reserved/planned; safe to keep in the source before
    the DNS host is live)
- Testnet P2P/RPC ports: 31337 / 19332
- Regtest P2P/RPC ports: 19444 / 19443
- Mainnet legacy Base58 pubkey prefix: `D`
- Mainnet default data directory on macOS:
  `~/Library/Application Support/Defcoin/`
- Config file: `defcoin.conf`
- User-facing display units:
  - `DFC`
  - `Packet`
  - `Tock`
  - `Mote`

The detailed publication-facing Litecoin comparison is maintained in
`doc/defcoin-core-vs-litecoin-core.md`.

Compatibility rule: historical Defcoin mainnet behavior takes priority over
newer Litecoin feature activation. Mainnet should not treat SegWit, CSV, CLTV,
BIP66, Taproot, or MWEB as active consensus features unless a future migration
plan deliberately changes that.

## Wallet Compatibility

Historical Defcoin wallets are expected to be Berkeley DB `wallet.dat` wallets.
This branch is configured with Berkeley DB 4.8 and SQLite support so inherited
legacy and descriptor wallet code can both compile.

Operational warnings before testing old wallets:

- Back up `wallet.dat` first.
- Shut down the old Defcoin node cleanly.
- Start with a copied data directory or a copied wallet.
- Expect rescans or reindexing on first startup.
- Pruned nodes may need a manual `-reindex`.
- Do not silently migrate old wallets.

## GUI Branding

User-facing names and binaries have been changed to Defcoin naming. macOS app
metadata now uses:

- App bundle: `Defcoin-Qt.app`
- Executable: `Defcoin-Qt`
- Bundle identifier: `io.defcoin.Defcoin-Qt`
- URL scheme: `defcoin:`
- App icon: `src/qt/res/icons/defcoin.icns`

Runtime artwork required by the build is kept in:

- `src/qt/res/icons/`
- `src/qt/res/themes/`

Development-only source artwork, rejected variants, and scratch graphics are
intentionally kept outside the public repository. The modder
`custom_themes/example_theme` folder tree is generated in the user's Defcoin
data directory at runtime so the app bundle does not ship duplicate default
theme graphics. If final production artwork changes, regenerate only the
runtime assets needed by the app and commit those outputs.

The About dialog now credits Defcoin Core, Litecoin Core, and Bitcoin Core
derivation and lists the current mainnet seed hostnames.

## Mining UI Architecture

Mining is skipped in the first Defcoin build.

The planned second build should add a dedicated `Local Mining` GUI menu/panel
without embedding miner code into consensus or wallet internals. The preferred
architecture is an external miner wrapper:

- Select an existing wallet receive address when solo mining.
- Support solo mining against the local node via RPC/getblocktemplate.
- Support pool mining fields for Stratum URL, worker/user, and password.
- Mask passwords in logs and previews.
- Store miner profiles locally.
- Capture stdout/stderr from an external miner process.
- Start with CPU Scrypt mining as the practical Apple Silicon target.
- Treat Apple Silicon GPU mining as experimental unless a maintained
  Metal-compatible Scrypt miner is verified.
- Treat USB ASIC support as an external-binary wrapper.

Before calling the second build complete, inspect the Peers screen and confirm
the user agent shows Defcoin nodes only. The local node user agent should be
`/DefcoinCore:2026.1/`.

## Known TODOs Before Mainnet Use

- Validate mainnet header sync against existing Defcoin peers.
- Confirm the patched genesis, checkpoints, seeds, and message-start bytes
  against multiple historical references.
- Keep peer service-bit requirements aligned with historical Defcoin nodes.
  The current port does not require Litecoin Witness/MWEB peer service bits.
- Hide or remove inherited signet exposure from user-facing Defcoin builds.
- Review remaining Litecoin strings in tests, packaging, examples, and old
  release documentation.
- Decide whether Bech32 UI/RPC exposure should be suppressed further even
  though mainnet SegWit/MWEB consensus activation is disabled.
- Exercise old copied `wallet.dat` files in an isolated data directory.
