# Defcoin Core Help Manual

Last updated: 2026-05-01

This manual mirrors the in-app Help menu content and adds source links for users who want deeper technical context. It is written for normal wallet users first, with enough detail for node operators to understand what the diagnostic pages mean.

## Search Index

- [Intro guide](#intro-guide)
- [Wallet pages](#wallet-pages)
- [Status icons](#status-icons)
- [Options](#options)
- [Pruning and storage](#pruning-and-storage)
- [Node window](#node-window)
- [Peers and discovery](#peers-and-discovery)
- [Wallet safety](#wallet-safety)
- [PSBT](#psbt)
- [Unsupported or compatibility-limited features](#unsupported-or-compatibility-limited-features)
- [Glossary](#glossary)
- [Known issues and review notes](#known-issues-and-review-notes)
- [Sources](#sources)

## Intro Guide

Defcoin Core is a native full-node wallet for the Defcoin network. It validates blocks, relays peer-to-peer traffic, stores wallet data locally, and gives the user direct control over receiving, sending, backup, encryption, and node operation.

Use Defcoin Core when you want the wallet and validating node on your own computer. The wallet pages manage keys and transactions. The Node window shows synchronization, peer connectivity, network traffic, latency, local diagnostics, and peer inspection tools.

Create or open wallets carefully. Back up `wallet.dat` before upgrades, imports, rescans, recovery work, or moving to another computer. If the wallet is encrypted, keep the passphrase offline and test backups before relying on them.

Historically, Defcoin has been used by its community online and in DC34-related in-person contexts, including badge-oriented experiments and paper-wallet style exchanges. This is community history, not a current official DC34 endorsement.

## Wallet Pages

Overview shows balances, recent transactions, synchronization status, and quick wallet state.

Send creates outgoing Defcoin payments. Confirm the destination address, amount, fee, and change behavior before sending.

Receive creates receive requests and addresses. A receive request is a convenience record; the actual spendable object is the address and the transaction that later pays it.

Transactions lists wallet activity. Double-click a transaction for detail, confirmations, transaction ID, and status.

Node opens node diagnostics: Information, Console, Network Traffic, Peers, banned peers, and peer inspection tools.

## Status Icons

Unit Selector changes display units only. It does not alter balances or transactions. Defcoin = 1.0 DFC, Packet = 0.001 DFC, Tock = 0.000001 DFC, and Mote = 0.00000001 DFC.

Wallet Lock shows encryption and lock state. Unlock only when spending or signing.

HD shows whether hierarchical deterministic key generation is enabled. HD wallets derive future addresses from wallet seed material, making backups more durable than one-backup-per-key workflows.

Proxy appears when proxy routing is active. Configure it in Options > Network. Advanced settings include `proxy`, `onion`, and `proxyrandomize` in `defcoin.conf` or on the command line.

Network Activity shows whether the node is connected to peers. Click it to disable or re-enable network activity temporarily.

Synchronization Checkmark shows whether the node is up to date. Click it to inspect synchronization progress while catching up.

## Options

Main controls startup behavior, database cache, pruning, and script verification threads.

Wallet controls wallet behavior such as coin control and spend-related display options.

Network controls incoming connections, UPnP, proxy routing, and reachability by IPv4, IPv6, Tor, or other supported transports.

Display controls language, theme, amount unit, third-party transaction URLs, and text-size behavior.

## Pruning and Storage

Pruning saves disk space by deleting old raw block and undo files after those files have been validated and used to build the block index and UTXO database. A pruned node still validates the chain, relays transactions and recent blocks, and maintains wallet state.

Warning: pruning limits old wallet rescans and is incompatible with transaction-index behavior. Returning to an unpruned node requires redownloading or reindexing the blockchain. If you expect to import old keys, rescan old wallets, or run explorer-style queries, keep an unpruned node.

## Node Window

Information shows client version, user agent, data directory, blocks directory, connections, mempool size, chain height, and verification progress.

Console runs RPC commands. Use it only when you understand the command because RPC can change wallet or node state. The console is powerful and intentionally warns against pasting untrusted commands.

Network Traffic charts received bytes, sent bytes, recent average latency, peer-average latency, optional moving averages, and visible time ranges. Lower latency means a peer set is responding faster.

Peers lists connected, inactive, local, and optionally banned peers. Columns include Node ID, Node, Port, FQDN, Domain Alias, Version, Services, Avg Ping, Recent Ping, Traffic Health, Sent, Received, User Agent, UA Count, Geo, City/St, and Seed.

Traffic Health is a compact heartbeat-style graph for each peer. It summarizes recent ping timing and traffic activity. Green generally means faster relative response, amber moderate response, and red slower response.

Locate Peers is a best-effort location lookup. Public peers use their IP address; local and LAN rows use this node's public WAN address. Local, LAN, private, Tor, and I2P addresses are not sent to public lookup services. Results are cached only for the running app session unless later settings explicitly add persistence.

Trace Route opens a separate traceroute window for the selected public peer. This uses system networking tools and is diagnostic only.

Peer Map is available in the DC34 Edition. It visualizes peers that have usable location data on an animated globe, can show real peers or presentation data sets, and includes a collapsible route list. Location data is approximate and diagnostic. Traffic lines between remote peers are illustrative because this node cannot directly measure traffic exchanged between two other peers.

## Peers and Discovery

Explicit `addnode` works when the exact host and port are known. Automatic discovery depends on DNS seeds, fixed seeds, or normal peer address gossip advertising the exact endpoint. A DNS `A` or `AAAA` record only gives an address, not a non-default Defcoin port, so non-default-port discovery requires either an explicit seed entry with the port or address gossip from another peer.

The "Only accept Defcoin-prefixed User Agents" setting rejects peers whose advertised user agent does not begin with `Defcoin`, ignoring case. This improves network hygiene, but it may reduce discovery if older nodes relay useful addresses while identifying themselves with legacy Litecoin-style user agents.

## Wallet Safety

- Encrypt wallets that hold meaningful value.
- Keep multiple offline backups of `wallet.dat` after creating or importing keys.
- Do not paste commands from strangers into the Console.
- Do not expose private keys to miners, pools, websites, or support chats.
- Test restores with a copy before depending on a backup.

## PSBT

Use the phrase Partially Signed Bitcoin/Defcoin Transaction (PSBT) for user-facing text. PSBT is a Bitcoin-origin transaction handoff format used by wallets and signers. In Defcoin Core it should be treated as an advanced signing workflow. Because Defcoin mainnet is intentionally legacy-compatible, do not assume every modern Bitcoin or Litecoin PSBT workflow is appropriate without testing.

## Unsupported or Compatibility-Limited Features

Defcoin mainnet does not currently treat SegWit, CSV, CLTV, BIP66, Taproot, MWEB, or Signet as active Defcoin mainnet features. Some inherited Litecoin Core code paths still exist because the app is based on Litecoin Core, but the GUI should avoid presenting unsupported Litecoin-only behavior as a Defcoin feature.

## Glossary

DFC: the primary Defcoin unit.

Packet: 0.001 DFC.

Tock: 0.000001 DFC.

Mote: 0.00000001 DFC, the smallest displayed unit.

Berkeley DB wallet: the legacy `wallet.dat` database format used by historical Defcoin wallets.

FQDN: fully qualified domain name, a complete DNS name for a host.

HD wallet: hierarchical deterministic wallet that derives future keys from wallet seed material.

Latency: network delay measured from peer ping timing. Lower is usually better.

Mempool: transactions your node knows about that are waiting to be mined into a block.

Peer: another node connected to your node.

Seed node: a DNS host or fixed address used to discover peers when your node starts.

## Known Issues and Review Notes

- Explicit `addnode` can connect to known IPv6 or non-default-port peers, but automatic discovery still depends on DNS seeds or peer address gossip advertising that exact address and port.
- The User Agent filter may reduce address-discovery paths if legacy peers identify as Litecoin-style nodes while still relaying useful Defcoin addresses.
- The Peer Map tab is a DC34 Edition prototype. Map imagery, animation smoothness, and Windows performance are still under active review.
- The DMG produced by local packaging is ad-hoc signed for testing. Public distribution should use Developer ID signing and notarization.
- Geo and reverse-DNS data are best-effort diagnostics, not consensus or identity proof.

## Sources

- [Bitcoin Core RPC documentation](https://bitcoincore.org/en/doc/)
- [Bitcoin Core 0.11.0 release notes: block file pruning](https://bitcoincore.org/en/releases/0.11.0/)
- [BIP 174: Partially Signed Bitcoin Transaction Format](https://bips.dev/174/)
- [BIP 32: Hierarchical Deterministic Wallets](https://bips.dev/32/)
- [BIP 155: addrv2 message](https://bips.dev/155/)
- [Bitcoin Optech: Using descriptors and PSBT](https://bitcoinops.org/en/river-descriptors-psbt/)
- Peer-to-peer: [Wikipedia](https://en.wikipedia.org/wiki/Peer-to-peer) | [Grokipedia](https://grokipedia.com/page/Peer-to-peer)
- Fully qualified domain name: [Wikipedia](https://en.wikipedia.org/wiki/Fully_qualified_domain_name) | [Grokipedia](https://grokipedia.com/page/Fully_qualified_domain_name)
- Latency: [Wikipedia](https://en.wikipedia.org/wiki/Latency_(engineering)) | [Grokipedia](https://grokipedia.com/page/Latency_(engineering))
- Traceroute: [Wikipedia](https://en.wikipedia.org/wiki/Traceroute) | [Grokipedia](https://grokipedia.com/page/Traceroute)
- Berkeley DB: [Wikipedia](https://en.wikipedia.org/wiki/Berkeley_DB) | [Grokipedia](https://grokipedia.com/page/Berkeley_DB)
