# Defcoin Core vs. Litecoin Core

Last updated: 2026-05-09

This document is the publication-facing summary of how the standard Defcoin
Core `2026.1` release differs from Litecoin Core `v0.21.5.5`.

It is intentionally limited to the standard Defcoin Core build. Experimental
visualization, alternate theme, and presentation-only work is outside this
comparison.

## Baseline

| Item | Value |
| --- | --- |
| Upstream project | `litecoin-project/litecoin` |
| Upstream release tag | `v0.21.5.5` |
| Upstream tag commit | `d8c8adc0b46cf96cc713455ba4aeb135efabac86` |
| Upstream tag verification | `git ls-remote --tags https://github.com/litecoin-project/litecoin.git 'refs/tags/v0.21.5*'` on 2026-05-09 showed `v0.21.5.5` tag object `2ce992fc1e0b054411043238204e9995f971966e` and peeled commit `d8c8adc0b46cf96cc713455ba4aeb135efabac86` |
| Defcoin release | `2026.1` |
| Defcoin codename | `Token Jester` |
| Defcoin source base | Litecoin Core `v0.21.5.5` with Defcoin chain, wallet, UI, seed, and packaging changes |

## High-Level Differences

| Area | Litecoin Core | Defcoin Core |
| --- | --- | --- |
| Product identity | Litecoin Core | Defcoin Core |
| GUI app | Litecoin-Qt | Defcoin Core / Defcoin-Qt |
| Daemon and CLI names | `litecoind`, `litecoin-cli`, `litecoin-tx`, `litecoin-wallet` | `defcoind`, `defcoin-cli`, `defcoin-tx`, `defcoin-wallet` |
| URI scheme | `litecoin:` | `defcoin:` |
| macOS data directory | `~/Library/Application Support/Litecoin/` | `~/Library/Application Support/Defcoin/` |
| Config file | `litecoin.conf` | `defcoin.conf` |
| Peer subversion | `/LitecoinCore:<version>/` | `/DefcoinCore:2026.1/` |
| Main display unit | LTC | DFC |
| Display subdivisions | Litecoin units | `Packet`, `Tock`, `Mote` |
| Mainnet default P2P port | `9333` | `1337` |
| Mainnet default RPC port | inherited Litecoin default | `9332` |
| Proof of work | Scrypt | Scrypt |
| Mainnet block target spacing | 150 seconds | 120 seconds |
| Mainnet difficulty target timespan | 3.5 days | 1 day |
| Mainnet DNS/fixed seeds | Litecoin infrastructure | Defcoin seed infrastructure |
| Mainnet address prefix | Litecoin prefixes | Defcoin prefixes |
| Mainnet SegWit/Taproot/MWEB | Litecoin deployment schedule | Not active for Defcoin mainnet in this release |

## Standard Build Scope

The standard Defcoin Core build keeps Litecoin Core's node, wallet, RPC, and Qt
architecture while changing only what is needed for Defcoin compatibility,
packaging, and basic Defcoin network operation.

## Porting Provenance

The 2026.1 Defcoin Core port was produced primarily with Codex running
GPT-5.5 at Extra High reasoning. The work used the current Litecoin Core
`v0.21.5.5` source, prior Defcoin source/build documentation, and local
build/test evidence to apply the Defcoin chain parameters, branding,
packaging, and UI changes while preserving the Litecoin-derived code structure
where possible.

Kept from Litecoin Core:

- block validation architecture;
- wallet architecture, including Berkeley DB wallet compatibility;
- mempool and transaction relay structure;
- RPC and REST architecture;
- Qt wallet page structure;
- pruning, rescan, reindex, peer, and proxy concepts;
- Scrypt proof-of-work implementation path.

Changed for Defcoin:

- chain parameters;
- genesis blocks and checkpoints;
- ports, seeds, address prefixes, and display units;
- release identity and peer user agent;
- standard GUI labels, Preferences wording, and Defcoin help text;
- minimal Node window controls useful for a small Defcoin network.

## Chain Parameter Differences

### Mainnet

