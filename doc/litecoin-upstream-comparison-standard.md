# Defcoin Core Standard Build: Litecoin Upstream Comparison

Last updated: 2026-05-09

## Upstream Baseline

The latest upstream Litecoin Core release checked for this comparison is:

- Repository: `litecoin-project/litecoin`
- Tag: `v0.21.5.5`
- Commit: `d8c8adc0b46cf96cc713455ba4aeb135efabac86`
- Tag date in the fetched upstream tag object: 2026-05-06
- Tag verification: `git ls-remote --tags
  https://github.com/litecoin-project/litecoin.git 'refs/tags/v0.21.5*'` on
  2026-05-09 returned the `v0.21.5.5` tag object and peeled commit above.

The prior comparison target was `v0.21.5.4`
(`940c89ad7190d49383f70f17f0b8c8e82f310b50`, tag date 2026-04-25).

## 0.21.5.4 To 0.21.5.5 Port Result

Defcoin Core has been advanced from the local Defcoin `0.21.5.4` base to the
Litecoin Core `0.21.5.5` source line.

The early `v0.21.5.5` commits for MWEB documentation, Linux desktop metadata,
security-contact text, and `v0.21.5.4` release notes were already represented in
the local tree as equivalent files. The source-bearing 0.21.5.5 commits were
then applied on top of Defcoin without changing the Defcoin UI feature split.

The imported upstream changes include:

- Hash-to-secret-key fallback hardening.
- The functional-test replacement for the external `litecoin_scrypt` Python
  dependency.
- PMMR cache bounds checking.
- MWEB/HogEx validation, mempool, rollforward, duplicate-pegin, miner, and RPC
  hardening.
- Safer MWEB logger initialization.
- Kernel lock-height enforcement.
- Invalid-block state-mutation hardening.
- Amount calculation hardening.
- `ParseHex` use in chainparams.
- Increased maximum P2P protocol message length, from 4 MB to 32 MB.
- Pegout script standardness enforcement.
- New and expanded functional tests for the above behavior.

Defcoin mainnet still keeps MWEB, Taproot, SegWit, CSV, BIP65, and BIP66
inactive unless a future Defcoin network release deliberately activates them.
The inherited MWEB fixes remain useful because regtest, tests, dormant code
paths, and future activation work all share that source.

## Litecoin Features Intentionally Disabled For Defcoin Mainnet

Defcoin mainnet intentionally does not activate or expose these inherited
Litecoin Core features as live Defcoin mainnet behavior in this build:

- BIP65/CLTV
- BIP66 strict DER signatures
- CSV
- SegWit activation
- Taproot
- MWEB
- Signet

The Options dialog now shows these as checked, disabled reminder boxes labeled
`Disable <feature> feature`. These controls are informational only and cannot be
changed from the GUI.

## GitHub Fork Preparation

To preserve the expected fork relationship, the clean public repository should
remain a GitHub fork of `litecoin-project/litecoin`, then the sanitized Defcoin
Core standard build should be pushed into that fork. The public history should
contain only build-essential source, documentation, and packaging assets.

`turnkit/defcoin-core` was checked on 2026-05-04 and is configured locally as
the Defcoin remote. Its upload/push step remains intentionally gated on
explicit approval.

Recommended order:

1. Confirm the GitHub fork relationship still points to
   `litecoin-project/litecoin`.
2. Create a Defcoin branch based on upstream `v0.21.5.5`.
3. Apply only the Defcoin Core standard build changes.
4. Verify no local-only assets, private credentials, or DC34 Edition-only assets are
   included.
5. Build and smoke-test the standard app.
6. Push only after explicit approval.

No GitHub push was performed as part of this comparison.

For the publication-facing difference summary, see
`doc/defcoin-core-vs-litecoin-core.md`. For the pre-push checklist, see
`doc/github-publication-checklist.md`.
