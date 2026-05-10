# Defcoin Core Standard Conversion Notes

Last updated: 2026-05-07

This document summarizes the source-level conversion work needed to turn the
Litecoin Core `v0.21.5.5` codebase into the standard Defcoin Core 2026.1
wallet/node. It intentionally excludes enhanced-only visualization and theme
work.

## Conversion Goals

- Preserve Litecoin Core's tested node, wallet, RPC, and Qt architecture.
- Replace Litecoin network identity with Defcoin network identity.
- Keep historical Defcoin mainnet compatibility ahead of newer Litecoin feature
  activation.
- Keep disabled inherited features visible enough that future maintainers know
  what is inactive, without presenting them as live Defcoin features.
- Keep the standard build compact and predictable.

## Required Source Changes

### Network And Chain Parameters

- Replace Litecoin genesis hashes and merkle roots with Defcoin values for
  mainnet, testnet, and regtest.
- Replace Litecoin checkpoints with Defcoin checkpoints.
- Change Defcoin mainnet target spacing to 120 seconds and target timespan to
  one day.
- Change Defcoin mainnet default P2P port to `1337` and RPC port to `9332`.
- Change Defcoin testnet P2P/RPC ports to `31337` / `19332`.
- Keep regtest isolated at inherited regtest ports.
- Replace Litecoin seed hosts with Defcoin seed hosts:
  `seed.defcoin.io`, `seed.defcoin.mikej.tech`, and
  `seed.defcoin.dc903.org`, plus the planned `seed.defcoincore.org` endpoint.
  The current source lists the dc903 seed endpoint with `:10332` because that
  reachable peer infrastructure advertises a non-default port.
- Replace fixed seeds with Defcoin fixed seed endpoints.
- Set Defcoin address prefixes:
  mainnet Base58 pubkey `30`, script `5`, second script `50`, secret key `176`,
  Bech32 `dfc`, and MWEB HRP `dfcmweb`.
- Set testnet and regtest Bech32 HRPs to Defcoin-specific values.
- Replace minimum-chain-work, assume-valid, chain transaction statistics, and
  assumed storage sizes with Defcoin-appropriate values.

### Consensus Feature Exposure

Defcoin mainnet keeps several inherited Litecoin deployments inactive in this
standard build:

- BIP65 / CLTV
- BIP66 strict DER signatures
- CSV
- SegWit
- Taproot
- MWEB
- Signet exposure

These choices are not cosmetic. They keep the port compatible with the observed
historical Defcoin network unless a future Defcoin release deliberately
activates newer rules.

### Proof Of Work

- Keep Litecoin's Scrypt proof-of-work implementation path.
- Keep Scrypt wiring in block validation and hashing code.
- Preserve inherited script, wallet, mempool, peer manager, and RPC flows unless
  a Defcoin compatibility change requires otherwise.

### Product Identity

- Rename user-facing product text to Defcoin Core.
- Rename binaries to `defcoind`, `defcoin-cli`, `defcoin-tx`,
  `defcoin-wallet`, and `defcoin-qt`.
- Set macOS bundle identity to `io.defcoin.Defcoin-Qt`.
- Set the URI scheme to `defcoin:`.
- Advertise the peer subversion as `/DefcoinCore:2026.1/`.
- Keep version `2026.1` and codename `Token Jester` as release identity.

### Wallet And Amount UI

- Keep Berkeley DB wallet support for historical `wallet.dat` compatibility.
- Keep SQLite support because the inherited codebase can build descriptor wallet
  paths.
- Replace display units with `DFC`, `Packet`, `Tock`, and `Mote`.
- Replace inherited Litecoin example addresses with valid Defcoin examples.
- Keep pruning warnings and wallet-rescan limitations explicit.

### Node UI Additions Kept In Standard

The standard build keeps only diagnostic controls that help Defcoin network
operation:

- show connected peers
- show inactive peers
- show banned peers
- discover local network nodes
- optionally reject non-Defcoin-prefixed user agents
- clearer peer and banned-peer display
- Preferences access from the main window
- Node window access from the main window

The standard build should not include the Peer Map, extra theme packs, party
artwork, animated peer health graphs, or other enhanced-only presentation
features.

## Build Conversion Work

The source conversion alone is not enough on modern macOS. The practical build
fixes are documented separately in `doc/defcoin-modern-build-guide.md`.

The short version is:

- use Qt 5 explicitly;
- use Berkeley DB 4.8 explicitly;
- prefer local Boost before Homebrew Boost in linker paths;
- disable UPnP unless the miniUPnPc call site is updated or an older compatible
  miniUPnPc is used;
- include the Apple SDK endian shim when compiling against the modern Tahoe SDK.

## Release Sanity Checks

Before publishing a standard build:

- `defcoin-qt --version` must show Defcoin Core `v2026.1`.
- `getnetworkinfo` must show `/DefcoinCore:2026.1/`.
- The standard splash, About dialog, filenames, and macOS metadata must not
  contain enhanced-build wording or artwork.
- The standard app bundle must not contain runtime theme packs.
- Windows ZIPs must not contain `.DS_Store` or `__MACOSX` metadata.
- Private credentials and local-only development assets must be absent.

For the publication-facing Litecoin Core comparison, see
`doc/defcoin-core-vs-litecoin-core.md`. For the current GitHub checklist, see
`doc/github-publication-checklist.md`.
