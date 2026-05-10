# Defcoin Core Deviation Matrix

Last updated: 2026-05-07

This file tracks the intentional differences between Defcoin Core and the current Litecoin Core release line. It is intended as a merge checklist and a release-review artifact for the Defcoin Core 2026.1 "Token Jester" release.

For the shorter publication-facing comparison, see
`doc/defcoin-core-vs-litecoin-core.md`.

## Baselines Checked

| Baseline | Version / tag | Evidence |
| --- | --- | --- |
| Current Litecoin Core release | `v0.21.5.5` | Local upstream tag list fetched from `litecoin-project/litecoin`; GitHub tag `v0.21.5.5` checked on 2026-05-07. |
| Defcoin Core release target | `2026.1` / `Token Jester` | This repository. |
| Apple Silicon compile check for upstream Litecoin Core | `v0.21.5.5` | Same upstream source tag; build notes are listed below. |

## Defcoin Core vs. Current Litecoin Core

| Area | Files / modules | Defcoin Core difference |
| --- | --- | --- |
| Product identity | Qt app metadata, splash, About, package scripts | Product name is Defcoin Core. Version identity is Defcoin `2026.1` / codename `Token Jester`. Standard package filenames use Defcoin naming and must not include alternate build wording or extra themed graphics. |
| Peer subversion / user agent | `src/clientversion.cpp`, `src/clientversion.h`, `src/init.cpp` | The P2P subversion advertises `/DefcoinCore:2026.1/` instead of the inherited numeric Litecoin client version. The internal inherited `CLIENT_VERSION` remains available for compatibility where the upstream numeric version is expected. |
| Copyright and credits | About dialog, splash, package metadata | Defcoin Core copyright is `Copyright (C) 2014-2026 The Defcoin Core developers`. Litecoin Core and Bitcoin Core credits remain where user-facing upstream credits are shown. macOS bundle copyright uses the Defcoin Core copyright string only. |
| Chain identity | `src/chainparams.cpp`, `src/chainparamsseeds.h` | Uses Defcoin genesis blocks, checkpoints, activation heights, default ports, seed hosts, address prefixes, and chain-work assumptions instead of Litecoin chain values. Detailed differences are listed below. |
| DNS and fixed seeds | `src/chainparams.cpp`, `src/chainparamsseeds.h` | Mainnet DNS seeds are `seed.defcoin.io`, `seed.defcoin.mikej.tech`, `seed.defcoin.dc903.org:10332`, and `seed.defcoincore.org`. Litecoin DNS seeds are removed. Defcoin fixed seeds replace Litecoin fixed seeds. |
| Amount units | `src/qt/bitcoinunits.*` and amount display callers | User-facing units are `DFC`, `Packet`, `Tock`, and `Mote`. Base arithmetic remains satoshi-style integer arithmetic. |
| Example receive address | Qt receive/payment text | Example address text uses a valid Defcoin address format, not a Litecoin example. |
| Main wallet toolbar | Qt main window | Defcoin Core keeps the wallet tabs and adds direct access to the Node window and Preferences. |
| Preferences naming | Qt menus/dialogs | User-facing settings language is normalized around Preferences where appropriate for desktop UI consistency. |
| Appearance | Qt options/theme helpers | Standard Defcoin Core exposes Light, Dark, and Auto/system appearance choices only. |
| Node window peer controls | `src/qt/rpcconsole.*`, peer table model/view helpers | Adds user-visible controls for showing inactive peers, showing banned peers, discovering local network nodes, and optionally rejecting peers whose user agent is not Defcoin-prefixed. |
| Local peer discovery | Node/peer UI and network helper code | Adds optional local-subnet discovery for nearby Defcoin nodes. On macOS this can trigger the normal Local Network privacy prompt. |
| Peer list presentation | Qt node window | Adds clearer peer table columns, banned peer display, seed labeling, local/LAN labeling, and text-size controls. These are UI and diagnostics changes; they do not change consensus. |
| Network traffic chart | Qt node window | Standard Defcoin Core keeps the chart focused on received and sent traffic and removes visualization-only metrics from the standard package. |
| Disabled inherited Litecoin features | Preferences and help text | Defcoin Core clearly marks inherited Litecoin Core features that are currently inactive or disabled for Defcoin, including MWEB and Signet where those controls or references could otherwise confuse users. |
| Build and packaging | `doc/build-osx.md`, `contrib/defcoin-macos-package-dmg.sh`, distribution notes | Adds Apple Silicon build/package instructions and a Defcoin DMG packaging path. |