| Parameter | Litecoin Core `v0.21.5.5` | Defcoin Core `2026.1` |
| --- | --- | --- |
| Network ID | `main` | `main` |
| Message start | `fb c0 b6 db` | `fb c0 b6 db` |
| Default P2P port | `9333` | `1337` |
| Default RPC port | Litecoin default RPC port | `9332` |
| Subsidy halving interval | `840000` | `840000` |
| Target spacing | `150` seconds | `120` seconds |
| Target timespan | `302400` seconds | `86400` seconds |
| Difficulty retarget window | `2016` Litecoin blocks | `720` Defcoin blocks |
| Versionbits miner confirmation window | `8064` | `8064` |
| Versionbits rule-change threshold | `6048` | `6048` |
| Proof-of-work limit | `00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff` | same |
| Minimum chain work | `00000000000000000000000000000000000000000000146878abee06fa883e0a` | `0000000000000000000000000000000000000000000000000001000000000000` |
| Assume-valid block | `80cdb35c080484df5bf384b311fde3c4694d3405765bc0f596e9eb369ff286e5` | unset / zero hash |
| Prune after height | `100000` | `100000` |
| Assumed blockchain size | `40` GB | `2` GB |
| Assumed chainstate size | `2` GB | `1` GB |
| Genesis timestamp | `1317972665` | `1394002925` |
| Genesis nonce | `2084524493` | `386295993` |
| Genesis bits | `0x1e0ffff0` | `0x1e0ffff0` |
| Genesis version | `1` | `1` |
| Genesis reward | `50 * COIN` | `50 * COIN` |
| Genesis hash | `12a765e31ffd4059bada1e25190f6e98c99d9714d334efa41a195a7e7e04bfe2` | `192047379f33ffd2bbbab3d53b9c4b9e9b72e48f888eadb3dcf57de95a6038ad` |
| Genesis merkle root | `97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9` | `7294da28c1b8eeba868388b14e2205874fb512f0ca31c2f583002557175f2c9c` |
| DNS seed hosts | Litecoin seed hosts | `seed.defcoin.io`, `seed.defcoin.mikej.tech`, `seed.defcoin.dc903.org:10332`, `seed.defcoincore.org` |
| Fixed seeds | Litecoin fixed seed table | Defcoin fixed seed table |
| Base58 pubkey prefix | `48` | `30` |
| Base58 script prefix | `5` | `5` |
| Base58 second script prefix | `50` | `50` |
| Base58 secret key prefix | `176` | `176` |
| Extended public key prefix | `0488b21e` | `0488b21e` |
| Extended secret key prefix | `0488ade4` | `0488ade4` |
| Bech32 HRP | `ltc` | `dfc` |
| MWEB HRP | `ltcmweb` | `dfcmweb` |
| ChainTxData timestamp | `1728673449` | `1551355823` |
| ChainTxData transaction count | `283070164` | `798434` |
| ChainTxData tx/sec estimate | `2.135848330748608` | `0.01` |

### Mainnet Feature Activation

| Feature | Litecoin Core `v0.21.5.5` | Defcoin Core `2026.1` |
| --- | --- | --- |
| BIP16 / P2SH | active at height `218579` | active from height `0` |
| BIP34 | height `710000` with Litecoin BIP34 hash | height `710000`; Defcoin BIP34 hash is unset / zero in this branch |
| BIP65 / CLTV | height `918684` | disabled by setting height to max int |
| BIP66 strict DER | height `811879` | disabled by setting height to max int |
| CSV | height `1201536` | disabled by setting height to max int |
| SegWit | height `1201536` | disabled by setting height to max int |
| Taproot | configured Litecoin deployment | deployment start and timeout set to max int |
| MWEB | configured Litecoin deployment | deployment start and timeout set to max int |
| Signet | inherited network in Litecoin family | not a supported Defcoin target in this release |

The Defcoin mainnet PoW retarget interval is one day, or 720 two-minute
blocks. The retained versionbits window is separate from PoW retargeting and is
kept at 8064 blocks with a 6048-block threshold, while mainnet versionbits
deployments used by newer Litecoin features are set to never activate.

### Testnet

