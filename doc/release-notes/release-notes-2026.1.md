# Defcoin Core 2026.1 Release Notes

Codename: Token Jester

Draft date: 2026-05-04

These are draft release notes for the standard Defcoin Core `2026.1` build.
They should be reviewed before public GitHub release publication.

## Overview

Defcoin Core 2026.1 is a Defcoin network port of Litecoin Core `v0.21.5.5`.
It provides native Defcoin node and wallet binaries, updated chain parameters,
Defcoin seed hosts, Defcoin display units, and modern macOS packaging support.

The standard build is intentionally conservative. It keeps Litecoin Core's
tested wallet/node architecture while adapting it for the historical Defcoin
network.

## Notable Changes

- Product identity changed to Defcoin Core.
- User-facing release identity changed to `2026.1`, codename `Token Jester`.
- Peer user agent changed to `/DefcoinCore:2026.1/`.
- Mainnet Defcoin P2P/RPC defaults are `1337` / `9332`.
- Mainnet block target spacing is 120 seconds.
- Mainnet retarget window is 720 blocks.
- Display units are `DFC`, `Packet`, `Tock`, and `Mote`.
- Mainnet seed hosts are:
  - `seed.defcoin.io`
  - `seed.defcoin.mikej.tech`
  - `seed.defcoin.dc903.org:10332`
- Berkeley DB wallet support remains enabled for legacy `wallet.dat`
  compatibility.
- SegWit, Taproot, MWEB, Signet, and related inherited Litecoin features are
  clearly marked as unavailable for Defcoin Core in this release.
- The Preferences menu includes text-size controls, and linked text resizing is
  enabled by default.
- The Node window includes basic Defcoin peer controls for inactive peers,
  banned peers, local network discovery, and optional Defcoin-prefixed user
  agent filtering.

## Build And Packaging

Apple Silicon standard distribution artifacts are staged locally as:

```text
Defcoin-Core-v2026.1-macOS-AppleSilicon.dmg
```

The Apple Silicon standard DMG SHA-256 at the time of this draft is:

```text
8dd7516bb37a53c008c54fd91cda72a7ef456cf9c75edbc512a7ee95a4bff204
```

The package is ad-hoc signed for local developer distribution. Public macOS
release distribution still requires Developer ID signing and notarization.

## Compatibility Notes

- Back up old Defcoin wallets before opening them with this release.
- Test old `wallet.dat` files only from copied wallets or copied data
  directories until compatibility is fully validated.
- Pruned nodes may need a reindex or rescan depending on wallet history.
- This release does not activate newer Litecoin mainnet consensus features for
  Defcoin mainnet.

## Publication Documents

For technical differences from Litecoin Core, see:

- `doc/defcoin-core-vs-litecoin-core.md`
- `doc/defcoin-build-deviation-matrix.md`
- `doc/litecoin-upstream-comparison-standard.md`

For publication readiness, see:

- `doc/github-publication-checklist.md`