## Detailed Chain Parameter Differences

### Mainnet

| Parameter | Litecoin Core `v0.21.5.5` | Defcoin Core |
| --- | --- | --- |
| Chain name | `main` | `main` |
| Default port | `9333` | `1337` |
| Message start bytes | `fb c0 b6 db` | `fb c0 b6 db` |
| Subsidy halving interval | `840000` | `840000` |
| BIP16 height | `218579` | `0` |
| BIP34 height | `710000` | `710000` |
| BIP34 hash | `fa09d204a83a768ed5a7c8d441fa62f2043abf420cff1226c7b4329aeb9d51cf` | unset / zero hash |
| BIP65 height | `918684` | disabled by setting to `std::numeric_limits<int>::max()` |
| BIP66 height | `811879` | disabled by setting to `std::numeric_limits<int>::max()` |
| CSV height | `1201536` | disabled by setting to `std::numeric_limits<int>::max()` |
| Segwit height | `1201536` | disabled by setting to `std::numeric_limits<int>::max()` |
| Minimum BIP9 warning height | `1209600` | disabled by setting to `std::numeric_limits<int>::max()` |
| Proof-of-work limit | `00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff` | same value |
| Target timespan | `3.5 * 24 * 60 * 60` seconds, or 3.5 days | `1 * 24 * 60 * 60` seconds, or 1 day |
| Target spacing | `2.5 * 60` seconds, or 150 seconds | `2 * 60` seconds, or 120 seconds |
| Allow minimum difficulty blocks | `false` | `false` |
| No retargeting | `false` | `false` |
| Rule-change activation threshold | `6048` | `6048` |
| Miner confirmation window | `8064` | `8064` |
| Testdummy deployment | bit `28`, never active, no timeout | bit `28`, never active, no timeout |
| Taproot deployment | bit `2`, start height `2161152`, timeout height `2370816` | bit `2`, never active, start/timeout heights set to max int |
| MWEB deployment | bit `4`, start height `2217600`, timeout height `2427264` | bit `4`, never active, start/timeout heights set to max int |
| MWEB metadata | has grandfather block hash and frozen output IDs | inactive; default frozen-output list helper remains compiled but chain activation is disabled |
| Minimum chain work | `00000000000000000000000000000000000000000000146878abee06fa883e0a` | `0000000000000000000000000000000000000000000000000001000000000000` |
| Default assume-valid block | `80cdb35c080484df5bf384b311fde3c4694d3405765bc0f596e9eb369ff286e5` | unset / zero hash |
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
| DNS seeds | `seed-a.litecoin.loshan.co.uk`, `dnsseed.thrasher.io`, `dnsseed.litecointools.com`, `dnsseed.litecoinpool.org`, `dnsseed.koin-project.com` | `seed.defcoin.io`, `seed.defcoin.mikej.tech`, `seed.defcoin.dc903.org:10332`, `seed.defcoincore.org` |
| Base58 public-key address prefix | `48` | `30` |
| Base58 script address prefix | `5` | `5` |
| Base58 second script address prefix | `50` | `50` |
| Base58 secret-key prefix | `176` | `176` |
| Extended public key prefix | `0488b21e` | `0488b21e` |
| Extended secret key prefix | `0488ade4` | `0488ade4` |
| Bech32 HRP | `ltc` | `dfc` |
| MWEB HRP | `ltcmweb` | `dfcmweb` |
| Fixed seeds | Litecoin fixed seed table | Defcoin fixed seed table |
| Default consistency checks | `false` | `false` |
| Require standard transactions | `true` | `true` |
| ChainTxData timestamp | `1728673449` | `1551355823` |
| ChainTxData transaction count | `283070164` | `798434` |
| ChainTxData tx/sec estimate | `2.135848330748608` | `0.01` |

Mainnet checkpoints differ at every populated checkpoint hash. Defcoin Core keeps the same checkpoint heights as the inherited Litecoin table, but replaces the block hashes with Defcoin chain hashes:

