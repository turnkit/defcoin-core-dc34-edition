# Defcoin Core Changelog

## 2026.1 Token Jester - Development Checkpoint

This checkpoint formalizes the split between the standard Defcoin Core build
and the experimental Defcoin Core DC34 Edition.

### Added

- User-facing release identity `2026.1`, codename `Token Jester`.
- Current Apple Silicon standard distribution artifact:
  `Defcoin-Core-v2026.1-arm.dmg`.
- Current Apple Silicon DC34 Edition distribution artifact:
  `Defcoin-Core-DC34 Edition-Build-v2026.1-arm.dmg`.
- Publication-facing difference document:
  `doc/defcoin-core-vs-litecoin-core.md`.
- GitHub publication checklist:
  `doc/github-publication-checklist.md`.
- Draft `2026.1` release notes:
  `doc/release-notes/release-notes-2026.1.md`.
- `doc/defcoin-2026.1-development-checkpoint.md` documenting build philosophy,
  packaging boundaries, standard-vs-DC34 Edition expectations, and UI principles.
- `doc/defcoin-unfinished-work-log.md` as the active sorted backlog.
- `NEXT_SESSION_HANDOFF.md` for a future session to resume without relying on
  chat history.

### Changed

- Mainnet seed list now uses `seed.defcoin.mikej.tech` in place of
  `seed2.defcoin.io`.
- Standard Apple Silicon app and DMG are rebuilt with DC34 Edition UI disabled and
  versioned as `2026.1`.
- Distribution outputs are organized under `Distribution_Versions/`.
- Preferences wording now replaces mixed Options/Settings wording in the main
  wallet menu and dialog surface.
- The gear button opens Preferences on normal click; press-and-hold can expose
  the Preferences tab menu.
- The main menu now includes `Preferences -> Text Size` controls.
- Linked wallet/node text resizing now defaults on, so main-window text size
  changes are applied across wallet pages, menus, table headers, child dialogs,
  and the Preferences/config dialog surface.
- Main-window text resizing now invalidates affected Qt layouts so text and
  controls can reflow after size changes.
- Coin-control tooltip text is now plain-language wallet guidance.
- Network Traffic uses `Display Logarithmically` terminology and reserves more
  top padding so chart labels do not clip.
- Peers Map presentation data is now labeled `Top 150 Data Centers` and uses a
  curated 150-market data-center list for DC34 Edition rendering tests.
- Traffic Health has a lightweight heartbeat animation so stable ping values
  still read as live node activity.
- Peers now includes `Show connected peers` and `Auto-size Columns`, and the
  Banned Peers panel can stay visible with a placeholder row when there are no
  bans.
- `Locate Peers` wording was renamed to `Lookup Peer Locations`.

## v0.21.5.4-defcoin.20260429 - Developer DMG Release

This branch is the first native Apple Silicon GUI port of Defcoin Core from
current Litecoin Core v0.21.5.4 source.

### Added

- Native macOS Apple Silicon build path for `Defcoin-Qt.app` and the Defcoin
  command-line tools.
- Defcoin mainnet seed hosts:
  - `seed.defcoin.io`
  - `seed.defcoin.mikej.tech`
  - `seed.defcoin.dc903.org`
- Fixed seed fallback entries for the currently reachable Defcoin seed nodes.
- Clone-safe Apple Silicon helper files:
  - `contrib/apple-sdk-le32-shim.h`
  - `contrib/build-boost-arm64.sh`
- `contrib/defcoin-smoke-test.sh` for local architecture, version, GUI, regtest
  daemon, CLI, and datadir checks.
- `contrib/defcoin-macos-package-dmg.sh` for building an ad-hoc signed Apple
  Silicon developer DMG with bundled Qt runtime.
- Theme `34`, based on the DC34 “Agency” style palette, including
  theme-aware splash/about coin artwork.
- Expanded Node window peer diagnostics with FQDN, version, services, geo,
  Ping Health history, configurable peer table views, and an average peer
  latency overlay on Network Traffic.
- Lightweight local-network peer discovery for private IPv4 LAN peers that
  have the Defcoin P2P port open.
- Structured theme asset packs under `src/qt/res/themes/`, plus generated
  `custom_themes/example_theme` modder folders under the Defcoin data
  directory.

### Changed

- Branding, bundle metadata, URL scheme, default config file, and user-facing
  labels from Litecoin to Defcoin.
- Display units from Litecoin-style subdivisions to `DFC`, `packets`, `tocks`,
  and `motes`.
- About dialog and splash text to use Defcoin artwork, credit Litecoin Core and
  Bitcoin Core derivation, and show the mainnet seed hostnames.
- macOS default data directory to `~/Library/Application Support/Defcoin/`.
- Main toolbar, status icons, Node window layout, console timestamp sizing, and
  dark/modern theme contrast for better macOS usability.
- RPC console font resizing now recomputes timestamp/icon/message columns from
  measured font metrics instead of fixed HTML widths.
- Network Traffic average peer latency now treats missing latency samples as
  gaps instead of graphing them as zero.
- The DMG packaging script now bundles QtSvg and the Qt SVG icon/image plugins
  so SVG toolbar/status icons render in the isolated packaged app.
- Peer Edit View now applies newly checked columns immediately without requiring
  a manual move up/down nudge.
- Gear and theme toolbar buttons no longer show menu drop-down arrows.
- The `34` theme now uses the raised DC34-style source logo for the
  theme button and the grimed DC34 source image for splash/About artwork.
- Theme discovery now scans built-in app resources and user
  `custom_themes/` folders at launch and when theme menus are opened. A custom
  theme is enabled unless its folder contains a file or folder named
  `DISABLED`.
- The modder `example_theme` folder tree is created in the user's Defcoin data
  directory at runtime without shipping duplicate copies of the default theme
  graphics in the app bundle or DMG.
- Development-only source artwork, scratch graphics, and handoff notes are kept
  outside the public repository so a clone contains only files needed for a
  functional build.
- The current DMG asset and checksum file are published at
  https://github.com/turnkit/defcoin-core/releases/tag/v0.21.5.4-defcoin.20260429.

### Deferred

- The Local Mining menu/panel is intentionally deferred until the current GUI
  build and mainnet peer behavior are stable.
- Old `wallet.dat` compatibility still needs isolated testing with copied
  wallets before production use.
- Public release signing and notarization still require an Apple Developer ID
  certificate.
