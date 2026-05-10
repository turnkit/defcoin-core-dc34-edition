# Defcoin Core v0.21.5.4-defcoin.20260429 Release Notes

This development release is based on Litecoin Core v0.21.5.4 and identifies
the local Defcoin Apple Silicon GUI port with a date-based Defcoin suffix.

## Significant Changes

- Native Apple Silicon GUI and command-line build path for Defcoin Core.
- Defcoin branding, macOS app metadata, datadir, seed hosts, display units, and
  Defcoin artwork resources.
- Node window peer diagnostics with FQDN, protocol version, abbreviated
  services, geo flag, configurable peer table views, and Traffic Health history.
- Peers now include an optional synthetic local-node row at the top of the
  table. It is included in the peer and user-agent counts while enabled, and
  can be hidden from Options > Network.
- Peers now show current peer count, Seen Since Open count, and current
  user-agent distribution summary and pie chart directly above the peer table.
- Peers can optionally show inactive peers seen during the current Node window
  session. Seed-domain matches are marked with an optional `Seed` column, and
  FQDN cells show configured seed domains above reverse-DNS names when both are
  known.
- Network Traffic average peer latency overlay with preserved chart history
  when the range slider changes.
- Expanded in-app Defcoin Core Help Manual covering wallet safety, units,
  pruning, node diagnostics, status icons, and Defcoin community history notes.
- RPC console font resizing that recalculates timestamp, direction icon, and
  message column widths from the active font metrics.
- Lightweight local-network peer discovery for private IPv4 LAN peers with an
  open Defcoin P2P port. ARP-known LAN hosts are also probed on known legacy
  Defcoin/Litecoin-derived service ports used by older nodes.
- Defcoin mainnet outbound selection now permits already-discovered non-default
  peer ports while the node is below its Defcoin outbound target, fixing small
  network connectivity to reachable `:10332` and similar legacy peers without
  adding those hosts as baked-in seeds.
- Auto, Light, Dark, Modern, and DC34-inspired `34` UI themes, including
  theme-aware splash and About coin artwork.
- Focused smoke test script for local architecture, version, GUI, regtest
  daemon, CLI, and datadir checks.
- Ad-hoc signed Apple Silicon developer DMG with bundled Qt runtime:
  `Defcoin-Core-v0.21.5.4-defcoin.20260429-arm64-adhoc.dmg`.
- Refreshed DC34 theme artwork for the theme button, splash screen, and
  About dialog, and removed menu drop-down arrows from the gear/theme toolbar
  buttons.
- Built-in themes now use a structured theme folder tree, and user-added themes
  can be dropped into the runtime `custom_themes/` folder under the Defcoin data
  directory. Custom theme folders are enabled by default and ignored only when
  they contain a file or folder named `DISABLED`.

## Compatibility Notes

- The numeric Litecoin-derived client version remains 0.21.5.4 for compatibility
  with the inherited version machinery.
- The release label is `v0.21.5.4-defcoin.20260429`.
- The DMG is a developer distribution image. It is not Developer ID signed or
  notarized, so macOS may require right-click Open on first launch.
- The GitHub release includes a `.sha256` checksum asset for the current DMG.
- Back up any legacy `wallet.dat` before testing it with this branch.
- The app creates a disabled `example_theme` folder tree for modders under the
  user's Defcoin data directory. To enable that example, remove or rename its
  `DISABLED` marker.
- LAN discovery is intentionally conservative: it probes private IPv4 hosts
  inferred from local interfaces on the Defcoin P2P port, and probes ARP-known
  LAN hosts on a short list of known legacy service ports. A peer behind a
  firewall, not listening, or using an unknown custom P2P port still needs a
  manual `addnode=<ip>:<port>` entry.