| Height | Litecoin checkpoint hash | Defcoin checkpoint hash |
| --- | --- | --- |
| `1500` | `841a2965955dd288cfa707a755d05a54e45f8bd476835ec9af4402a2b59a2967` | `9ba753f88b2ddb87ccb947db90607190e1319f1574355f76d06adb911b7f7ecc` |
| `4032` | `9ce90e427198fc0ef05e5905ce3503725b80e26afd35a987965fd7e3d9cf0846` | `c047770e10be2c180bf64092c73c87507cca7d5586bb8db875f4cb2689dae085` |
| `8064` | `eb984353fc5190f210651f150c40b8a4bab9eeeff0b729fcb3987da694430d70` | `bd5a8919d814acb5d2019a7be7597dec7e046f05ca5fe3709fff2e573cd0b06a` |
| `16128` | `602edf1859b7f9a6af809f1d9b0e6cb66fdc1d4d9dcd7a4bec03e12a1ccd153d` | `6cada493d40338dfe5db7adf674189796efb697e35546705270be4ecbe01927a` |
| `23420` | `d80fdf9ca81afd0bd2b2a90ac3a9fe547da58f2530ec874e978fce0b5101b507` | `0ee4642fd165d5b69b6ae28b088a5ef458426577387c9c155c3c881e2a642692` |
| `50000` | `69dc37eb029b68f075a5012dcc0419c127672adb4f3a32882b2b3e71d07a20a6` | `c13e25648abb47ecc3e73a9ab42d3c9a7f986c5f4f04f1fa48035157e182d464` |
| `80000` | `4fcb7c02f676a300503f49c764a89955a8f920b46a8cbecb4867182ecdb2e90a` | `f5f2ccad4487e9c0152f788ede5a0e0a2e12482ee962b7b1566328f6347d83a0` |
| `120000` | `bd9d26924f05f6daa7f0155f32828ec89e8e29cee9e7121b026a7a3552ac6131` | `dac4b8548f8f96cc8ce55bf63459f2213ddc964d697f776598d26760b87914d8` |
| `161500` | `dbe89880474f4bb4f75c227c77ba1cdc024991123b28b8418dbbf7798471ff43` | `2e846042b7a2dc1c67a3953fb4347c2c8bb5b25e48c18741362621b02f0dcfc6` |
| `179620` | `2ad9c65c990ac00426d18e446e0fd7be2ffa69e9a7dcb28358a50b2b78b9f709` | `1b81d68160fdd4050828d61735e1573881345514548c20cf65376d37b2c5a0fa` |
| `240000` | `7140d1c4b4c2157ca217ee7636f24c9c73db39c4590c4e6eab2e3ea1555088aa` | `0f16c1a0d4b376d141bd5a87fa3df782f469e69a435e6d17cd05c6275797f8a3` |
| `383640` | `2b6809f094a9215bafc65eb3f110a35127a34be94b7d0590a096c3f126c6f364` | `5f5b591b20fa8701f987457bec0ae0e98125b1d8b73e624cbba371db7d10f3f3` |
| `409004` | `487518d663d9f1fa08611d9395ad74d982b667fbdc0e77e9cf39b4f1355908a3` | `4dd357e42bab4a6ffbb763f91fb5885fd85ba1187a1385bcf030030df05735d6` |
| `456000` | `bf34f71cc6366cd487930d06be22f897e34ca6a40501ac7d401be32456372004` | `d31bd54f4c856cccb1edb8588ad6748b02e7de3a7bdc0340a2ab2ab31100e011` |
| `638902` | `15238656e8ec63d28de29a8c75fcf3a5819afc953dcd9cc45cecc53baec74f38` | `dc0a1fbe223f4bd64c51cef0dcd10dabe11e4a7370b433e7942f7ad2fcb94447` |
| `721000` | `198a7b4de1df9478e2463bd99d75b714eab235a2e63e741641dc8a759a9840e5` | `9623f4c4e3277eeecf7575d2dff3348b453395a489ce5888dcfe939a92bcf04d` |

### Testnet

