#include "SyntheticPasswordCrypto.h"
#include <cassert>
#include <iostream>

int main() {
    // NIST AES-256-GCM: zero key, zero 96-bit IV, zero 128-bit plaintext.
    const uint8_t key[32]={};
    const uint8_t ciphertext_and_tag[] = {
        0xce,0xa7,0x40,0x3d,0x4d,0x60,0x6b,0x6e,0x07,0x4e,0xc5,0xd3,0xba,0xf3,0x9d,0x18,
        0xd0,0xd1,0xc8,0xa7,0x99,0x99,0x6b,0xf0,0x26,0x5b,0x98,0xb5,0xd4,0x8a,0xb9,0x19};
    std::vector<uint8_t> blob(12,0), plaintext;
    blob.insert(blob.end(),std::begin(ciphertext_and_tag),std::end(ciphertext_and_tag));
    using android::vold::DecryptSyntheticPasswordGcm;
    assert(DecryptSyntheticPasswordGcm(blob,key,sizeof(key),&plaintext));
    assert(plaintext==std::vector<uint8_t>(16,0));
    for (size_t i=0;i<blob.size();++i) {
        auto corrupt=blob; corrupt[i]^=1;
        assert(!DecryptSyntheticPasswordGcm(corrupt,key,sizeof(key),&plaintext));
        assert(plaintext.empty());
    }
    for (size_t n=0;n<blob.size();++n) {
        std::vector<uint8_t> truncated(blob.begin(),blob.begin()+n);
        assert(!DecryptSyntheticPasswordGcm(truncated,key,sizeof(key),&plaintext));
    }
    uint8_t wrong_key[32]={1};
    assert(!DecryptSyntheticPasswordGcm(blob,wrong_key,sizeof(wrong_key),&plaintext));
    assert(!DecryptSyntheticPasswordGcm(blob,key,16,&plaintext));
    assert(!DecryptSyntheticPasswordGcm(blob,nullptr,32,&plaintext));
    std::cout << "Synthetic password AES-GCM authentication tests passed\n";
}
