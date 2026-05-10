#ifndef BITCOIN_CHAINPARAMSSEEDS_H
#define BITCOIN_CHAINPARAMSSEEDS_H
/**
 * List of fixed seed nodes for the Defcoin network.
 *
 * Each line contains a BIP155 serialized (networkID, addr, port) tuple.
 * These are used as a small-network fallback when DNS seeds and peers.dat do
 * not provide enough usable Defcoin peers.
 */
static const uint8_t chainparams_seed_main[] = {
    // seed.defcoin.io
    // 66.42.91.225:1337
    0x01,0x04,0x42,0x2a,0x5b,0xe1,0x05,0x39,

    // seed.defcoin.mikej.tech
    // 73.52.182.204:1337
    0x01,0x04,0x49,0x34,0xb6,0xcc,0x05,0x39,

    // seed.defcoin.dc903.org
    // 50.116.19.40:10332
    0x01,0x04,0x32,0x74,0x13,0x28,0x28,0x5c,
};

static const uint8_t chainparams_seed_test[] = {
};
#endif // BITCOIN_CHAINPARAMSSEEDS_H