| Parameter | Litecoin Core `v0.21.5.5` | Defcoin Core `2026.1` |
| --- | --- | --- |
| Default P2P port | `19335` | `31337` |
| Message start | `fd d2 c8 f1` | `fc c1 b7 dc` |
| Target spacing | `150` seconds | `120` seconds |
| Target timespan | `302400` seconds | `86400` seconds |
| Miner confirmation window | `2016` | `720` |
| Rule-change activation threshold | `1512` | `540` |
| Genesis timestamp | `1486949366` | `1394003000` |
| Genesis nonce | `293345` | `707631` |
| Genesis hash | `4966625a4b2851d9fdee139e56211a0d88575f59ed816ff5e6a63deb4e3e29a0` | `3b714281e102f963bc15948692d18f5e2792bfe5e81d333d25d7f9c563e07f7f` |
| Genesis merkle root | `97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9` | `7294da28c1b8eeba868388b14e2205874fb512f0ca31c2f583002557175f2c9c` |
| Testnet DNS seeds | Litecoin testnet seeds | none configured |
| Fixed seeds | Litecoin testnet fixed seed table | cleared |
| Bech32 HRP | `tltc` | `tdfc` |
| MWEB HRP | `tmweb` | `tdfcmweb` |
| Checkpoints | Litecoin testnet checkpoints | only Defcoin testnet genesis checkpoint |

### Regtest

| Parameter | Litecoin Core `v0.21.5.5` | Defcoin Core `2026.1` |
| --- | --- | --- |
| Default P2P port | `19444` | `19444` |
| Message start | `fa bf b5 da` | `fa bf b5 da` |
| Target spacing | `150` seconds | `120` seconds |
| Target timespan | `302400` seconds | `86400` seconds |
| Genesis timestamp | `1296688602` | `1296688602` |
| Genesis nonce | `0` | `0` |
| Genesis hash | `530827f38f93b43ed12af0b3ad25a288dc02ed74d6d7857862df51fc56c416f9` | `e7427996266509d152c77ff38e31191bcbaae06f8cac6ba7181d5d8e7cbbcfc9` |
| Genesis merkle root | `97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9` | `7294da28c1b8eeba868388b14e2205874fb512f0ca31c2f583002557175f2c9c` |
| Bech32 HRP | `rltc` | `rdfc` |
| MWEB HRP | `tmweb` | `rdfcmweb` |

## Checkpoints

Mainnet Defcoin Core keeps the inherited checkpoint heights but replaces the
checkpoint hashes with Defcoin chain hashes at:

```text
1500, 4032, 8064, 16128, 23420, 50000, 80000, 120000,
161500, 179620, 240000, 383640, 409004, 456000, 638902,
721000
```

The full checkpoint hash table is maintained in
`doc/defcoin-build-deviation-matrix.md` and `src/chainparams.cpp`.

## Peer Discovery And User Agents

Defcoin Core advertises `/DefcoinCore:2026.1/`.

The standard GUI exposes a user-agent filter so operators can reject peers that
do not advertise a Defcoin-prefixed user agent. This is a small-network hygiene
control, not a consensus rule. It must not be used to block valid peers before
they have exchanged enough address information for normal peer discovery.

The current accepted Defcoin-prefixed family is `/Defcoin...`, including the
standard `/DefcoinCore:2026.1/` subversion.

## Disabled Inherited Litecoin Features

The standard Preferences dialog marks these inherited Litecoin Core features as
currently unavailable for Defcoin Core:

- BIP65 / CLTV;
- BIP66 strict DER signatures;
- CSV;
- SegWit activation;
- Taproot;
- MWEB;
- Signet.

The disabled controls are reminders for maintainers and users. They are not GUI
switches that can activate those rules.

## Build And Packaging Differences

Defcoin Core adds:

- Defcoin binary names;
- Defcoin macOS bundle identifiers and metadata;
- Defcoin app icon and Qt resources;
- Apple Silicon DMG packaging support;
- modern macOS build notes for Qt 5, Berkeley DB 4.8, Boost compatibility, and
  Homebrew library paths;
- Windows ZIP hygiene rules to avoid `.DS_Store` and `__MACOSX` metadata.

The current GitHub publication checklist is maintained in
`doc/github-publication-checklist.md`.
