#pragma once

#ifdef __APPLE__
#include <sys/endian.h>

/*
 * Current macOS SDKs provide endian helper names in sys/endian.h. Litecoin
 * 0.21.x also defines local Scrypt helpers with those names. Keep the source
 * tree unmodified for the baseline build by renaming Litecoin's local helpers
 * after the SDK declarations have been seen.
 */
#define le32dec litecoin_scrypt_le32dec
#define le32enc litecoin_scrypt_le32enc
#define be32dec litecoin_scrypt_be32dec
#define be32enc litecoin_scrypt_be32enc
#endif
