// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_PROOF_H
#define COMMON_PROOF_H

#include <stream/datastream.h>

#include "destination.h"
#include "key.h"
#include "uint256.h"
#include "util.h"

class CProofOfHashWorkCompact
{
public:
    int32_t btcVersion;
    uint256 btcHashPrevBlock;
    uint256 btcHashMerkleRoot;
    uint32_t btcTime;
    uint32_t btcBits;
    uint32_t btcNonce;

    uint16_t auxOffset;
    std::vector<uint8_t> coinbase;
    std::vector<uint256> auxMerkleBranch;
    std::vector<uint256> trMerkleBranch;
    enum
    {
        PROOFHASHWORK_SIZE = 860
    };
public:
    CProofOfHashWorkCompact()
    {
    }
    void Save(std::vector<unsigned char>& vchProof)
    {
        try
        {
            xengine::CBufStream ss;
            ss << btcVersion << btcHashPrevBlock << btcHashMerkleRoot << btcTime << btcBits << btcNonce << auxOffset << coinbase << auxMerkleBranch << trMerkleBranch;
            vchProof.assign(ss.GetData(), ss.GetData() + ss.GetSize());
        }
        catch (const std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
        }
    }

    void Load(const std::vector<unsigned char>& vchProof)
    {
        try
        {
            xengine::CBufStream ss;
            ss.Write((const char*)vchProof.data(),vchProof.size());
            ss >> btcVersion >> btcHashPrevBlock >> btcHashMerkleRoot >> btcTime >> btcBits >> btcNonce >> auxOffset >> coinbase >> auxMerkleBranch >> trMerkleBranch;   
        }
        catch (const std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
        }
    }

    std::vector<uint8_t> BuildCoinBase(std::vector<uint8_t> &data1,
                                        std::vector<uint8_t> &data2, 
                                        uint256 &mam_pow,
                                        std::vector<uint256> &_auxMerkleBranch) const
    {
        for (auto obj : _auxMerkleBranch)
        {
            uint8_t buf[64] = {0};
            memcpy(buf,mam_pow.begin(),32);
            memcpy(buf+32,obj.begin(),32);
            mam_pow = minemon::crypto::CryptoSHA256D(buf,64);
        }
        std::vector<uint8_t> res;
        res.insert(res.end(),data1.begin(),data1.end());
        res.insert(res.end(),mam_pow.begin(),mam_pow.end());
        res.insert(res.end(),data2.begin(),data2.end());
        return res;
    }

    void SetBtcPow(uint16_t _auxOffset, 
                    std::vector<uint8> &_coinbase, 
                    std::vector<uint256> &_auxMerkleBranch,
                    std::vector<uint256> &_trMerkleBranch,
                    int32_t _btcVersion,
                    uint256 _btcHashPrevBlock,
                    uint32_t _btcBits)
    {
        auxOffset = _auxOffset;
        coinbase = _coinbase;
        auxMerkleBranch = _auxMerkleBranch;
        trMerkleBranch = _trMerkleBranch;
        uint256 root = minemon::crypto::CryptoSHA256D(coinbase.data(),coinbase.size());
        for (auto obj : trMerkleBranch)
        {
            uint8_t buf[64] = {0};
            memcpy(buf,root.begin(),32);
            memcpy(buf+32,obj.begin(),32);
            root = minemon::crypto::CryptoSHA256D(buf,64);
        }
        btcHashMerkleRoot = root;
        btcVersion = _btcVersion;
        btcHashPrevBlock = _btcHashPrevBlock;
        btcBits = _btcBits;
        btcNonce = 0;
    }

    void GetBtcPow(std::vector<uint8>& vch)
    {
        try
        {
            xengine::CBufStream ss;
            ss << btcVersion << btcHashPrevBlock << btcHashMerkleRoot << btcTime << btcBits << btcNonce;
            vch.assign(ss.GetData(), ss.GetData() + ss.GetSize());
        }
        catch (const std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
        }
    }

    void SetBtcPow(std::vector<uint8>& vch)
    {
        try
        {
            xengine::CBufStream ss;
            ss.Write((const char*)vch.data(),vch.size());
            ss >> btcVersion >> btcHashPrevBlock >> btcHashMerkleRoot >> btcTime >> btcBits >> btcNonce;
        }
        catch (const std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
        }
    }

    static bool VerificationMerkleTree(const uint256 &root, const uint256 &hash, const std::vector<uint256> &merkle_branch)
    {
        uint256 res = hash;
        for (auto obj : merkle_branch)
        {
            uint8_t buf[64] = {0};
            memcpy(buf,res.begin(),32);
            memcpy(buf+32,obj.begin(),32);
            res = minemon::crypto::CryptoSHA256D(buf,64);
        }
        return res == root;
    }
};

#endif //COMMON_PROOF_H
