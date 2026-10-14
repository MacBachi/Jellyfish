#pragma once
// mbedtls, trimmed to what the over-the-air update needs: SHA-256 and ECDSA P-256
// verification of a signed image (jell_ota.cpp). Nothing else of mbedtls is compiled in.
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_SHA256_C
