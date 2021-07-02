// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_BLOCKDB_H
#define STORAGE_BLOCKDB_H

#include "block.h"
#include "blockindexdb.h"
#include "forkcontext.h"
#include "forkdb.h"
#include "pledgedb.h"
#include "redeemdb.h"
#include "templatedatadb.h"
#include "transaction.h"
#include "txindexdb.h"
#include "unspentdb.h"

namespace minemon
{
namespace storage
{

class CBlockDB
{
public:
    CBlockDB();
    ~CBlockDB();
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool RemoveAll();
    bool AddNewForkContext(const CForkContext& ctxt);
    bool RetrieveForkContext(const uint256& hash, CForkContext& ctxt);
    bool ListForkContext(std::vector<CForkContext>& vForkCtxt);
    bool AddNewFork(const uint256& hash);
    bool RemoveFork(const uint256& hash);
    bool ListFork(std::vector<std::pair<uint256, uint256>>& vFork);
    bool UpdateFork(const uint256& hash, const uint256& hashRefBlock, const uint256& hashForkBased,
                    const std::vector<std::pair<uint256, CTxIndex>>& vTxNew, const std::vector<uint256>& vTxDel,
                    const std::vector<std::pair<CAddrTxIndex, CAddrTxInfo>>& vAddrTxNew, const std::vector<CAddrTxIndex>& vAddrTxDel,
                    const std::vector<CTxUnspent>& vAddNewUnspent, const std::vector<CTxUnspent>& vRemoveUnspent);
    bool AddNewBlock(const CBlockOutline& outline);
    bool RemoveBlock(const uint256& hash);
    bool UpdatePledgeContext(const uint256& hash, const CPledgeContext& ctxtPledge);
    bool WalkThroughBlock(CBlockDBWalker& walker);
    bool RetrieveTxIndex(const uint256& txid, CTxIndex& txIndex, uint256& fork);
    bool RetrieveTxIndex(const uint256& fork, const uint256& txid, CTxIndex& txIndex);
    bool RetrieveTxUnspent(const uint256& fork, const CTxOutPoint& out, CTxOut& unspent);
    bool WalkThroughUnspent(const uint256& hashFork, CForkUnspentDBWalker& walker);
    bool AddBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::map<CDestination, std::pair<int64, int>>>& mapBlockPledgeIn);
    bool RetrievePowPledgeList(const uint256& hashBlock, const CDestination& destPowMint, std::map<CDestination, std::pair<int64, int>>& mapPowPledgeList);
    bool RetrieveAddressPledgeData(const uint256& hashBlock, const CDestination& destPowMint, const CDestination& destPledge, int64& nPledgeAmount, int& nPledgeHeight);
    bool UpdateTemplateData(const CDestination& dest, const std::vector<uint8>& vTemplateData);
    bool RetrieveTemplateData(const CDestination& dest, std::vector<uint8>& vTemplateData);
    bool UpdateRedeemData(const uint256& hashBlock, const CRedeemContext& redeemData);
    bool RetrieveAddressRedeem(const uint256& hashBlock, const CDestination& dest, CDestRedeem& destRedeem);
    bool AddBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn);

protected:
    bool LoadFork();

protected:
    CForkDB dbFork;
    CBlockIndexDB dbBlockIndex;
    CTxIndexDB dbTxIndex;
    CUnspentDB dbUnspent;
    CTemplateDataDB dbTemplateData;
    CPledgeDB dbPledge;
    CRedeemDB dbRedeem;
};

} // namespace storage
} // namespace minemon

#endif //STORAGE_BLOCKDB_H
