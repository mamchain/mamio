// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto.h"

#include <iostream>
#include <sodium.h>
#include <sys/mman.h>

#include "util.h"

using namespace std;

namespace minemon
{
namespace crypto
{

//////////////////////////////
// CCryptoSodiumInitializer()
class CCryptoSodiumInitializer
{
public:
    CCryptoSodiumInitializer()
    {
        if (sodium_init() < 0)
        {
            throw CCryptoError("CCryptoSodiumInitializer : Failed to initialize libsodium");
        }
    }
    ~CCryptoSodiumInitializer()
    {
    }
};

static CCryptoSodiumInitializer _CCryptoSodiumInitializer;

//////////////////////////////
// Secure memory
void* CryptoAlloc(const size_t size)
{
    return sodium_malloc(size);
}

void CryptoFree(void* ptr)
{
    sodium_free(ptr);
}

//////////////////////////////
// Heap memory lock

void CryptoMLock(void* const addr, const size_t len)
{
    if (sodium_mlock(addr, len) < 0)
    {
        throw CCryptoError("CryptoMLock : Failed to mlock");
    }
}

void CryptoMUnlock(void* const addr, const size_t len)
{
    if (sodium_munlock(addr, len) < 0)
    {
        throw CCryptoError("CryptoMUnlock : Failed to munlock");
    }
}

//////////////////////////////
// Rand

uint32 CryptoGetRand32()
{
    return randombytes_random();
}

uint64 CryptoGetRand64()
{
    return (randombytes_random() | (((uint64)randombytes_random()) << 32));
}

void CryptoGetRand256(uint256& u)
{
    randombytes_buf(&u, 32);
}

//////////////////////////////
// Hash

uint256 CryptoHash(const void* msg, size_t len)
{
    uint256 hash;
    crypto_generichash_blake2b((uint8*)&hash, sizeof(hash), (const uint8*)msg, len, nullptr, 0);
    return hash;
}

uint256 CryptoHash(const uint256& h1, const uint256& h2)
{
    uint256 hash;
    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init(&state, nullptr, 0, sizeof(hash));
    crypto_generichash_blake2b_update(&state, h1.begin(), sizeof(h1));
    crypto_generichash_blake2b_update(&state, h2.begin(), sizeof(h2));
    crypto_generichash_blake2b_final(&state, hash.begin(), sizeof(hash));
    return hash;
}

uint256 CryptoPowHash(const void* msg, size_t len)
{
    return CryptoSHA256D(msg,len);
}

//////////////////////////////
// SHA256D
uint256 CryptoSHA256D(const void* msg, size_t len)
{
    uint8_t buf[32] = {0};
    crypto_hash_sha256(buf, (const uint8*)msg, len);
    uint256 hash;
    crypto_hash_sha256((uint8*)&hash, buf, 32);
    return hash;
}

//////////////////////////////
// SHA256
uint256 CryptoSHA256(const void* msg, size_t len)
{
    uint256 hash;
    crypto_hash_sha256((uint8*)&hash, (const uint8*)msg, len);
    return hash;
}

//////////////////////////////
// Sign & verify

uint256 CryptoMakeNewKey(CCryptoKey& key)
{
    uint256 pk;
    while (crypto_sign_ed25519_keypair((uint8*)&pk, (uint8*)&key) == 0)
    {
        int count = 0;
        const uint8* p = key.secret.begin();
        for (int i = 1; i < 31; ++i)
        {
            if (p[i] != 0x00 && p[i] != 0xFF)
            {
                if (++count >= 4)
                {
                    return pk;
                }
            }
        }
    }
    return uint64(0);
}

uint256 CryptoImportKey(CCryptoKey& key, const uint256& secret)
{
    uint256 pk;
    crypto_sign_ed25519_seed_keypair((uint8*)&pk, (uint8*)&key, (uint8*)&secret);
    return pk;
}

void CryptoSign(const CCryptoKey& key, const void* md, const size_t len, vector<uint8>& vchSig)
{
    vchSig.resize(64);
    crypto_sign_ed25519_detached(&vchSig[0], nullptr, (const uint8*)md, len, (const uint8*)&key);
}

bool CryptoVerify(const uint256& pubkey, const void* md, const size_t len, const vector<uint8>& vchSig)
{
    return (vchSig.size() == 64
            && !crypto_sign_ed25519_verify_detached(&vchSig[0], (const uint8*)md, len, (const uint8*)&pubkey));
}

// return the nIndex key is signed in multiple signature
static bool IsSigned(const uint8* pIndex, const size_t nLen, const size_t nIndex)
{
    return (nLen * 8 > nIndex) && pIndex[nLen - 1 - nIndex / 8] & (1 << (nIndex % 8));
}

// set the nIndex key signed in multiple signature
static bool SetSigned(uint8* pIndex, const size_t nLen, const size_t nIndex)
{
    if (nLen * 8 <= nIndex)
    {
        return false;
    }
    pIndex[nLen - 1 - nIndex / 8] |= (1 << (nIndex % 8));
    return true;
}

bool CryptoMultiSign(const set<uint256>& setPubKey, const CCryptoKey& privkey, const uint8* pM, const size_t lenM, vector<uint8>& vchSig)
{
    if (setPubKey.empty())
    {
        xengine::StdTrace("multisign", "key set is empty");
        return false;
    }

    // index
    set<uint256>::const_iterator itPub = setPubKey.find(privkey.pubkey);
    if (itPub == setPubKey.end())
    {
        xengine::StdTrace("multisign", "no key %s in set", privkey.pubkey.ToString().c_str());
        return false;
    }
    size_t nIndex = distance(setPubKey.begin(), itPub);

    // unpack index of  (index,S,Ri,...,Rj)
    size_t nIndexLen = (setPubKey.size() - 1) / 8 + 1;
    if (vchSig.empty())
    {
        vchSig.resize(nIndexLen);
    }
    else if (vchSig.size() < nIndexLen + 64)
    {
        xengine::StdTrace("multisign", "vchSig size %lu is too short, need %lu minimum", vchSig.size(), nIndexLen + 64);
        return false;
    }
    uint8* pIndex = &vchSig[0];

    // already signed
    if (IsSigned(pIndex, nIndexLen, nIndex))
    {
        xengine::StdTrace("multisign", "key %s is already signed", privkey.pubkey.ToString().c_str());
        return true;
    }

    // position of RS, (index,Ri,Si,...,R,S,...,Rj,Sj)
    size_t nPosRS = nIndexLen;
    for (size_t i = 0; i < nIndex; ++i)
    {
        if (IsSigned(pIndex, nIndexLen, i))
        {
            nPosRS += 64;
        }
    }
    if (nPosRS > vchSig.size())
    {
        xengine::StdTrace("multisign", "index %lu key is signed, but not exist R", nIndex);
        return false;
    }

    // sign
    vector<uint8> vchRS;
    CryptoSign(privkey, pM, lenM, vchRS);

    // record
    if (!SetSigned(pIndex, nIndexLen, nIndex))
    {
        xengine::StdTrace("multisign", "set %lu index signed error", nIndex);
        return false;
    }
    vchSig.insert(vchSig.begin() + nPosRS, vchRS.begin(), vchRS.end());

    return true;
}

bool CryptoMultiVerify(const set<uint256>& setPubKey, const uint8* pM, const size_t lenM, const vector<uint8>& vchSig, set<uint256>& setPartKey)
{
    if (setPubKey.empty())
    {
        xengine::StdTrace("multiverify", "key set is empty");
        return false;
    }

    // unpack (index,Ri,Si,...,Rj,Sj)
    int nIndexLen = (setPubKey.size() - 1) / 8 + 1;
    if (vchSig.size() < (nIndexLen + 64))
    {
        xengine::StdTrace("multiverify", "vchSig size %lu is too short, need %lu minimum", vchSig.size(), nIndexLen + 64);
        return false;
    }
    const uint8* pIndex = &vchSig[0];

    size_t nPosRS = nIndexLen;
    size_t i = 0;
    for (auto itPub = setPubKey.begin(); itPub != setPubKey.end(); ++itPub, ++i)
    {
        if (IsSigned(pIndex, nIndexLen, i))
        {
            const uint256& pk = *itPub;
            if (nPosRS + 64 > vchSig.size())
            {
                xengine::StdTrace("multiverify", "index %lu key is signed, but not exist R", i);
                return false;
            }

            vector<uint8> vchRS(vchSig.begin() + nPosRS, vchSig.begin() + nPosRS + 64);
            if (!CryptoVerify(pk, pM, lenM, vchRS))
            {
                xengine::StdTrace("multiverify", "verify index %lu key sign failed", i);
                return false;
            }

            setPartKey.insert(pk);
            nPosRS += 64;
        }
    }

    if (nPosRS != vchSig.size())
    {
        xengine::StdTrace("multiverify", "vchSig size %lu is too long, need %lu", vchSig.size(), nPosRS);
        return false;
    }

    return true;
}

/******** defect old version multi-sign end *********/

// Encrypt
void CryptoKeyFromPassphrase(int version, const CCryptoString& passphrase, const uint256& salt, CCryptoKeyData& key)
{
    key.resize(32);
    if (version == 0)
    {
        key.assign(salt.begin(), salt.end());
    }
    else if (version == 1)
    {
        if (crypto_pwhash_scryptsalsa208sha256(&key[0], 32, passphrase.c_str(), passphrase.size(), (const uint8*)&salt,
                                               crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE,
                                               crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE)
            != 0)
        {
            throw CCryptoError("CryptoKeyFromPassphrase : Failed to create key, key version = 1");
        }
    }
    else
    {
        throw CCryptoError("CryptoKeyFromPassphrase : Failed to create key, unknown key version");
    }
}

bool CryptoEncryptSecret(int version, const CCryptoString& passphrase, const CCryptoKey& key, CCryptoCipher& cipher)
{
    CCryptoKeyData ek;
    CryptoKeyFromPassphrase(version, passphrase, key.pubkey, ek);
    randombytes_buf(&cipher.nonce, sizeof(cipher.nonce));

    return (crypto_aead_chacha20poly1305_encrypt(cipher.encrypted, nullptr,
                                                 (const uint8*)&key.secret, 32,
                                                 (const uint8*)&key.pubkey, 32,
                                                 nullptr, (uint8*)&cipher.nonce, &ek[0])
            == 0);
}

bool CryptoDecryptSecret(int version, const CCryptoString& passphrase, const CCryptoCipher& cipher, CCryptoKey& key)
{
    CCryptoKeyData ek;
    CryptoKeyFromPassphrase(version, passphrase, key.pubkey, ek);
    return (crypto_aead_chacha20poly1305_decrypt((uint8*)&key.secret, nullptr, nullptr,
                                                 cipher.encrypted, 48,
                                                 (const uint8*)&key.pubkey, 32,
                                                 (const uint8*)&cipher.nonce, &ek[0])
            == 0);
}

// aes encrypt
bool CryptoAesMakeKey(const uint256& secret_local, const uint256& pubkey_remote, uint256& key)
{
    unsigned char curve25519_secret_local[32];
    unsigned char curve25519_pubkey_remote[32];
    if (crypto_sign_ed25519_sk_to_curve25519(curve25519_secret_local, secret_local.begin()) != 0)
    {
        return false;
    }
    if (crypto_sign_ed25519_pk_to_curve25519(curve25519_pubkey_remote, pubkey_remote.begin()) != 0)
    {
        return false;
    }
    if (crypto_scalarmult_curve25519(key.begin(), curve25519_secret_local, curve25519_pubkey_remote) != 0)
    {
        return false;
    }
    return true;
}

const unsigned char aes_nonce[crypto_aead_aes256gcm_NPUBBYTES] = { 0x58, 0x36, 0x99, 0xaa, 0x63, 0x7c, 0x6e, 0x5e, 0xd6, 0x76, 0x1b, 0xee };
const unsigned char aes_additional[8] = { 0x42, 0xa6, 0x16, 0x09, 0x63, 0x66, 0x92, 0xe5 };

bool CryptoAesEncrypt(const uint256& secret_local, const uint256& pubkey_remote, const std::vector<uint8>& message, std::vector<uint8>& ciphertext)
{
    if (crypto_aead_aes256gcm_is_available() == 0)
    {
        return false;
    }

    uint256 key;
    if (!CryptoAesMakeKey(secret_local, pubkey_remote, key))
    {
        return false;
    }

    std::vector<uint8> ciphertext_buf;
    unsigned long long ciphertext_len = 0;
    ciphertext_buf.reserve(message.size() + crypto_aead_aes256gcm_ABYTES);

    if (crypto_aead_aes256gcm_encrypt(&(ciphertext_buf[0]), &ciphertext_len,
                                      &(message[0]), message.size(),
                                      aes_additional, sizeof(aes_additional),
                                      NULL, aes_nonce, key.begin())
        != 0)
    {
        return false;
    }
    if (ciphertext_len < crypto_aead_aes256gcm_ABYTES)
    {
        return false;
    }

    ciphertext.assign(ciphertext_buf.begin(), ciphertext_buf.begin() + ciphertext_len);
    return true;
}

bool CryptoAesDecrypt(const uint256& secret_local, const uint256& pubkey_remote, const std::vector<uint8>& ciphertext, std::vector<uint8>& message)
{
    if (crypto_aead_aes256gcm_is_available() == 0)
    {
        return false;
    }
    if (ciphertext.size() < crypto_aead_aes256gcm_ABYTES)
    {
        return false;
    }

    uint256 key;
    if (!CryptoAesMakeKey(secret_local, pubkey_remote, key))
    {
        return false;
    }

    std::vector<uint8> decrypted_buf;
    unsigned long long decrypted_len = 0;
    decrypted_buf.reserve(ciphertext.size());

    if (crypto_aead_aes256gcm_decrypt(&(decrypted_buf[0]), &decrypted_len, NULL,
                                      &(ciphertext[0]), ciphertext.size(),
                                      aes_additional, sizeof(aes_additional),
                                      aes_nonce, key.begin())
        != 0)
    {
        return false;
    }
    if (decrypted_len <= 0)
    {
        return false;
    }

    message.assign(decrypted_buf.begin(), decrypted_buf.begin() + decrypted_len);
    return true;
}

} // namespace crypto
} // namespace minemon
