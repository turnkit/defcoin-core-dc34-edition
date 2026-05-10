# GitHub Publication Checklist

Last updated: 2026-05-09

This checklist prepares the standard Defcoin Core repository for GitHub
publication.

## Repository Shape

Target public repository:

```text
turnkit/defcoin-core
```

Intended fork relationship:

```text
litecoin-project/litecoin -> turnkit/defcoin-core
```

Preferred public history:

1. Start from upstream Litecoin Core tag `v0.21.5.5`.
2. Apply standard Defcoin Core changes only.
3. Keep release artifacts separate from source unless uploading them to a
   GitHub Release.

## Must Include

- Defcoin Core source changes required for chain, wallet, network, seed, and
  packaging compatibility.
- Runtime Qt/macOS resources required for a clean clone and successful build.
- Standard documentation:
  - `README.md`
  - `CHANGELOG.md`
  - `doc/defcoin-core-vs-litecoin-core.md`
  - `doc/defcoin-build-deviation-matrix.md`
  - `doc/defcoin-litecoin-conversion-notes.md`
  - `doc/defcoin-modern-build-guide.md`
  - `doc/litecoin-upstream-comparison-standard.md`
  - `doc/defcoin-porting-notes.md`
  - `doc/defcoin-0.21.5.5-port-notes.md`
  - `doc/github-publication-checklist.md`
- Required package/build helper scripts.
- Required third-party notices for the standard build.

## Must Not Include

- Private credentials, SSH keys, wallet files, API tokens, `.env` files, or
  local RPC cookies.
- Personal source artwork and scratch graphics that are not required by the
  runtime build.
- Local-only design experiments, rejected mockups, or screenshot QA folders.
- Generated build folders such as `depends/work`, `depends/built`, `.tmp-build`,
  `dist`, or ad-hoc app bundles.
- Binary release artifacts committed into source history unless intentionally
  managed through Git LFS or a release artifact policy.
- `.DS_Store`, `__MACOSX`, or other host metadata.
- Experimental presentation features in the standard branch.

## Release Artifact Policy

Distribution artifacts should be staged outside the source repository and then
uploaded to a GitHub Release. Do not commit DMGs, ZIPs, app bundles, or build
cache folders to source history.

Expected Apple Silicon standard release asset:

```text
Defcoin-Core-v2026.1-macOS-AppleSilicon.dmg
```

## Required Pre-Push Checks

Run these before pushing the standard repository:

```sh
git status --short
git diff --check
git ls-files --cached --others --exclude-standard | rg '(^|/)(\\.DS_Store|__MACOSX)(/|$)' || true
rg -n "BEGIN (RSA|OPENSSH|PRIVATE) KEY|PRIVATE KEY|GITHUB_TOKEN|ghp_|sk-[A-Za-z0-9]|private_credentials" .
```

Review expected fixture hits from inherited tests and examples. Do not treat
inherited test passwords or sample RPC config strings as private credentials,
but do inspect any unexpected hit outside test/example paths.
Finder may recreate ignored `.DS_Store` files in local build folders; the
publication risk is whether they are tracked or unignored.

Build and smoke-test the standard app:

```sh
git diff --check
make -j"$(sysctl -n hw.ncpu)"
./contrib/defcoin-smoke-test.sh
make appbundle
DEFCOIN_PACKAGE_THEMES=0 ARCH_LABEL=arm ./contrib/defcoin-macos-package-dmg.sh v2026.1
```

Verify the standard app identity:

```sh
./src/qt/defcoin-qt -version
./src/defcoind -version
```

Expected release identity:

```text
Defcoin Core version v2026.1
Codename: Token Jester
```

Expected peer subversion from `getnetworkinfo`:

```text
/DefcoinCore:2026.1/
```

## GitHub Release Notes

Use `CHANGELOG.md` and `doc/release-notes/release-notes-2026.1.md` as the
starting point for release text.

For a GitHub Release upload, attach release artifacts from
`Distribution_Versions` rather than committing DMGs or ZIPs into the source
tree.

## Current Status

- Official Litecoin upstream tags were checked on 2026-05-09 with
  `git ls-remote`; `v0.21.5.5` was present with peeled commit
  `d8c8adc0b46cf96cc713455ba4aeb135efabac86`.
- The standard repository should be pushed from a clean working tree after
  `git diff --check` and the focused build/smoke checks pass.
- The standard GitHub release should attach the Apple Silicon DMG above and a
  SHA-256 checksum file.

## Publication Provenance Note

For the public README or release notes, use this wording if project provenance
is included:

> Defcoin Core 2026.1 was ported from Litecoin Core `v0.21.5.5` with Codex
> running GPT-5.5 at Extra High reasoning as the primary coding assistant. The
> port combined the current Litecoin Core source with documentation and source
> evidence from previous Defcoin builds, then applied Defcoin chain parameters,
> branding, seed infrastructure, packaging, and UI updates for modern macOS and
> Windows builds.