| Parameter | Litecoin Core `v0.21.5.5` | Defcoin Core |
| --- | --- | --- |
| Default port | `19335` | `31337` |
| Message start bytes | `fd d2 c8 f1` | `fc c1 b7 dc` |
| Subsidy halving interval | `840000` | `840000` |
| BIP16 height | `0` | `0` |
| BIP34 height/hash | height `76`, hash `8075c771ed8b495ffd943980a95f702ab34fce3c8c54e379548bda33cc8c0573` | same height and hash |
| BIP65 height | `76` | `76` |
| BIP66 height | `76` | `76` |
| CSV height | `6048` | `6048` |
| Segwit height | `6048` | `6048` |
| Target timespan | 3.5 days | 1 day |
| Target spacing | 150 seconds | 120 seconds |
| Allow minimum difficulty blocks | `true` | `true` |
| Miner confirmation window | `2016` | `720` |
| Rule-change activation threshold | `1512` | `540` |
| Taproot deployment | bit `2`, start height `2225664`, timeout height `2435328` | same bit and heights |
| MWEB deployment | bit `4`, start height `2209536`, timeout height `2419200` | same bit and heights |
| Minimum chain work | `000000000000000000000000000000000000000000000000004260a1758f04aa` | same value |
| Default assume-valid block | `4a280c0e150e3b74ebe19618e6394548c8a39d5549fd9941b9c431c73822fbd5` | same value |
| Prune after height | `1000` | `1000` |
| Genesis timestamp | `1486949366` | `1394003000` |
| Genesis nonce | `293345` | `707631` |
| Genesis bits/version/reward | `0x1e0ffff0`, version `1`, `50 * COIN` | same bits/version/reward |
| Genesis hash | `4966625a4b2851d9fdee139e56211a0d88575f59ed816ff5e6a63deb4e3e29a0` | `3b714281e102f963bc15948692d18f5e2792bfe5e81d333d25d7f9c563e07f7f` |
| Genesis merkle root | `97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9` | `7294da28c1b8eeba868388b14e2205874fb512f0ca31c2f583002557175f2c9c` |
| DNS seeds | `testnet-seed.litecointools.com`, `seed-b.litecoin.loshan.co.uk`, `dnsseed-testnet.thrasher.io` | none currently configured |
| Fixed seeds | Litecoin testnet fixed seed table | none; Defcoin testnet clears fixed seeds |
| Base58 public-key address prefix | `111` | `111` |
| Base58 script address prefix | `196` | `196` |
| Base58 second script address prefix | `58` | `58` |
| Base58 secret-key prefix | `239` | `239` |
| Extended public key prefix | `043587cf` | `043587cf` |
| Extended secret key prefix | `04358394` | `04358394` |
| Bech32 HRP | `tltc` | `tdfc` |
| MWEB HRP | `tmweb` | `tdfcmweb` |
| Default consistency checks | `false` | `false` |
| Require standard transactions | `false` | `false` |
| Test chain / mockable chain flags | test chain `true`, mockable `false` | test chain `true`, mockable `false` |
| Checkpoints | heights `300`, `2056`, `2352616` with Litecoin testnet hashes | only height `0`, hash `3b714281e102f963bc15948692d18f5e2792bfe5e81d333d25d7f9c563e07f7f` |
| ChainTxData timestamp | `1607986972` | `1607986972` |
| ChainTxData transaction count | `4229067` | `4229067` |
| ChainTxData tx/sec estimate | `0.06527021772939347` | `0.06527021772939347` |

### Regtest

| Parameter | Litecoin Core `v0.21.5.5` | Defcoin Core |
| --- | --- | --- |
| Default port | `19444` | `19444` |
| Message start bytes | `fa bf b5 da` | `fa bf b5 da` |
| Subsidy halving interval | `150` | `150` |
| BIP16/BIP34/BIP65/BIP66/CSV/Segwit heights | `0`, `500`, `1351`, `1251`, `432`, `0` | same heights |
| Proof-of-work limit | `7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff` | same value |
| Target timespan | 3.5 days | 1 day |
| Target spacing | 150 seconds | 120 seconds |
| Allow minimum difficulty blocks | `true` | `true` |
| No retargeting | `true` | `true` |
| Rule-change activation threshold | `108` | `108` |
| Miner confirmation window | `144` | `144` |
| Testdummy deployment | bit `28`, start time `0`, no timeout | same |
| Taproot deployment | bit `2`, always active, no timeout | same |
| MWEB deployment | bit `4`, start time `1601450001`, no timeout | same |
| Minimum chain work | zero | zero |
| Default assume-valid block | zero | zero |
| Prune after height | `1000` | `1000` |
| Genesis timestamp | `1296688602` | `1296688602` |
| Genesis nonce | `0` | `0` |
| Genesis hash | `530827f38f93b43ed12af0b3ad25a288dc02ed74d6d7857862df51fc56c416f9` | `e7427996266509d152c77ff38e31191bcbaae06f8cac6ba7181d5d8e7cbbcfc9` |
| Genesis merkle root | `97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9` | `7294da28c1b8eeba868388b14e2205874fb512f0ca31c2f583002557175f2c9c` |
| Fixed seeds | none | none |
| DNS seeds | none | none |
| Default consistency checks | `true` | `true` |
| Require standard transactions | `true` | `true` |
| Test chain / mockable chain flags | test chain `true`, mockable `true` | test chain `true`, mockable `true` |
| Checkpoint at height 0 | `530827f38f93b43ed12af0b3ad25a288dc02ed74d6d7857862df51fc56c416f9` | `e7427996266509d152c77ff38e31191bcbaae06f8cac6ba7181d5d8e7cbbcfc9` |
| Base58 public-key address prefix | `111` | `111` |
| Base58 script address prefix | `196` | `196` |
| Base58 second script address prefix | `58` | `58` |
| Base58 secret-key prefix | `239` | `239` |
| Extended public key prefix | `043587cf` | `043587cf` |
| Extended secret key prefix | `04358394` | `04358394` |
| Bech32 HRP | `rltc` | `rdfc` |
| MWEB HRP | `tmweb` | `rdfcmweb` |
| ChainTxData timestamp | `0` | `0` |
| ChainTxData transaction count | `0` | `0` |
| ChainTxData tx/sec estimate | `0` | `0` |

