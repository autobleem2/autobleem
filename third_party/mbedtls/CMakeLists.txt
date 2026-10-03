# mbedTLS 3.6.7 LTS (Apache-2.0), vendored for abfetch - AutoBleem's HTTPS client (src/tools/abfetch). Only
# what autobleem_config.h compiles is here: library/ lost every source that config leaves empty, include/ is
# whole. Upstream: https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7, the release tarball's
# sha256 a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6. To upgrade: replace include/ and
# the listed library/ files from a newer 3.6.x tarball, keep autobleem_config.h, and rebuild - a link error
# names a source that is now needed.
add_library(abmbedtls STATIC
    library/aes.c
    library/asn1parse.c
    library/asn1write.c
    library/base64.c
    library/bignum.c
    library/bignum_core.c
    library/chacha20.c
    library/chachapoly.c
    library/cipher.c
    library/cipher_wrap.c
    library/constant_time.c
    library/ctr_drbg.c
    library/ecdh.c
    library/ecdsa.c
    library/ecp.c
    library/ecp_curves.c
    library/entropy.c
    library/entropy_poll.c
    library/error.c
    library/gcm.c
    library/md.c
    library/net_sockets.c
    library/oid.c
    library/pem.c
    library/pk.c
    library/pk_ecc.c
    library/pk_wrap.c
    library/pkparse.c
    library/platform.c
    library/platform_util.c
    library/poly1305.c
    library/rsa.c
    library/rsa_alt_helpers.c
    library/sha1.c
    library/sha256.c
    library/sha512.c
    library/ssl_ciphersuites.c
    library/ssl_client.c
    library/ssl_msg.c
    library/ssl_tls.c
    library/ssl_tls12_client.c
    library/x509.c
    library/x509_crt.c
)
target_include_directories(abmbedtls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/library)
target_compile_definitions(abmbedtls PUBLIC MBEDTLS_CONFIG_FILE="autobleem_config.h")
set_target_properties(abmbedtls PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
# vendored code, as the other third_party trees: no warnings of ours
target_compile_options(abmbedtls PRIVATE -w)
if (WIN32)
    target_link_libraries(abmbedtls PUBLIC ws2_32 bcrypt)
endif()
