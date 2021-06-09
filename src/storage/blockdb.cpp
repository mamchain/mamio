// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockdb.h"

#include "stream/datastream.h"

using namespace std;

namespace minemon
{
namespace storage
{

//////////////////////////////
// CBlockDB

CBlockDB::CBlockDB()
{
}

CBlockDB::~CBlockDB()
{
}

bool CBlockDB::Initialize(const boost::filesystem::path& pathData)
{
    if (!dbFork.Initialize(pathData))
    {
        return false;
    }

    if (!dbBlockIndex.Initialize(pathData))
    {
        return false;
    }

    if (!dbTxIndex.Initialize(pathData))
    {
        return false;
    }

    if (!dbUnspent.Initialize(pathData))
    {
        return false;
    }

    if (!dbTemplateData.Initialize(pathData))
    {
        return false;
    }

    if (!dbPledge.Initialize(pathData))
    {
        return false;
    }

    if (!dbRedeem.Initialize(pathData))
    {
        return false;
    }

    return LoadFork();
}

void CBlockDB::Deinitialize()
{
    dbRedeem.Deinitialize();
    dbPledge.Deinitialize();
    dbTemplateData.Deinitialize();
    dbUnspent.Deinitialize();
    dbTxIndex.Deinitialize();
    dbBlockIndex.Deinitialize();
    dbFork.Deinitialize();
}

bool CBlockDB::RemoveAll()
{
    dbRedeem.Clear();
    dbPledge.Clear();
    dbTemplateData.Clear();
    dbUnspent.Clear();
    dbTxIndex.Clear();
    dbBlockIndex.Clear();
    dbFork.Clear();

    return true;
}

bool CBlockDB::AddNewForkContext(const CForkContext& ctxt)
{
    return dbFork.AddNewForkContext(ctxt);
}

bool CBlockDB::RetrieveForkContext(const uint256& hash, CForkContext& ctxt)
{
    ctxt.SetNull();
    return dbFork.RetrieveForkContext(hash, ctxt);
}

bool CBlockDB::ListForkContext(vector<CForkContext>& vForkCtxt)
{
    vForkCtxt.clear();
    return dbFork.ListForkContext(vForkCtxt);
}

bool CBlockDB::AddNewFork(const uint256& hash)
{
    if (!dbFork.UpdateFork(hash))
    {
        return false;
    }

    if (!dbTxIndex.AddNewFork(hash))
    {
        RemoveFork(hash);
        return false;
    }

    if (!dbUnspent.AddNewFork(hash))
    {
        RemoveFork(hash);
        return false;
    }

    return true;
}

bool CBlockDB::RemoveFork(const uint256& hash)
{
    dbTxIndex.RemoveFork(hash);
    dbUnspent.RemoveFork(hash);
    return dbFork.RemoveFork(hash);
}

bool CBlockDB::ListFork(vector<pair<uint256, uint256>>& vFork)
{
    vFork.clear();
    return dbFork.ListFork(vFork);
}

bool CBlockDB::UpdateFork(const uint256& hash, const uint256& hashRefBlock, const uint256& hashForkBased,
                          const vector<pair<uint256, CTxIndex>>& vTxNew, const vector<uint256>& vTxDel,
                          const vector<pair<CAddrTxIndex, CAddrTxInfo>>& vAddrTxNew, const vector<CAddrTxIndex>& vAddrTxDel,
                          const vector<CTxUnspent>& vAddNewUnspent, const vector<CTxUnspent>& vRemoveUnspent)
{
    if (!dbUnspent.Exists(hash))
    {
        return false;
    }

    bool fIgnoreTxDel = false;
    if (hashForkBased != hash && hashForkBased != 0)
    {
        if (!dbUnspent.Copy(hashForkBased, hash))
        {
            return false;
        }
        fIgnoreTxDel = true;
    }

    if (!dbFork.UpdateFork(hash, hashRefBlock))
    {
        return false;
    }

    if (!dbTxIndex.Update(hash, vTxNew, fIgnoreTxDel ? vector<uint256>() : vTxDel))
    {
        return false;
    }

    if (!dbUnspent.Update(hash, vAddNewUnspent, vRemoveUnspent))
    {
        return false;
    }

    return true;
}

bool CBlockDB::AddNewBlock(const CBlockOutline& outline)
{
    return dbBlockIndex.AddNewBlock(outline);
}

bool CBlockDB::RemoveBlock(const uint256& hash)
{
    return dbBlockIndex.RemoveBlock(hash);
}

bool CBlockDB::UpdatePledgeContext(const uint256& hash, const CPledgeContext& ctxtPledge)
{
    return dbPledge.AddNew(hash, ctxtPledge);
}

bool CBlockDB::WalkThroughBlock(CBlockDBWalker& walker)
{
    return dbBlockIndex.WalkThroughBlock(walker);
}

bool CBlockDB::RetrieveTxIndex(const uint256& txid, CTxIndex& txIndex, uint256& fork)
{
    txIndex.SetNull();
    return dbTxIndex.Retrieve(txid, txIndex, fork);
}

bool CBlockDB::RetrieveTxIndex(const uint256& fork, const uint256& txid, CTxIndex& txIndex)
{
    txIndex.SetNull();
    return dbTxIndex.Retrieve(fork, txid, txIndex);
}

bool CBlockDB::RetrieveTxUnspent(const uint256& fork, const CTxOutPoint& out, CTxOut& unspent)
{
    return dbUnspent.Retrieve(fork, out, unspent);
}

bool CBlockDB::WalkThroughUnspent(const uint256& hashFork, CForkUnspentDBWalker& walker)
{
    return dbUnspent.WalkThrough(hashFork, walker);
}

bool CBlockDB::RetrievePledge(const uint256& hash, CPledgeContext& ctxtPledge)
{
    return dbPledge.Retrieve(hash, ctxtPledge);
}

bool CBlockDB::UpdateTemplateData(const CDestination& dest, const vector<uint8>& vTemplateData)
{
    return dbTemplateData.AddNew(dest, vTemplateData);
}

bool CBlockDB::RetrieveTemplateData(const CDestination& dest, vector<uint8>& vTemplateData)
{
    return dbTemplateData.Retrieve(dest, vTemplateData);
}

bool CBlockDB::UpdateRedeemData(const uint256& hashBlock, const CRedeemContext& redeemData)
{
    return dbRedeem.AddNew(hashBlock, redeemData);
}

bool CBlockDB::RetrieveRedeemData(const uint256& hashBlock, CRedeemContext& redeemData)
{
    return dbRedeem.Retrieve(hashBlock, redeemData);
}

bool CBlockDB::LoadFork()
{
    vector<pair<uint256, uint256>> vFork;
    if (!dbFork.ListFork(vFork))
    {
        return false;
    }

    for (int i = 0; i < vFork.size(); i++)
    {
        if (!dbTxIndex.LoadFork(vFork[i].first))
        {
            return false;
        }

        if (!dbUnspent.LoadFork(vFork[i].first))
        {
            return false;
        }
    }
    return true;
}

} // namespace storage
} // namespace minemon