## Current Litecoin Core vs. Apple Silicon Buildable Litecoin Core

The source release is the same: the current upstream Litecoin Core release checked for this matrix is `v0.21.5.5`, and the Apple Silicon build test uses the same `v0.21.5.5` tag.

The differences discovered during the Apple Silicon compile check are build-environment differences, not source-version differences:

Note: this section documents the upstream Litecoin reference build check. The
current Defcoin Core 2026.1 Apple Silicon build uses Homebrew `boost@1.85`
after Defcoin-side Boost filesystem compatibility fixes.

| Item | Current upstream default expectation | Apple Silicon build result in this workspace |
| --- | --- | --- |
| Source version | `v0.21.5.5` | `v0.21.5.5` |
| Architecture | upstream source is portable | native `arm64-apple-darwin25.4.0` compile attempt |
| Qt | Qt 5 required for GUI | Qt `5.15.18` from Homebrew was detected and used |
| Boost | Boost >= 1.58 accepted | local pinned Boost `1.84.0` must be preferred before Homebrew Boost in linker search paths |
| miniUPnPc | configure can enable UPnP when a compatible miniUPnPc is installed | Homebrew miniUPnPc headers expose a newer `UPNP_GetValidIGD` signature than this Litecoin source calls, so upstream Litecoin `v0.21.5.5` does not compile against that Homebrew miniUPnPc without either disabling UPnP or patching the call site |
| Build configuration verified on this Mac | not applicable | `--without-miniupnpc`, local Boost path first in `LDFLAGS`, Qt 5, tests/bench disabled |
| Source patch required for the checked compile path | none for the documented configuration | none; the compatibility change is configure/linker-path selection |
| Smoke test | not applicable | `litecoind -regtest` started, `getblockchaininfo` returned the Litecoin regtest genesis hash and normal chain state, then `stop` completed |

Recommended Apple Silicon configure baseline for checking upstream Litecoin Core:

```sh
./configure \
  --with-gui=qt5 \
  --disable-tests \
  --disable-bench \
  --disable-ccache \
  --without-miniupnpc \
  --with-boost="$HOME/src/local-deps/boost-1.84.0-arm64" \
  --with-incompatible-bdb \
  CPPFLAGS="-include $HOME/src/local-deps/apple-sdk-le32-shim.h -I/opt/homebrew/include -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -DMAC_OSX -DOBJC_OLD_DISPATCH_PROTOTYPES=0" \
  LDFLAGS="-L$HOME/src/local-deps/boost-1.84.0-arm64/lib -L/opt/homebrew/lib" \
  PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig:/opt/homebrew/lib/pkgconfig"
```

Notes:

- The first compile attempt with UPnP enabled failed at `net.cpp` because the installed miniUPnPc expects the newer 7-argument `UPNP_GetValidIGD` call while upstream Litecoin `v0.21.5.5` calls the older 5-argument form.
- The next compile attempt failed at link time until the local Boost library path was moved before `/opt/homebrew/lib`; otherwise the linker selected Homebrew Boost libraries while objects had been built against the pinned local Boost headers.
- If UPnP support is required for an upstream-reference build on this Mac, use a miniUPnPc version with the older compatible API or carry a small source compatibility patch instead of `--without-miniupnpc`.
