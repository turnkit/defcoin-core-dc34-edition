# Defcoin Core 0.21.5.5 Port Notes

Last updated: 2026-05-07

This document summarizes the upstream Litecoin Core `v0.21.5.4` to
`v0.21.5.5` changes that were ported into Defcoin Core. It is intended for the
public repository and release-review notes.

## Upstream Reference

| Item | Value |
| --- | --- |
| Upstream repository | `litecoin-project/litecoin` |
| Previous base | `v0.21.5.4`, commit `940c89ad7190d49383f70f17f0b8c8e82f310b50` |
| New base | `v0.21.5.5`, commit `d8c8adc0b46cf96cc713455ba4aeb135efabac86` |
| Tag date | 2026-05-06 |
| Verification command | `git fetch --tags origin` |

## Source Changes Ported

The Defcoin port keeps the Defcoin chain parameters, seeds, user agent,
packaging, and Qt work intact while importing Litecoin Core's `0.21.5.5`
hardening changes.

The port includes:

- MWEB hash-to-secret-key fallback hardening.
- Functional-test replacement for the external `litecoin_scrypt` Python module.
- PMMR cache leaf bounds checking.
- HogEx fee and amount mismatch classification as consensus errors.
- Pre-MWEB activation HogEx marker rejection.
- MWEB chainstate update during rollforward.
- Duplicate pegin guards.
- MWEB mempool assertions.
- Miner/MWEB mining fixes and better getblocktemplate fee/sigop accounting.
- RPC `maxfeerate=0` handling for MWEB transactions in `sendrawtransaction` and
  `testmempoolaccept`.
- Expanded MWEB P2P functional tests.
- Safer MWEB logger initialization.
- Kernel lock-height enforcement.
- Invalid-block state-mutation hardening.
- Amount overflow hardening.
- `ParseHex` use in chain parameter helper code.
- Larger maximum P2P protocol message length: 32 MB instead of 4 MB.
- Pegout script standardness enforcement.

## Defcoin-Specific Notes

Defcoin mainnet still leaves inherited Litecoin Core features such as MWEB,
Taproot, SegWit, CSV, BIP65, and BIP66 inactive unless a future Defcoin network
release deliberately activates them.

That does not make the imported code irrelevant. These changes still matter for:

- regtest and functional-test coverage;
- dormant code paths that remain compiled;
- future feature activation work;
- keeping Defcoin's fork close enough to Litecoin Core that future upstream
  security and consensus fixes can be reviewed and ported cleanly.

## Port Verification

The staged source port passes:

```sh
git diff --cached --check
git diff --check
```

A targeted Apple Silicon daemon compile was also completed with an explicit
native configure and local Homebrew dependencies:

```sh
./configure --with-gui=no --disable-wallet --disable-tests --disable-bench \
  --disable-zmq --without-miniupnpc
make -C src -j4 defcoind
./src/defcoind -version
```

The successful version check reported the Defcoin `2026.1` product version from
the rebased source tree. That product identity is deliberate; the Litecoin Core
baseline is documented here as `v0.21.5.5`.
