// mbedTLS configuration for abfetch (src/tools/abfetch), AutoBleem's own HTTPS client: the console fetches
// its online update with it, from the stick, so nothing depends on what the console's kernel payload ships.
// Built with -DMBEDTLS_CONFIG_FILE="autobleem_config.h" in place of mbedtls_config.h.
//
// What it is: a TLS 1.2 client with ECDHE (ECDSA or RSA certificates), AES-GCM and ChaCha20-Poly1305,
// X.509 chains checked against a PEM bundle (cacert.pem next to abfetch). What it deliberately is not:
//   - no TLS 1.3 (it needs the PSA crypto core; every server abfetch talks to speaks 1.2),
//   - no MBEDTLS_HAVE_TIME_DATE: a certificate's validity dates are NOT checked. The PlayStation Classic
//     has no battery-backed clock, so its date after a boot is whatever it is - checking it would refuse
//     every server on a console that has not synced the time. The chain, the signatures and the host name
//     are checked; what is downloaded is also checked against the sha256 in the catalog.
#ifndef AUTOBLEEM_MBEDTLS_CONFIG_H
#define AUTOBLEEM_MBEDTLS_CONFIG_H

// system support
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_NET_C
#define MBEDTLS_PLATFORM_C // mbedTLS requires it on Windows (its snprintf); harmless elsewhere

// random numbers
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

// ciphers and hashes
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_CHACHAPOLY_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C

// public keys
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C

// certificates
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
// no MBEDTLS_FS_IO: abfetch reads the CA bundle itself (with FS_IO but without HAVE_TIME, 3.6's x509_crt.c
// does not compile on Windows - it includes windows.h only for the time functions)

// TLS 1.2, client side
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED

// messages for the log
#define MBEDTLS_ERROR_C

#endif
