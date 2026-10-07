/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>
#include <openssl/crypto.h>
#include <openssl/evp.h>

namespace android::vold {

// SyntheticPasswordCrypto.encrypt(): IV (12 bytes), ciphertext, GCM tag
// (16 bytes). Never release unauthenticated synthetic-password material.
inline bool DecryptSyntheticPasswordGcm(const std::vector<uint8_t>& blob,
                                       const uint8_t* key, size_t key_size,
                                       std::vector<uint8_t>* plaintext) {
    constexpr size_t iv_size = 12, tag_size = 16;
    if (!plaintext) return false;
    plaintext->clear();
    if (!key || key_size < 32 || blob.size() <= iv_size + tag_size ||
        blob.size() - iv_size - tag_size > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    const int ciphertext_size = static_cast<int>(blob.size() - iv_size - tag_size);
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>
        ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) return false;
    std::vector<uint8_t> output(ciphertext_size + tag_size);
    int written = 0, final_size = 0;
    const bool ok = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key, blob.data()) == 1 &&
        EVP_DecryptUpdate(ctx.get(), output.data(), &written, blob.data() + iv_size, ciphertext_size) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag_size,
            const_cast<uint8_t*>(blob.data() + blob.size() - tag_size)) == 1 &&
        EVP_DecryptFinal_ex(ctx.get(), output.data() + written, &final_size) == 1;
    if (!ok) {
        OPENSSL_cleanse(output.data(), output.size());
        return false;
    }
    output.resize(written + final_size);
    *plaintext = std::move(output);
    return true;
}

}  // namespace android::vold
