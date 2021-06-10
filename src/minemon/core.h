// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_CORE_H
#define MINEMON_CORE_H

#include "base.h"

namespace minemon
{

class CCoreProtocol : public ICoreProtocol
{
public:
    CCoreProtocol();
    virtual ~CCoreProtocol();
    virtual const uint256& GetGenesisBlockHash() override;
    virtual void GetGenesisBlock(CBlock& block) override;
    virtual Errno ValidateTransaction(const CTransaction& tx, int nHeight) override;
    virtual Errno ValidateBlock(const CBlock& block) override;
    virtual Errno ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile) override;

    virtual Errno VerifyBlockTx(const CTransaction& tx, const CTxContxt& txContxt, CBlockIndex* pIndexPrev, int nForkHeight, const uint256& fork) override;
    virtual Errno VerifyTransaction(const CTransaction& tx, const std::vector<CTxOut>& vPrevOutput, int nForkHeight, const uint256& fork) override;

    virtual Errno VerifyProofOfWork(const CBlock& block, const CBlockIndex* pIndexPrev) override;
    virtual bool GetBlockTrust(const CBlock& block, uint256& nChainTrust) override;
    virtual bool GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, uint32_t& nBits) override;
    virtual int64 GetMintWorkReward(const int nHeight) override;
    virtual int64 GetBlockPowReward(const int nHeight) override;
    virtual int64 GetBlockPledgeReward(const int nHeight) override;
    virtual int64 GetMintTotalReward(const int nHeight) override;
    virtual bool GetPledgeMinMaxValue(const uint256& hashPrevBlock, int64& nPowMinPledge, int64& nStakeMinPledge, int64& nMaxPledge) override;
    virtual uint32 CalcSingleBlockDistributePledgeRewardTxCount() override;

protected:
    bool HandleInitialize() override;
    Errno Debug(const Errno& err, const char* pszFunc, const char* pszFormat, ...);
    Errno VerifyDexOrderTx(const CTransaction& tx, const CDestination& destIn, int64 nValueIn, int nHeight);
    Errno VerifyDexMatchTx(const CTransaction& tx, int64 nValueIn, int nHeight);
    Errno VerifyMintPledgeTx(const CTransaction& tx);

protected:
    uint256 hashGenesisBlock;
    uint256 nProofOfWorkLowerLimit;
    uint256 nProofOfWorkUpperLimit;
    uint256 nProofOfWorkInit;
    uint32 nProofOfWorkDifficultyInterval;
    IBlockChain* pBlockChain;
};

class CTestNetCoreProtocol : public CCoreProtocol
{
public:
    CTestNetCoreProtocol();
    void GetGenesisBlock(CBlock& block) override;
};

class CProofOfWorkParam
{
public:
    CProofOfWorkParam(bool fTestnet);

public:
    uint256 hashGenesisBlock;
    uint256 nProofOfWorkLowerLimit;
    uint256 nProofOfWorkUpperLimit;
    uint256 nProofOfWorkInit;
    uint32 nProofOfWorkDifficultyInterval;
    int64 nDelegateProofOfStakeEnrollMinimumAmount;
    int64 nDelegateProofOfStakeEnrollMaximumAmount;
    uint32 nDelegateProofOfStakeHeight;
};

} // namespace minemon

#endif //MINEMON_BASE_H
