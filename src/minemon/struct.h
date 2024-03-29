/// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_STRUCT_H
#define MINEMON_STRUCT_H

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <map>
#include <set>
#include <vector>

#include "block.h"
#include "proto.h"
#include "template/template.h"
#include "transaction.h"
#include "uint256.h"

namespace minemon
{

inline int64 CalcMinTxFee(const CTransaction& tx, const int nHeight, const uint32 MIN_TX_FEE)
{
    //if (nHeight < NEW_CALC_DATA_HEIGTH)
    //{
    //    return (tx.vchSig.size() + tx.vchData.size()) * 100;
    //}
    //else
    //{
    int64 nVchData = tx.vchData.size();
    if (0 == nVchData)
    {
        return MIN_TX_FEE;
    }
    uint32_t multiplier = nVchData / 200;
    if (nVchData % 200 > 0)
    {
        multiplier++;
    }
    if (multiplier > 5)
    {
        return MIN_TX_FEE + MIN_TX_FEE * 10 + (multiplier - 5) * MIN_TX_FEE * 4;
    }
    else
    {
        return MIN_TX_FEE + multiplier * MIN_TX_FEE * 2;
    }
    //}
}

// Status
class CForkStatus
{
public:
    CForkStatus() {}
    CForkStatus(const uint256& hashOriginIn, const uint256& hashParentIn, int nOriginHeightIn)
      : hashOrigin(hashOriginIn),
        hashParent(hashParentIn),
        nOriginHeight(nOriginHeightIn),
        nLastBlockTime(0),
        nLastBlockHeight(-1),
        nMoneySupply(0),
        nMintType(-1)
    {
    }

    bool IsNull()
    {
        return nLastBlockTime == 0;
    }

public:
    uint256 hashOrigin;
    uint256 hashParent;
    int nOriginHeight;

    uint256 hashLastBlock;
    int64 nLastBlockTime;
    int nLastBlockHeight;
    int64 nMoneySupply;
    uint16 nMintType;
    std::multimap<int, uint256> mapSubline;
};

class CWalletBalance
{
public:
    int64 nAvailable;
    int64 nLocked;
    int64 nUnconfirmed;

public:
    void SetNull()
    {
        nAvailable = 0;
        nLocked = 0;
        nUnconfirmed = 0;
    }
};

// Notify
class CBlockChainUpdate
{
public:
    CBlockChainUpdate()
    {
        SetNull();
    }
    CBlockChainUpdate(CBlockIndex* pIndex)
    {
        hashFork = pIndex->GetOriginHash();
        hashParent = pIndex->GetParentHash();
        nOriginHeight = pIndex->pOrigin->GetBlockHeight() - 1;
        hashLastBlock = pIndex->GetBlockHash();
        nLastBlockTime = pIndex->GetBlockTime();
        nLastBlockHeight = pIndex->GetBlockHeight();
        nLastMintType = pIndex->nMintType;
        nMoneySupply = pIndex->GetMoneySupply();
    }
    void SetNull()
    {
        hashFork = 0;
        nOriginHeight = -1;
        nLastBlockHeight = -1;
        nLastMintType = 0;
    }
    bool IsNull() const
    {
        return (hashFork == 0);
    }

public:
    uint256 hashFork;
    uint256 hashParent;
    int nOriginHeight;
    uint256 hashLastBlock;
    int64 nLastBlockTime;
    int nLastBlockHeight;
    uint16 nLastMintType;
    int64 nMoneySupply;
    std::set<uint256> setTxUpdate;
    std::vector<CBlockEx> vBlockAddNew;
    std::vector<CBlockEx> vBlockRemove;
};

class CTxSetChange
{
public:
    uint256 hashFork;
    std::map<uint256, int> mapTxUpdate;
    std::vector<CAssembledTx> vTxAddNew;
    std::vector<std::pair<uint256, std::vector<CTxIn>>> vTxRemove;
};

class CNetworkPeerUpdate
{
public:
    bool fActive;
    uint64 nPeerNonce;
    network::CAddress addrPeer;
};

class CTransactionUpdate
{
public:
    CTransactionUpdate()
    {
        SetNull();
    }
    void SetNull()
    {
        hashFork = 0;
    }
    bool IsNull() const
    {
        return (hashFork == 0);
    }

public:
    uint256 hashFork;
    int64 nChange;
    CTransaction txUpdate;
};

/* Protocol & Event */
class CNil
{
    friend class xengine::CStream;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
    }
};

class CBlockMakerUpdate
{
public:
    uint256 hashParent;
    int nOriginHeight;

    uint256 hashBlock;
    int64 nBlockTime;
    int nBlockHeight;
    uint256 nAgreement;
    //std::size_t nWeight;
    uint16 nMintType;
};

/* Net Channel */
class CPeerKnownTx
{
public:
    CPeerKnownTx() {}
    CPeerKnownTx(const uint256& txidIn)
      : txid(txidIn), nTime(xengine::GetTime()) {}

public:
    uint256 txid;
    int64 nTime;
};

typedef boost::multi_index_container<
    CPeerKnownTx,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CPeerKnownTx, uint256, &CPeerKnownTx::txid>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CPeerKnownTx, int64, &CPeerKnownTx::nTime>>>>
    CPeerKnownTxSet;

typedef CPeerKnownTxSet::nth_index<0>::type CPeerKnownTxSetById;
typedef CPeerKnownTxSet::nth_index<1>::type CPeerKnownTxSetByTime;

typedef boost::multi_index_container<
    uint256,
    boost::multi_index::indexed_by<
        boost::multi_index::sequenced<>,
        boost::multi_index::ordered_unique<boost::multi_index::identity<uint256>>>>
    CUInt256List;
typedef CUInt256List::nth_index<1>::type CUInt256ByValue;

/* CStatItemBlockMaker & CStatItemP2pSyn */
class CStatItemBlockMaker
{
public:
    CStatItemBlockMaker()
      : nTimeValue(0), nPOWBlockCount(0), nDPOSBlockCount(0), nBlockTPS(0), nTxTPS(0) {}

    uint32 nTimeValue;

    uint64 nPOWBlockCount;
    uint64 nDPOSBlockCount;
    uint64 nBlockTPS;
    uint64 nTxTPS;
};

class CStatItemP2pSyn
{
public:
    CStatItemP2pSyn()
      : nRecvBlockCount(0), nRecvTxTPS(0), nSendBlockCount(0), nSendTxTPS(0), nSynRecvTxTPS(0), nSynSendTxTPS(0) {}

    uint32 nTimeValue;

    uint64 nRecvBlockCount;
    uint64 nRecvTxTPS;
    uint64 nSendBlockCount;
    uint64 nSendTxTPS;

    uint64 nSynRecvTxTPS;
    uint64 nSynSendTxTPS;
};

} // namespace minemon

#endif // MINEMON_STRUCT_H
