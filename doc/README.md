Defcoin Core
=============

Setup
---------------------
Defcoin Core is the original Defcoin client and it builds the backbone of the network. It downloads and, by default, stores the entire history of Defcoin transactions, which requires approximately 22 gigabytes of disk space. Depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

The current development release line is Defcoin Core `2026.1`, codename
`Token Jester`.

Running
---------------------
The following are some helpful notes on how to run Defcoin Core on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/defcoin-qt` (GUI) or
- `bin/defcoind` (headless)

### Windows

Unpack the files into a directory, and then run defcoin-qt.exe.

### macOS

Drag Defcoin Core to your applications folder, and then run Defcoin Core.

### Need Help?

Use the in-app Help manual for wallet and node basics. For developer and
release-preparation notes, start with the Defcoin-specific documents below.

Building
---------------------
The following are developer notes on how to build Defcoin Core on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [FreeBSD Build Notes](build-freebsd.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [NetBSD Build Notes](build-netbsd.md)
- [Gitian Building Guide (External Link)](https://github.com/bitcoin-core/docs/blob/master/gitian-building.md)

Defcoin-specific build and release preparation:

- [Defcoin Core vs. Litecoin Core](defcoin-core-vs-litecoin-core.md)
- [Defcoin Build Deviation Matrix](defcoin-build-deviation-matrix.md)
- [Defcoin Standard Conversion Notes](defcoin-litecoin-conversion-notes.md)
- [Defcoin Modern Build Guide](defcoin-modern-build-guide.md)
- [Litecoin Upstream Comparison](litecoin-upstream-comparison-standard.md)
- [GitHub Publication Checklist](github-publication-checklist.md)
- [Defcoin 2026.1 Development Checkpoint](defcoin-2026.1-development-checkpoint.md)
- [Defcoin Unfinished Work Log](defcoin-unfinished-work-log.md)
- [Defcoin Core Help Manual](defcoin-core-help-manual.md)

Development
---------------------
The Defcoin repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Productivity Notes](productivity.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://doxygen.bitcoincore.org/)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [JSON-RPC Interface](JSON-RPC-interface.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Resources
* Discuss on the [DefcoinTalk](https://defcointalk.io/) forums.
* Discuss general Defcoin development on #defcoin-dev on Freenode. If you don't have an IRC client, use [webchat here](https://webchat.freenode.net/#defcoin-dev).

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [bitcoin.conf Configuration File](bitcoin-conf.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Memory](reduce-memory.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)
- [PSBT support](psbt.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
