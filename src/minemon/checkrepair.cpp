// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkrepair.h"

#include "template/mintpledge.h"

using namespace std;
using namespace xengine;
using namespace boost::filesystem;

#define BLOCKFILE_PREFIX "block"

namespace minemon
{

/////////////////////////////////////////////////////////////////////////
// CCheckForkUnspentWalker

bool CCheckForkUnspentWalker::Walk(const CTxOutPoint& txout, const CTxOut& output)
{
    if (!output.IsNull())
    {
        if (!mapForkUnspent.insert(make_pair(txout, CCheckTxOut(output))).second)
        {
            StdError("Check", "Insert leveldb unspent fail, unspent: [%d] %s.", txout.n, txout.hash.GetHex().c_str());
            return false;
        }
    }
    else
    {
        StdLog("Check", "Leveldb unspent is NULL, unspent: [%d] %s.", txout.n, txout.hash.GetHex().c_str());
    }
    return true;
}

bool CCheckForkUnspentWalker::CheckForkUnspent(map<CTxOutPoint, CCheckTxOut>& mapBlockForkUnspent)
{
    {
        map<CTxOutPoint, CCheckTxOut>::iterator it = mapBlockForkUnspent.begin();
        for (; it != mapBlockForkUnspent.end(); ++it)
        {
            map<CTxOutPoint, CCheckTxOut>::iterator mt = mapForkUnspent.find(it->first);
            if (mt == mapForkUnspent.end())
            {
                StdLog("Check", "Check block unspent: find utxo fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                mapForkUnspent.insert(*it);
                vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
            }
            else if (it->second != mt->second)
            {
                StdLog("Check", "Check block unspent: txout error, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                mt->second = it->second;
                vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
            }
        }
    }

    {
        map<CTxOutPoint, CCheckTxOut>::iterator it = mapForkUnspent.begin();
        for (; it != mapForkUnspent.end();)
        {
            map<CTxOutPoint, CCheckTxOut>::iterator mt = mapBlockForkUnspent.find(it->first);
            if (mt == mapBlockForkUnspent.end())
            {
                StdLog("Check", "Check db unspent: find utxo fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                vRemove.push_back(it->first);
                mapForkUnspent.erase(it++);
                continue;
            }
            else if (it->second != mt->second)
            {
                StdLog("Check", "Check db unspent: txout error, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                it->second = mt->second;
                vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
            }
            ++it;
        }
    }
    return !(vAddUpdate.size() > 0 || vRemove.size() > 0);
}

/////////////////////////////////////////////////////////////////////////
// CCheckForkManager

bool CCheckForkManager::FetchForkStatus(const string& strDataPath)
{
    CForkDB dbFork;
    if (!dbFork.Initialize(path(strDataPath)))
    {
        StdError("Check", "Fetch fork status: Initialize fail");
        return false;
    }

    vector<pair<uint256, uint256>> vFork;
    if (!dbFork.ListFork(vFork))
    {
        StdError("Check", "Fetch fork status: ListFork fail");
        dbFork.Deinitialize();
        return false;
    }

    for (const auto& fork : vFork)
    {
        const uint256 hashFork = fork.first;
        if (hashFork != objProofParam.hashGenesisBlock)
        {
            continue;
        }
        CCheckForkStatus& status = mapForkStatus[hashFork];
        status.hashLastBlock = fork.second;

        if (!dbFork.RetrieveForkContext(hashFork, status.ctxt))
        {
            StdError("Check", "Fetch fork status: RetrieveForkContext fail");
            dbFork.Deinitialize();
            return false;
        }
    }

    dbFork.Deinitialize();
    return true;
}

void CCheckForkManager::GetTxFork(const uint256& hashFork, int nHeight, vector<uint256>& vFork)
{
    if (hashFork == objProofParam.hashGenesisBlock)
    {
        vFork.push_back(objProofParam.hashGenesisBlock);
    }
}

bool CCheckForkManager::UpdateForkLast(const string& strDataPath, const vector<pair<uint256, uint256>>& vForkLast)
{
    CForkDB dbFork;
    if (!dbFork.Initialize(path(strDataPath)))
    {
        StdError("Check", "Update fork last: Initialize fail");
        return false;
    }
    for (const auto& data : vForkLast)
    {
        if (!dbFork.UpdateFork(data.first, data.second))
        {
            StdError("Check", "Update fork last: update fail, fork: %s, last: %s",
                     data.first.GetHex().c_str(), data.second.GetHex().c_str());
            dbFork.Deinitialize();
            return false;
        }
    }
    dbFork.Deinitialize();
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckWalletForkUnspent

bool CCheckWalletForkUnspent::LocalTxExist(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        StdLog("Check", "Wallet fork LocalTxExist: find tx fail, txid: %s", txid.GetHex().c_str());
        return false;
    }
    if (it->second.hashFork != hashFork)
    {
        StdLog("Check", "Wallet fork LocalTxExist: fork error, txid: %s, wtx fork: %s, wallet fork: %s",
               txid.GetHex().c_str(), it->second.hashFork.GetHex().c_str(), hashFork.GetHex().c_str());
        return false;
    }
    return true;
}

CCheckWalletTx* CCheckWalletForkUnspent::GetLocalWalletTx(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        StdLog("Check", "Wallet fork GetLocalWalletTx: find tx fail, txid: %s, forkid: %s", txid.GetHex().c_str(), hashFork.GetHex().c_str());
        return nullptr;
    }
    if (it->second.hashFork != hashFork)
    {
        StdLog("Check", "Wallet fork GetLocalWalletTx: fork error, txid: %s, wtx fork: %s, wallet fork: %s",
               txid.GetHex().c_str(), it->second.hashFork.GetHex().c_str(), hashFork.GetHex().c_str());
        return nullptr;
    }
    return &(it->second);
}

bool CCheckWalletForkUnspent::AddTx(const CWalletTx& wtx)
{
    if (mapWalletTx.find(wtx.txid) != mapWalletTx.end())
    {
        StdDebug("Check", "Add wallet tx: tx is exist, wtx txid: %s, wtx at forkid: %s, forkid: %s",
                 wtx.txid.GetHex().c_str(), wtx.hashFork.GetHex().c_str(), hashFork.GetHex().c_str());
        return true;
    }
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.insert(make_pair(wtx.txid, CCheckWalletTx(wtx, ++nSeqCreate))).first;
    if (it == mapWalletTx.end())
    {
        StdLog("Check", "Add wallet tx: add fail, txid: %s", wtx.txid.GetHex().c_str());
        return false;
    }
    CWalletTxLinkSetByTxHash& idxTx = setWalletTxLink.get<0>();
    if (idxTx.find(wtx.txid) != idxTx.end())
    {
        setWalletTxLink.erase(wtx.txid);
    }
    if (!setWalletTxLink.insert(CWalletTxLink(&(it->second))).second)
    {
        StdLog("Check", "Add wallet tx: add link fail, txid: %s", wtx.txid.GetHex().c_str());
        return false;
    }
    return true;
}

void CCheckWalletForkUnspent::RemoveTx(const uint256& txid)
{
    mapWalletTx.erase(txid);
    setWalletTxLink.erase(txid);
}

bool CCheckWalletForkUnspent::UpdateUnspent()
{
    CWalletTxLinkSetBySequenceNumber& idxTx = setWalletTxLink.get<1>();
    {
        CWalletTxLinkSetBySequenceNumber::iterator it = idxTx.begin();
        for (; it != idxTx.end(); ++it)
        {
            const CCheckWalletTx& wtx = *(it->ptx);
            if (wtx.IsMine())
            {
                if (!AddWalletUnspent(CTxOutPoint(wtx.txid, 0), wtx.GetOutput(0)))
                {
                    StdError("Check", "Wallet update unspent: add unspent 0 fail");
                    return false;
                }
            }
            if (wtx.IsFromMe())
            {
                if (!AddWalletUnspent(CTxOutPoint(wtx.txid, 1), wtx.GetOutput(1)))
                {
                    StdError("Check", "Wallet update unspent: add unspent 1 fail");
                    return false;
                }
            }
        }
    }
    {
        CWalletTxLinkSetBySequenceNumber::iterator it = idxTx.begin();
        for (; it != idxTx.end(); ++it)
        {
            const CCheckWalletTx& wtx = *(it->ptx);
            if (wtx.IsFromMe())
            {
                for (int n = 0; n < wtx.vInput.size(); n++)
                {
                    if (!AddWalletSpent(wtx.vInput[n].prevout, wtx.txid, wtx.sendTo))
                    {
                        StdError("Check", "Wallet update unspent: spent fail");
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool CCheckWalletForkUnspent::AddWalletSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapWalletUnspent.find(txPoint);
    if (it == mapWalletUnspent.end())
    {
        StdError("Check", "AddWalletSpent find fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
        return false;
    }
    mapWalletUnspent.erase(it);
    return true;
}

bool CCheckWalletForkUnspent::AddWalletUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut)
{
    if (!txOut.IsNull())
    {
        if (!mapWalletUnspent.insert(make_pair(txPoint, CCheckTxOut(txOut))).second)
        {
            StdError("Check", "insert wallet unspent fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

int CCheckWalletForkUnspent::GetTxAtBlockHeight(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        return -1;
    }
    return it->second.nBlockHeight;
}

bool CCheckWalletForkUnspent::CheckWalletUnspent(const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<CTxOutPoint, CCheckTxOut>::iterator mt = mapWalletUnspent.find(point);
    if (mt == mapWalletUnspent.end())
    {
        StdLog("Check", "CheckWalletUnspent: find unspent fail, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
        return false;
    }
    if (mt->second != out)
    {
        StdLog("Check", "CheckWalletUnspent: out error, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckWalletTxWalker

bool CCheckWalletTxWalker::Walk(const CWalletTx& wtx)
{
    if (wtx.hashFork == 0)
    {
        StdError("Check", "Wallet Walk: fork is 0.");
        return false;
    }
    return AddWalletTx(wtx);
}

bool CCheckWalletTxWalker::Exist(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("Check", "Wallet Exist: hashFork is 0.");
        return false;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("Check", "Wallet Exist: hashFork find fail, fork: %s.", hashFork.GetHex().c_str());
        return false;
    }
    return it->second.LocalTxExist(txid);
}

CCheckWalletTx* CCheckWalletTxWalker::GetWalletTx(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("Check", "Wallet get tx: hashFork is 0.");
        return nullptr;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("Check", "Wallet get tx: hashFork find fail, fork: %s.", hashFork.GetHex().c_str());
        return nullptr;
    }
    return it->second.GetLocalWalletTx(txid);
}

bool CCheckWalletTxWalker::AddWalletTx(const CWalletTx& wtx)
{
    if (pForkManager == nullptr)
    {
        StdError("Check", "Wallet add tx: pForkManager is null.");
        return false;
    }
    if (wtx.hashFork == 0)
    {
        StdError("Check", "Wallet add tx: wtx fork is 0.");
        return false;
    }

    vector<uint256> vFork;
    pForkManager->GetTxFork(wtx.hashFork, wtx.nBlockHeight, vFork);
    for (const uint256& hashFork : vFork)
    {
        map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
        if (it == mapWalletFork.end())
        {
            it = mapWalletFork.insert(make_pair(hashFork, CCheckWalletForkUnspent(hashFork))).first;
            if (it == mapWalletFork.end())
            {
                StdError("Check", "Wallet add tx: add fork fail.");
                return false;
            }
        }
        if (!it->second.AddTx(wtx))
        {
            StdError("Check", "Wallet add tx: add fail.");
            return false;
        }
    }

    nWalletTxCount++;
    return true;
}

void CCheckWalletTxWalker::RemoveWalletTx(const uint256& hashFork, int nHeight, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("Check", "Wallet remove tx: hashFork is 0.");
        return;
    }

    vector<uint256> vFork;
    pForkManager->GetTxFork(hashFork, nHeight, vFork);
    for (const uint256& hash : vFork)
    {
        map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hash);
        if (it == mapWalletFork.end())
        {
            StdError("Check", "Wallet remove tx: fork find fail, fork: %s.", hash.GetHex().c_str());
        }
        else
        {
            it->second.RemoveTx(txid);
        }
    }

    nWalletTxCount--;
}

bool CCheckWalletTxWalker::UpdateUnspent()
{
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.begin();
    for (; it != mapWalletFork.end(); ++it)
    {
        if (!it->second.UpdateUnspent())
        {
            StdError("Check", "Wallet update unspent fail, fork: %s.", it->first.GetHex().c_str());
            return false;
        }
    }
    return true;
}

int CCheckWalletTxWalker::GetTxAtBlockHeight(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("Check", "Wallet get block height: hashFork is 0.");
        return -1;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("Check", "Wallet get block height: fork find fail, fork: %s.", hashFork.GetHex().c_str());
        return -1;
    }
    return it->second.GetTxAtBlockHeight(txid);
}

bool CCheckWalletTxWalker::CheckWalletUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("Check", "Wallet check unspent: fork find fail, fork: %s.", hashFork.GetHex().c_str());
        return false;
    }
    return it->second.CheckWalletUnspent(point, out);
}

/////////////////////////////////////////////////////////////////////////
// CCheckForkTxPool

bool CCheckForkTxPool::AddTx(const uint256& txid, const CAssembledTx& tx)
{
    vTx.push_back(make_pair(txid, tx));

    if (!mapTxPoolTx.insert(make_pair(txid, tx)).second)
    {
        StdError("Check", "TxPool AddTx: Add tx failed, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    for (int i = 0; i < tx.vInput.size(); i++)
    {
        if (!Spent(tx.vInput[i].prevout, txid, tx.sendTo))
        {
            StdError("Check", "TxPool AddTx: Spent fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    if (!Unspent(CTxOutPoint(txid, 0), tx.GetOutput(0)))
    {
        StdError("Check", "TxPool AddTx: Add unspent 0 fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    if (!tx.IsMintTx())
    {
        if (!Unspent(CTxOutPoint(txid, 1), tx.GetOutput(1)))
        {
            StdError("Check", "TxPool AddTx: Add unspent 1 fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckForkTxPool::Spent(const CTxOutPoint& point, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapTxPoolUnspent.find(point);
    if (it == mapTxPoolUnspent.end())
    {
        StdError("Check", "TxPool Spent: find fail, utxo: [%d] %s", point.n, point.hash.GetHex().c_str());
        return false;
    }
    mapTxPoolUnspent.erase(it);
    return true;
}

bool CCheckForkTxPool::Unspent(const CTxOutPoint& point, const CTxOut& out)
{
    if (!out.IsNull())
    {
        if (!mapTxPoolUnspent.insert(make_pair(point, CCheckTxOut(out))).second)
        {
            StdError("Check", "TxPool Unspent: insert unspent fail, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckForkTxPool::CheckTxExist(const uint256& txid)
{
    return (mapTxPoolTx.find(txid) != mapTxPoolTx.end());
}

bool CCheckForkTxPool::CheckTxPoolUnspent(const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapTxPoolUnspent.find(point);
    if (it == mapTxPoolUnspent.end())
    {
        StdLog("Check", "TxPool CheckTxPoolUnspent: find fail, utxo: [%d] %s", point.n, point.hash.GetHex().c_str());
        return false;
    }
    if (it->second != out)
    {
        StdLog("Check", "TxPool CheckTxPoolUnspent: out error, utxo: [%d] %s", point.n, point.hash.GetHex().c_str());
        return false;
    }
    return true;
}

bool CCheckForkTxPool::GetWalletTx(const uint256& hashFork, const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    for (int i = 0; i < vTx.size(); i++)
    {
        const CAssembledTx& atx = vTx[i].second;
        bool fIsMine = (setAddress.find(atx.sendTo) != setAddress.end());
        bool fFromMe = (setAddress.find(atx.destIn) != setAddress.end());
        if (fIsMine || fFromMe)
        {
            CWalletTx wtx(vTx[i].first, atx, hashFork, fIsMine, fFromMe);
            vWalletTx.push_back(wtx);
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckTxPoolData

void CCheckTxPoolData::AddForkUnspent(const uint256& hashFork, const map<CTxOutPoint, CCheckTxOut>& mapUnspent)
{
    if (hashFork != 0)
    {
        mapForkTxPool[hashFork].mapTxPoolUnspent = mapUnspent;
    }
}

bool CCheckTxPoolData::FetchTxPool(const string& strPath)
{
    CTxPoolData datTxPool;
    if (!datTxPool.Initialize(path(strPath)))
    {
        StdLog("Check", "TxPool: Failed to initialize txpool data");
        return false;
    }

    vector<pair<uint256, pair<uint256, CAssembledTx>>> vTx;
    if (!datTxPool.LoadCheck(vTx))
    {
        StdLog("Check", "TxPool: Load txpool data failed");
        return false;
    }

    for (int i = 0; i < vTx.size(); i++)
    {
        if (vTx[i].first == 0)
        {
            StdError("Check", "TxPool: tx fork hash is 0, txid: %s", vTx[i].second.first.GetHex().c_str());
            return false;
        }
        if (!mapForkTxPool[vTx[i].first].AddTx(vTx[i].second.first, vTx[i].second.second))
        {
            StdError("Check", "TxPool: Add tx fail, txid: %s", vTx[i].second.first.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckTxPoolData::CheckTxExist(const uint256& hashFork, const uint256& txid)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.find(hashFork);
    if (it == mapForkTxPool.end())
    {
        return false;
    }
    return it->second.CheckTxExist(txid);
}

bool CCheckTxPoolData::CheckTxPoolUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.find(hashFork);
    if (it == mapForkTxPool.end())
    {
        return false;
    }
    return it->second.CheckTxPoolUnspent(point, out);
}

bool CCheckTxPoolData::GetTxPoolWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.begin();
    for (; it != mapForkTxPool.end(); ++it)
    {
        if (!it->second.GetWalletTx(it->first, setAddress, vWalletTx))
        {
            return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckBlockFork

void CCheckBlockFork::UpdateMaxTrust(CBlockIndex* pBlockIndex)
{
    if (pBlockIndex->IsOrigin())
    {
        if (pOrigin != nullptr)
        {
            StdLog("Check", "CCheckBlockFork pOrigin is not NULL");
            return;
        }
        pOrigin = pBlockIndex;
    }
    if (pLast == nullptr
        || !(pLast->nChainTrust > pBlockIndex->nChainTrust
             || (pLast->nChainTrust == pBlockIndex->nChainTrust && !pBlockIndex->IsEquivalent(pLast))))
    {
        pLast = pBlockIndex;
    }
}

bool CCheckBlockFork::AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, const uint256& hashAtForkIn, uint32 nFileNoIn, uint32 nOffsetIn)
{
    const uint256 txid = txIn.GetHash();
    map<uint256, CCheckBlockTx>::iterator mt = mapBlockTx.insert(make_pair(txid, CCheckBlockTx(txIn, contxtIn, nHeight, hashAtForkIn, nFileNoIn, nOffsetIn))).first;
    if (mt == mapBlockTx.end())
    {
        StdLog("Check", "AddBlockTx: add block tx fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    for (int i = 0; i < txIn.vInput.size(); i++)
    {
        if (!AddBlockSpent(txIn.vInput[i].prevout, txid, txIn.sendTo))
        {
            StdLog("Check", "AddBlockTx: block spent fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    if (!AddBlockUnspent(CTxOutPoint(txid, 0), CTxOut(txIn)))
    {
        StdLog("Check", "AddBlockTx: add block unspent 0 fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    if (!txIn.IsMintTx())
    {
        if (!AddBlockUnspent(CTxOutPoint(txid, 1), CTxOut(txIn, contxtIn.destIn, contxtIn.GetValueIn())))
        {
            StdLog("Check", "AddBlockTx: add block unspent 1 fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckBlockFork::AddBlockSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapBlockUnspent.find(txPoint);
    if (it == mapBlockUnspent.end())
    {
        StdLog("Check", "AddBlockSpent: utxo find fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
        return false;
    }
    mapBlockUnspent.erase(it);
    return true;
}

bool CCheckBlockFork::AddBlockUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut)
{
    if (!txOut.IsNull())
    {
        if (!mapBlockUnspent.insert(make_pair(txPoint, CCheckTxOut(txOut))).second)
        {
            StdLog("Check", "AddBlockUnspent: Add block unspent fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckBlockFork::CheckTxExist(const uint256& txid, int& nHeight)
{
    map<uint256, CCheckBlockTx>::iterator it = mapBlockTx.find(txid);
    if (it == mapBlockTx.end())
    {
        StdLog("Check", "CheckBlockFork: Check tx exist, find tx fail, txid: %s", txid.GetHex().c_str());
        return false;
    }
    nHeight = it->second.txIndex.nBlockHeight;
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckBlockWalker

CCheckBlockWalker::~CCheckBlockWalker()
{
    ClearBlockIndex();
    dbBlockIndex.Deinitialize();
    dbTemplateData.Deinitialize();
    dbPledge.Deinitialize();
    dbRedeem.Deinitialize();
}

bool CCheckBlockWalker::Initialize(const string& strPath)
{
    if (!objTsBlock.Initialize(path(strPath) / "block", BLOCKFILE_PREFIX))
    {
        StdError("Check", "Block walker: TsBlock initialize fail, path: %s.", strPath.c_str());
        return false;
    }

    if (!dbBlockIndex.Initialize(path(strPath)))
    {
        StdLog("Check", "dbBlockIndex Initialize fail");
        return false;
    }
    dbBlockIndex.WalkThroughBlock(objBlockIndexWalker);
    StdLog("Check", "Fetch block index success, count: %ld", objBlockIndexWalker.mapBlockIndex.size());

    {
        if (!dbTemplateData.Initialize(path(strPath)))
        {
            StdLog("Check", "dbTemplateData Initialize fail");
            return false;
        }

        class CCheckTemplateWalker : public CTemplateDataDBWalker
        {
        public:
            CCheckTemplateWalker(map<CDestination, pair<vector<uint8>, int>>& mapIn)
              : mapTemplateDataLoad(mapIn)
            {
            }
            bool Walk(const CDestination& dest, const std::vector<uint8>& vTemplateData)
            {
                mapTemplateDataLoad.insert(make_pair(dest, make_pair(vTemplateData, 0)));
                return true;
            }

        public:
            map<CDestination, pair<vector<uint8>, int>>& mapTemplateDataLoad;
        };

        CCheckTemplateWalker templateWalker(mapTemplateData);
        if (!dbTemplateData.WalkThroughTemplateData(templateWalker))
        {
            StdLog("Check", "dbTemplateData WalkThroughTemplateData fail");
            return false;
        }
        StdLog("Check", "Fetch template data success, count: %ld", mapTemplateData.size());
    }

    if (!dbPledge.Initialize(path(strPath)))
    {
        StdLog("Check", "dbTemplateData Initialize fail");
        return false;
    }
    if (!dbRedeem.Initialize(path(strPath)))
    {
        StdLog("Check", "dbTemplateData Initialize fail");
        return false;
    }
    return true;
}

bool CCheckBlockWalker::Walk(const CBlockEx& block, uint32 nFile, uint32 nOffset)
{
    const uint256 hashBlock = block.GetHash();
    if (mapBlock.count(hashBlock) > 0)
    {
        StdError("Check", "Block walk: block exist, hash: %s.", hashBlock.GetHex().c_str());
        return true;
    }

    if (block.IsGenesis())
    {
        if (hashBlock != objProofParam.hashGenesisBlock)
        {
            StdError("Check", "Block walk: genesis block error, genesis: %s, block: %s.",
                     objProofParam.hashGenesisBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
            return false;
        }
        if (hashGenesis != 0)
        {
            StdError("Check", "Block walk: more genesis block, block hash: %s, hashGenesis: %s.",
                     hashBlock.GetHex().c_str(), hashGenesis.GetHex().c_str());
            return false;
        }
        hashGenesis = hashBlock;
    }
    else
    {
        if (mapBlock.empty() || hashGenesis == 0)
        {
            StdError("Check", "Block walk: no genesis block, block hash: %s.", hashBlock.GetHex().c_str());
            return false;
        }
    }

    CBlockEx* pPrevBlock = nullptr;
    if (block.hashPrev != 0)
    {
        map<uint256, CBlockEx>::iterator it = mapBlock.find(block.hashPrev);
        if (it != mapBlock.end())
        {
            pPrevBlock = &(it->second);
        }
    }
    if (!block.IsGenesis() && pPrevBlock == nullptr)
    {
        StdError("Check", "Block walk: Prev block not exist, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }

    map<uint256, CBlockEx>::iterator mt = mapBlock.insert(make_pair(hashBlock, block)).first;
    if (mt == mapBlock.end())
    {
        StdError("Check", "Block walk: Insert block fail, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    CBlockEx& checkBlock = mt->second;

    CBlockIndex* pNewBlockIndex = nullptr;
    CBlockOutline* pBlockOutline = objBlockIndexWalker.GetBlockOutline(hashBlock);
    if (pBlockOutline == nullptr)
    {
        uint256 nChainTrust;
        if (block.IsGenesis())
        {
            nChainTrust = uint64(0);
        }
        else
        {
            uint256 nTarget;
            nTarget.SetCompact(block.nBits);
            // nChainTrust = 2**256 / (nTarget+1) = ~nTarget / (nTarget+1) + 1
            nChainTrust = (~nTarget / (nTarget + 1)) + 1;
        }
        pNewBlockIndex = AddNewIndex(hashBlock, static_cast<const CBlock&>(checkBlock), nFile, nOffset, nChainTrust);
        if (pNewBlockIndex == nullptr)
        {
            StdError("Check", "Block walk: Add new block index fail 1, block: %s.", hashBlock.GetHex().c_str());
            return false;
        }
    }
    else
    {
        pNewBlockIndex = AddNewIndex(hashBlock, *pBlockOutline);
        if (pNewBlockIndex == nullptr)
        {
            StdError("Check", "Block walk: Add new block index fail 2, block: %s.", hashBlock.GetHex().c_str());
            return false;
        }
        if (pNewBlockIndex->nFile != nFile || pNewBlockIndex->nOffset != nOffset)
        {
            StdLog("Check", "Block walk: Block index param error, block: %s, File: %d - %d, Offset: %d - %d.",
                   hashBlock.GetHex().c_str(), pNewBlockIndex->nFile, nFile, pNewBlockIndex->nOffset, nOffset);
            pNewBlockIndex->nFile = nFile;
            pNewBlockIndex->nOffset = nOffset;
        }
    }
    if (pNewBlockIndex->GetOriginHash() != hashGenesis)
    {
        StdError("Check", "Block walk: Get block origin hash is not genesis block, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    mapCheckFork[pNewBlockIndex->GetOriginHash()].UpdateMaxTrust(pNewBlockIndex);

    if (!UpdateTxTemplateData(hashBlock, block))
    {
        StdLog("Check", "Block walk: Update tx template data fail, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }

    if (!UpdatePledge(hashBlock, block))
    {
        StdLog("Check", "Block walk: Update pledge fail, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }

    if (!UpdateRedeem(hashBlock, block))
    {
        StdLog("Check", "Block walk: Update redeem fail, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }

    nBlockCount++;
    return true;
}

bool CCheckBlockWalker::UpdateBlockNext()
{
    map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.begin();
    while (it != mapCheckFork.end())
    {
        CCheckBlockFork& checkFork = it->second;
        if (checkFork.pOrigin == nullptr || checkFork.pLast == nullptr)
        {
            if (checkFork.pOrigin == nullptr)
            {
                StdError("Check", "UpdateBlockNext: pOrigin is null, fork: %s", it->first.GetHex().c_str());
            }
            else
            {
                StdError("Check", "UpdateBlockNext: pLast is null, fork: %s", it->first.GetHex().c_str());
            }
            mapCheckFork.erase(it++);
            continue;
        }
        CBlockIndex* pBlockIndex = checkFork.pLast;
        while (pBlockIndex && !pBlockIndex->IsOrigin())
        {
            if (pBlockIndex->pPrev)
            {
                pBlockIndex->pPrev->pNext = pBlockIndex;
            }
            pBlockIndex = pBlockIndex->pPrev;
        }
        if (it->first == hashGenesis)
        {
            nMainChainHeight = checkFork.pLast->GetBlockHeight();
        }
        ++it;
    }
    return true;
}

bool CCheckBlockWalker::UpdateBlockTx(CCheckForkManager& objForkMn)
{
    if (hashGenesis == 0)
    {
        StdError("Check", "UpdateBlockTx: hashGenesis is 0");
        return false;
    }

    vector<uint256> vForkList;
    vForkList.push_back(hashGenesis);

    for (const uint256& hashFork : vForkList)
    {
        map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
        if (it != mapCheckFork.end())
        {
            CCheckBlockFork& checkFork = it->second;
            if (checkFork.pOrigin == nullptr)
            {
                StdError("Check", "UpdateBlockTx: pOrigin is null, fork: %s", it->first.GetHex().c_str());
                continue;
            }
            CBlockIndex* pIndex = checkFork.pOrigin;
            while (pIndex)
            {
                map<uint256, CBlockEx>::iterator it = mapBlock.find(pIndex->GetBlockHash());
                if (it == mapBlock.end())
                {
                    StdError("Check", "UpdateBlockTx: Find block fail, block: %s", pIndex->GetBlockHash().GetHex().c_str());
                    return false;
                }
                const CBlockEx& block = it->second;
                if (!block.IsNull())
                {
                    vector<uint256> vFork;
                    objForkMn.GetTxFork(hashFork, pIndex->GetBlockHeight(), vFork);

                    CBufStream ss;
                    CTxContxt txContxt;
                    txContxt.destIn = block.txMint.sendTo;
                    uint32 nTxOffset = pIndex->nOffset + block.GetTxSerializedOffset();
                    if (!AddBlockTx(block.txMint, txContxt, block.GetBlockHeight(), hashFork, pIndex->nFile, nTxOffset, vFork))
                    {
                        StdError("Check", "UpdateBlockTx: Add mint tx fail, txid: %s, block: %s",
                                 block.txMint.GetHash().GetHex().c_str(), pIndex->GetBlockHash().GetHex().c_str());
                        return false;
                    }
                    nTxOffset += ss.GetSerializeSize(block.txMint);

                    CVarInt var(block.vtx.size());
                    nTxOffset += ss.GetSerializeSize(var);
                    for (int i = 0; i < block.vtx.size(); i++)
                    {
                        if (!AddBlockTx(block.vtx[i], block.vTxContxt[i], block.GetBlockHeight(), hashFork, pIndex->nFile, nTxOffset, vFork))
                        {
                            StdError("Check", "UpdateBlockTx: Add tx fail, txid: %s, block: %s",
                                     block.vtx[i].GetHash().GetHex().c_str(), pIndex->GetBlockHash().GetHex().c_str());
                            return false;
                        }
                        nTxOffset += ss.GetSerializeSize(block.vtx[i]);
                    }
                }
                pIndex = pIndex->pNext;
            }
            if (it->first == hashGenesis)
            {
                nMainChainTxCount = checkFork.mapBlockTx.size();
            }
        }
    }
    return true;
}

bool CCheckBlockWalker::AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, const uint256& hashAtForkIn, uint32 nFileNoIn, uint32 nOffsetIn, const vector<uint256>& vFork)
{
    for (const uint256& hashFork : vFork)
    {
        map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
        if (it != mapCheckFork.end())
        {
            if (!it->second.AddBlockTx(txIn, contxtIn, nHeight, hashAtForkIn, nFileNoIn, nOffsetIn))
            {
                StdError("Check", "Block add tx: Add fail, txid: %s, fork: %s",
                         txIn.GetHash().GetHex().c_str(), hashFork.GetHex().c_str());
                return false;
            }
        }
    }
    return true;
}

CBlockIndex* CCheckBlockWalker::AddNewIndex(const uint256& hash, const CBlock& block, uint32 nFile, uint32 nOffset, const uint256& nChainTrust)
{
    CBlockIndex* pIndexNew = new CBlockIndex(block, nFile, nOffset);
    if (pIndexNew != nullptr)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pIndexNew)).first;
        if (mi == mapBlockIndex.end())
        {
            StdError("Check", "Add New Index: insert fail, block: %s", hash.GetHex().c_str());
            mapBlockIndex.erase(hash);
            delete pIndexNew;
            return nullptr;
        }
        pIndexNew->phashBlock = &((*mi).first);
        pIndexNew->nChainTrust = nChainTrust;

        int64 nMoneySupply = block.GetBlockMint();
        if (nMoneySupply < 0)
        {
            StdError("Check", "Add New Index: GetBlockMint fail, block: %s", hash.GetHex().c_str());
            mapBlockIndex.erase(hash);
            delete pIndexNew;
            return nullptr;
        }
        uint64 nRandBeacon = block.GetBlockBeacon();
        CBlockIndex* pIndexPrev = nullptr;
        map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(block.hashPrev);
        if (miPrev != mapBlockIndex.end())
        {
            pIndexPrev = (*miPrev).second;
            pIndexNew->pPrev = pIndexPrev;
            if (!pIndexNew->IsOrigin())
            {
                pIndexNew->pOrigin = pIndexPrev->pOrigin;
                nRandBeacon ^= pIndexNew->pOrigin->nRandBeacon;
            }
            nMoneySupply += pIndexPrev->nMoneySupply;
            pIndexNew->nChainTrust += pIndexPrev->nChainTrust;
        }
        pIndexNew->nMoneySupply = nMoneySupply;
        pIndexNew->nRandBeacon = nRandBeacon;
    }
    else
    {
        StdError("Check", "Add New Index: new fail, block: %s", hash.GetHex().c_str());
    }
    return pIndexNew;
}

CBlockIndex* CCheckBlockWalker::AddNewIndex(const uint256& hash, const CBlockOutline& objBlockOutline)
{
    CBlockIndex* pIndexNew = new CBlockIndex(static_cast<const CBlockIndex&>(objBlockOutline));
    if (pIndexNew != nullptr)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pIndexNew)).first;
        if (mi == mapBlockIndex.end())
        {
            StdError("Check", "Add New Index: insert fail, block: %s", hash.GetHex().c_str());
            delete pIndexNew;
            return nullptr;
        }
        pIndexNew->phashBlock = &((*mi).first);

        if (objBlockOutline.hashPrev != 0)
        {
            map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(objBlockOutline.hashPrev);
            if (miPrev == mapBlockIndex.end())
            {
                StdError("Check", "Add New Index: find prev fail, prev: %s", objBlockOutline.hashPrev.GetHex().c_str());
                mapBlockIndex.erase(hash);
                delete pIndexNew;
                return nullptr;
            }
            pIndexNew->pPrev = miPrev->second;
        }

        if (objBlockOutline.hashOrigin != 0)
        {
            map<uint256, CBlockIndex*>::iterator miOri = mapBlockIndex.find(objBlockOutline.hashOrigin);
            if (miOri == mapBlockIndex.end())
            {
                StdError("Check", "Add New Index: find origin fail, origin: %s", objBlockOutline.hashOrigin.GetHex().c_str());
                mapBlockIndex.erase(hash);
                delete pIndexNew;
                return nullptr;
            }
            pIndexNew->pOrigin = miOri->second;
        }
    }
    else
    {
        StdError("Check", "Add New Index: new fail, block: %s", hash.GetHex().c_str());
    }
    return pIndexNew;
}

void CCheckBlockWalker::ClearBlockIndex()
{
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin();
    for (; mi != mapBlockIndex.end(); ++mi)
    {
        delete (*mi).second;
    }
    mapBlockIndex.clear();
    mapBlock.clear();
}

bool CCheckBlockWalker::CheckTxExist(const uint256& hashFork, const uint256& txid, int& nHeight)
{
    map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
    if (it == mapCheckFork.end())
    {
        StdError("Check", "CheckTxExist: find fork fail, fork: %s, txid: %s", hashFork.GetHex().c_str(), txid.GetHex().c_str());
        return false;
    }
    return it->second.CheckTxExist(txid, nHeight);
}

bool CCheckBlockWalker::GetBlockWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    map<uint256, CCheckBlockFork>::iterator mt = mapCheckFork.begin();
    for (; mt != mapCheckFork.end(); ++mt)
    {
        const uint256& hashFork = mt->first;
        CCheckBlockFork& checkFork = mt->second;
        if (checkFork.pOrigin == nullptr)
        {
            StdError("Check", "GetBlockWalletTx: pOrigin is null, fork: %s", hashFork.GetHex().c_str());
            return false;
        }
        CBlockIndex* pBlockIndex = checkFork.pOrigin;
        while (pBlockIndex)
        {
            map<uint256, CBlockEx>::iterator it = mapBlock.find(pBlockIndex->GetBlockHash());
            if (it == mapBlock.end())
            {
                StdError("Check", "GetBlockWalletTx: find block fail, block: %s, fork: %s", pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                return false;
            }
            const CBlockEx& block = it->second;
            for (int i = 0; i < block.vtx.size(); i++)
            {
                //CAssembledTx(const CTransaction& tx, int nBlockHeightIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
                //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)

                bool fIsMine = (setAddress.find(block.vtx[i].sendTo) != setAddress.end());
                bool fFromMe = (setAddress.find(block.vTxContxt[i].destIn) != setAddress.end());
                if (fIsMine || fFromMe)
                {
                    CAssembledTx atx(block.vtx[i], block.GetBlockHeight(), block.vTxContxt[i].destIn, block.vTxContxt[i].GetValueIn());
                    CWalletTx wtx(block.vtx[i].GetHash(), atx, hashFork, fIsMine, fFromMe);
                    vWalletTx.push_back(wtx);
                }
            }
            if (!block.txMint.IsNull() && setAddress.find(block.txMint.sendTo) != setAddress.end())
            {
                CAssembledTx atx(block.txMint, block.GetBlockHeight(), CDestination(), 0);
                CWalletTx wtx(block.txMint.GetHash(), atx, hashFork, true, false);
                vWalletTx.push_back(wtx);
            }
            pBlockIndex = pBlockIndex->pNext;
        }
    }
    return true;
}

bool CCheckBlockWalker::CheckBlockIndex()
{
    for (map<uint256, CBlockIndex*>::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); ++it)
    {
        if (!objBlockIndexWalker.CheckBlock(CBlockOutline(it->second)))
        {
            StdLog("Check", "CheckBlockIndex: Find block index fail, add block index, block: %s.", it->first.GetHex().c_str());
            if (!fOnlyCheck)
            {
                dbBlockIndex.AddNewBlock(CBlockOutline(it->second));
            }
        }
    }
    for (map<uint256, CBlockOutline>::iterator it = objBlockIndexWalker.mapBlockIndex.begin(); it != objBlockIndexWalker.mapBlockIndex.end(); ++it)
    {
        const CBlockOutline& blockOut = it->second;
        if (mapBlockIndex.find(blockOut.hashBlock) == mapBlockIndex.end())
        {
            StdLog("Check", "CheckBlockIndex: Find block hash fail, remove block index, block: %s.", blockOut.hashBlock.GetHex().c_str());
            if (!fOnlyCheck)
            {
                dbBlockIndex.RemoveBlock(blockOut.hashBlock);
            }
        }
    }
    return true;
}

bool CCheckBlockWalker::UpdateTxTemplateData(const uint256& hashBlock, const CBlockEx& block)
{
    if (block.txMint.sendTo.IsTemplate() && CTemplate::IsDestInRecorded(block.txMint.sendTo))
    {
        CTemplatePtr ptr = CTemplate::CreateTemplatePtr(block.txMint.sendTo.GetTemplateId().GetType(), block.txMint.vchSig);
        if (!ptr)
        {
            StdLog("Check", "Update Tx Template Data: mint tx sendto error, sendto: %s, block: %s",
                   CAddress(block.txMint.sendTo).ToString().c_str(), hashBlock.GetHex().c_str());
            return false;
        }
        if (ptr->GetTemplateId() != block.txMint.sendTo.GetTemplateId())
        {
            StdLog("Check", "Update Tx Template Data: mint tx templateid error, sendto: %s, block: %s",
                   CAddress(block.txMint.sendTo).ToString().c_str(), hashBlock.GetHex().c_str());
            return false;
        }
        if (!UpdateDestTemplateData(block.txMint.sendTo, ptr->GetTemplateData()))
        {
            StdLog("Check", "Update Tx Template Data: mint tx update fail, sendto: %s, block: %s",
                   CAddress(block.txMint.sendTo).ToString().c_str(), hashBlock.GetHex().c_str());
            return false;
        }
    }
    for (size_t i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        const CTxContxt& txcontxt = block.vTxContxt[i];
        if (!tx.vchSig.empty())
        {
            vector<uint8> vDestInData;
            if (tx.sendTo.IsTemplate() && CTemplate::IsDestInRecorded(tx.sendTo))
            {
                CTemplatePtr ptr = CTemplate::CreateTemplatePtr(tx.sendTo.GetTemplateId().GetType(), tx.vchSig);
                if (!ptr)
                {
                    StdLog("Check", "Update tx template data: CreateTemplatePtr fail, sendTo: %s, tx: %s, block: %s.",
                           CAddress(tx.sendTo).ToString().c_str(), tx.GetHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
                if (ptr->GetTemplateId() != tx.sendTo.GetTemplateId())
                {
                    StdLog("Check", "Update tx template data: GetTemplateId error, sendTo: %s, tx: %s, block: %s.",
                           CAddress(tx.sendTo).ToString().c_str(), tx.GetHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
                if (!UpdateDestTemplateData(tx.sendTo, ptr->GetTemplateData()))
                {
                    StdLog("Check", "Update tx template data: UpdateDestTemplateData fail, sendTo: %s, tx: %s, block: %s.",
                           CAddress(tx.sendTo).ToString().c_str(), tx.GetHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
                set<CDestination> setSubDest;
                if (!ptr->GetSignDestination(tx, uint256(), 0, tx.vchSig, setSubDest, vDestInData))
                {
                    StdLog("Check", "Update tx template data: GetSignDestination fail, sendTo: %s, tx: %s, block: %s.",
                           CAddress(tx.sendTo).ToString().c_str(), tx.GetHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
            }
            else
            {
                vDestInData = tx.vchSig;
            }

            if (txcontxt.destIn.IsTemplate())
            {
                CTemplatePtr ptr = CTemplate::CreateTemplatePtr(txcontxt.destIn.GetTemplateId().GetType(), vDestInData);
                if (!ptr)
                {
                    StdLog("Check", "Update tx template data: CreateTemplatePtr fail, destIn: %s, tx: %s, block: %s.",
                           CAddress(txcontxt.destIn).ToString().c_str(), tx.GetHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
                if (ptr->GetTemplateId() != txcontxt.destIn.GetTemplateId())
                {
                    StdLog("Check", "Update tx template data: GetTemplateId error, destIn: %s, tx: %s, block: %s.",
                           CAddress(txcontxt.destIn).ToString().c_str(), tx.GetHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
                if (!UpdateDestTemplateData(txcontxt.destIn, ptr->GetTemplateData()))
                {
                    StdLog("Check", "Update tx template data: UpdateDestTemplateData fail, destIn: %s, tx: %s, block: %s.",
                           CAddress(txcontxt.destIn).ToString().c_str(), tx.GetHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
            }
        }
    }
    return true;
}

bool CCheckBlockWalker::UpdateDestTemplateData(const CDestination& dest, const vector<uint8>& vTemplateData)
{
    auto tit = mapTemplateData.find(dest);
    if (tit == mapTemplateData.end())
    {
        StdLog("Check", "Update dest template data: Template data not exist, dest: %s", CAddress(dest).ToString().c_str());
        if (fOnlyCheck)
        {
            return false;
        }
        if (!dbTemplateData.AddNew(dest, vTemplateData))
        {
            StdError("Check", "Check template data: AddNew sendTo fail, dest: %s", CAddress(dest).ToString().c_str());
            return false;
        }
        mapTemplateData.insert(make_pair(dest, make_pair(vTemplateData, 1)));
    }
    else
    {
        if (tit->second.first != vTemplateData)
        {
            StdLog("Check", "Check template data: Template data error, dest: %s", CAddress(dest).ToString().c_str());
            if (fOnlyCheck)
            {
                return false;
            }
            if (!dbTemplateData.AddNew(dest, vTemplateData))
            {
                StdError("Check", "Check template data: AddNew sendTo fail, dest: %s", CAddress(dest).ToString().c_str());
                return false;
            }
        }
        tit->second.second = 1;
    }
    return true;
}

bool CCheckBlockWalker::ClearSurplusTemplateData()
{
    for (const auto& kv : mapTemplateData)
    {
        if (kv.second.second == 0)
        {
            StdLog("Check", "ClearSurplusTemplateData: Surplus template data, dest: %s.", CAddress(kv.first).ToString().c_str());
            if (!fOnlyCheck)
            {
                dbTemplateData.Remove(kv.first);
            }
        }
    }
    return true;
}

bool CCheckBlockWalker::UpdatePledge(const uint256& hashBlock, const CBlockEx& block)
{
    map<CDestination, pair<CDestination, int64>> mapBlockPledge; // pledge address, pow address
    for (size_t i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        const CTxContxt& txcontxt = block.vTxContxt[i];
        if (tx.sendTo.IsTemplate() && tx.sendTo.GetTemplateId().GetType() == TEMPLATE_MINTPLEDGE)
        {
            CTemplatePtr ptr = nullptr;
            if (!tx.vchSig.empty())
            {
                ptr = CTemplate::CreateTemplatePtr(TEMPLATE_MINTPLEDGE, tx.vchSig);
            }
            if (!ptr)
            {
                vector<uint8> vTemplateData;
                if (!dbTemplateData.Retrieve(tx.sendTo, vTemplateData))
                {
                    StdLog("Check", "Update pledge: Retrieve template data fail, sendTo: %s", CAddress(tx.sendTo).ToString().c_str());
                    return false;
                }
                ptr = CTemplate::CreateTemplatePtr(TEMPLATE_MINTPLEDGE, vTemplateData);
                if (!ptr)
                {
                    StdLog("Check", "Update pledge: CreateTemplatePtr fail, sendTo: %s", CAddress(tx.sendTo).ToString().c_str());
                    return false;
                }
            }
            auto pledge = boost::dynamic_pointer_cast<CTemplateMintPledge>(ptr);
            auto& md = mapBlockPledge[tx.sendTo];
            md.first = pledge->destPowMint;
            md.second += tx.nAmount;
        }
        if (txcontxt.destIn.IsTemplate() && txcontxt.destIn.GetTemplateId().GetType() == TEMPLATE_MINTPLEDGE)
        {
            vector<uint8> vDestInData;
            if (tx.sendTo.IsTemplate() && CTemplate::IsDestInRecorded(tx.sendTo))
            {
                CTemplatePtr ptr = CTemplate::CreateTemplatePtr(tx.sendTo.GetTemplateId().GetType(), tx.vchSig);
                if (!ptr)
                {
                    StdLog("Check", "Update pledge: CreateTemplatePtr fail, destIn: %s", CAddress(txcontxt.destIn).ToString().c_str());
                    return false;
                }
                set<CDestination> setSubDest;
                if (!ptr->GetSignDestination(tx, uint256(), 0, tx.vchSig, setSubDest, vDestInData))
                {
                    StdLog("Check", "Update pledge: GetSignDestination fail, destIn: %s", CAddress(txcontxt.destIn).ToString().c_str());
                    return false;
                }
            }
            else
            {
                vDestInData = tx.vchSig;
            }
            CTemplatePtr ptr = nullptr;
            if (!vDestInData.empty())
            {
                ptr = CTemplate::CreateTemplatePtr(TEMPLATE_MINTPLEDGE, vDestInData);
            }
            if (!ptr)
            {
                vector<uint8> vTemplateData;
                if (!dbTemplateData.Retrieve(txcontxt.destIn, vTemplateData))
                {
                    StdLog("Check", "Update pledge: Retrieve fail, destIn: %s", CAddress(txcontxt.destIn).ToString().c_str());
                    return false;
                }
                ptr = CTemplate::CreateTemplatePtr(TEMPLATE_MINTPLEDGE, vTemplateData);
                if (!ptr)
                {
                    StdLog("Check", "Update pledge: CreateTemplatePtr fail, destIn: %s", CAddress(txcontxt.destIn).ToString().c_str());
                    return false;
                }
            }
            auto pledge = boost::dynamic_pointer_cast<CTemplateMintPledge>(ptr);
            auto& md = mapBlockPledge[txcontxt.destIn];
            md.first = pledge->destPowMint;
            md.second -= (tx.nAmount + tx.nTxFee);
        }
    }

    CPledgeContext ctxtPledge;
    if (!dbPledge.GetBlockPledge(hashBlock, block.hashPrev, mapBlockPledge, ctxtPledge))
    {
        StdLog("Check", "Update pledge: Get block pledge fail, block: %s", hashBlock.GetHex().c_str());
        return false;
    }

    bool fUpdate = false;
    CPledgeContext ctxtCurPledge;
    if (!dbPledge.RetrieveBlockPledge(hashBlock, ctxtCurPledge))
    {
        StdLog("Check", "Update pledge: Retrieve pledge fail, block: %s", hashBlock.GetHex().c_str());
        if (fOnlyCheck)
        {
            return false;
        }
        fUpdate = true;
    }
    else
    {
        if (ctxtCurPledge != ctxtPledge)
        {
            StdLog("Check", "Update pledge: Pledge error, block: %s", hashBlock.GetHex().c_str());
            if (fOnlyCheck)
            {
                return false;
            }
            fUpdate = true;
        }
    }
    if (fUpdate)
    {
        if (!dbPledge.AddNew(hashBlock, ctxtPledge))
        {
            StdLog("Check", "Update pledge: Add pledge fail, block: %s", hashBlock.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckBlockWalker::UpdateRedeem(const uint256& hashBlock, const CBlockEx& block)
{
    vector<pair<CDestination, int64>> vTxRedeem;
    for (size_t i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        const CTxContxt& txcontxt = block.vTxContxt[i];
        if (tx.sendTo.IsTemplate() && tx.sendTo.GetTemplateId().GetType() == TEMPLATE_MINTREDEEM)
        {
            vTxRedeem.push_back(make_pair(tx.sendTo, tx.nAmount));
        }
        if (txcontxt.destIn.IsTemplate() && txcontxt.destIn.GetTemplateId().GetType() == TEMPLATE_MINTREDEEM)
        {
            vTxRedeem.push_back(make_pair(txcontxt.destIn, 0 - (tx.nAmount + tx.nTxFee)));
        }
    }

    CRedeemContext ctxtRedeem;
    if (!dbRedeem.GetBlockRedeem(hashBlock, block.hashPrev, vTxRedeem, ctxtRedeem))
    {
        StdLog("Check", "Update redeem: Get block redeem fail, block: %s", hashBlock.GetHex().c_str());
        return false;
    }

    bool fUpdate = false;
    CRedeemContext ctxtCurRedeem;
    if (!dbRedeem.RetrieveBlockRedeem(hashBlock, ctxtCurRedeem))
    {
        StdLog("Check", "Update redeem: Retrieve redeem fail, block: %s", hashBlock.GetHex().c_str());
        if (fOnlyCheck)
        {
            return false;
        }
        fUpdate = true;
    }
    else
    {
        if (ctxtCurRedeem != ctxtRedeem)
        {
            StdLog("Check", "Update redeem: Pledge error, block: %s", hashBlock.GetHex().c_str());
            if (fOnlyCheck)
            {
                return false;
            }
            fUpdate = true;
        }
    }
    if (fUpdate)
    {
        if (!dbRedeem.AddNew(hashBlock, ctxtRedeem))
        {
            StdLog("Check", "Update redeem: Add redeem fail, block: %s", hashBlock.GetHex().c_str());
            return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckRepairData

bool CCheckRepairData::FetchBlockData()
{
    CTimeSeriesCached tsBlock;
    if (!tsBlock.Initialize(path(strDataPath) / "block", BLOCKFILE_PREFIX))
    {
        StdError("Check", "tsBlock Initialize fail");
        return false;
    }

    if (!objBlockWalker.Initialize(strDataPath))
    {
        StdError("Check", "objBlockWalker Initialize fail");
        return false;
    }

    uint32 nLastFileRet = 0;
    uint32 nLastPosRet = 0;

    StdLog("Check", "Fetch block and tx......");
    if (!tsBlock.WalkThrough(objBlockWalker, nLastFileRet, nLastPosRet, !fOnlyCheck))
    {
        StdError("Check", "Fetch block and tx fail.");
        return false;
    }
    StdLog("Check", "Fetch block and tx success, block: %ld.", objBlockWalker.nBlockCount);

    if (objBlockWalker.nBlockCount > 0)
    {
        if (objBlockWalker.hashGenesis == 0)
        {
            StdError("Check", "Not genesis block");
            return false;
        }

        StdLog("Check", "Update blockchain......");
        if (!objBlockWalker.UpdateBlockNext())
        {
            StdError("Check", "Fetch block and tx, update block next fail");
            return false;
        }
        StdLog("Check", "Update blockchain success, main chain height: %d.", objBlockWalker.nMainChainHeight);

        StdLog("Check", "Update block tx......");
        if (!objBlockWalker.UpdateBlockTx(objForkManager))
        {
            StdError("Check", "Fetch block and tx, update block tx fail");
            return false;
        }
        StdLog("Check", "Update block tx success, main chain tx count: %ld.", objBlockWalker.nMainChainTxCount);
    }

    objBlockWalker.ClearSurplusTemplateData();
    return true;
}

bool CCheckRepairData::FetchUnspent()
{
    CUnspentDB dbUnspent;
    if (!dbUnspent.Initialize(path(strDataPath), false))
    {
        StdError("Check", "FetchUnspent: dbUnspent Initialize fail");
        return false;
    }

    map<uint256, CCheckBlockFork>::iterator it = objBlockWalker.mapCheckFork.begin();
    for (; it != objBlockWalker.mapCheckFork.end(); ++it)
    {
        if (!dbUnspent.LoadFork(it->first))
        {
            StdError("Check", "FetchUnspent: dbUnspent LoadFork fail.");
            dbUnspent.Deinitialize();
            return false;
        }
        if (!dbUnspent.WalkThrough(it->first, mapForkUnspentWalker[it->first]))
        {
            StdError("Check", "FetchUnspent: dbUnspent WalkThrough fail.");
            dbUnspent.Deinitialize();
            return false;
        }
    }

    dbUnspent.Deinitialize();
    return true;
}

bool CCheckRepairData::FetchTxPool()
{
    map<uint256, CCheckBlockFork>::iterator it = objBlockWalker.mapCheckFork.begin();
    for (; it != objBlockWalker.mapCheckFork.end(); ++it)
    {
        objTxPoolData.AddForkUnspent(it->first, it->second.mapBlockUnspent);
    }
    if (!objTxPoolData.FetchTxPool(strDataPath))
    {
        StdError("Check", "FetchTxPool: Fetch tx pool fail.");
        return false;
    }
    return true;
}

bool CCheckRepairData::FetchWalletAddress()
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("Check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.WalkThroughAddress(objWalletAddressWalker))
    {
        StdLog("Check", "WalkThroughAddress fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::FetchWalletTx()
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("Check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.WalkThroughTx(objWalletTxWalker))
    {
        StdLog("Check", "WalkThroughTx fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::CheckRepairFork()
{
    vector<pair<uint256, uint256>> vForkUpdate;
    map<uint256, CCheckForkStatus>::iterator it = objForkManager.mapForkStatus.begin();
    for (; it != objForkManager.mapForkStatus.end(); ++it)
    {
        map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.find(it->first);
        if (mt != objBlockWalker.mapCheckFork.end() && mt->second.pLast != nullptr)
        {
            if (it->second.hashLastBlock != mt->second.pLast->GetBlockHash())
            {
                StdLog("Check", "Check fork last: last block error, fork: %s, fork last: %s, block last: %s",
                       it->first.GetHex().c_str(),
                       it->second.hashLastBlock.GetHex().c_str(),
                       mt->second.pLast->GetBlockHash().GetHex().c_str());
                vForkUpdate.push_back(make_pair(it->first, mt->second.pLast->GetBlockHash()));
            }
        }
    }
    if (!fOnlyCheck && !vForkUpdate.empty())
    {
        return objForkManager.UpdateForkLast(strDataPath, vForkUpdate);
    }
    return true;
}

bool CCheckRepairData::CheckBlockUnspent()
{
    bool fCheckResult = true;
    map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
    for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
    {
        if (!mapForkUnspentWalker[mt->first].CheckForkUnspent(mt->second.mapBlockUnspent))
        {
            fCheckResult = false;
        }
    }
    return fCheckResult;
}

bool CCheckRepairData::CheckWalletTx(vector<CWalletTx>& vAddTx, vector<uint256>& vRemoveTx)
{
    //tx check
    {
        map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
        for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckBlockFork& objBlockFork = mt->second;
            map<uint256, CCheckBlockTx>::iterator it = objBlockFork.mapBlockTx.begin();
            for (; it != objBlockFork.mapBlockTx.end(); ++it)
            {
                const uint256& txid = it->first;
                const CCheckBlockTx& cacheTx = it->second;
                if (cacheTx.hashAtFork == hashFork && cacheTx.tx.nAmount > 0)
                {
                    bool fIsMine = objWalletAddressWalker.CheckAddress(cacheTx.tx.sendTo);
                    bool fFromMe = objWalletAddressWalker.CheckAddress(cacheTx.txContxt.destIn);
                    if (fIsMine || fFromMe)
                    {
                        CCheckWalletTx* pWalletTx = objWalletTxWalker.GetWalletTx(hashFork, txid);
                        if (pWalletTx == nullptr)
                        {
                            StdLog("Check", "CheckWalletTx: [block tx] find wallet tx fail, txid: %s, block height: %d, fork: %s",
                                   txid.GetHex().c_str(), cacheTx.txIndex.nBlockHeight, hashFork.GetHex().c_str());
                            //CAssembledTx(const CTransaction& tx, int nBlockHeightIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
                            //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)
                            CAssembledTx atx(cacheTx.tx, cacheTx.txIndex.nBlockHeight, cacheTx.txContxt.destIn, cacheTx.txContxt.GetValueIn());
                            CWalletTx wtx(txid, atx, hashFork, fIsMine, fFromMe);
                            objWalletTxWalker.AddWalletTx(wtx);
                            vAddTx.push_back(wtx);
                        }
                        else
                        {
                            if (pWalletTx->nBlockHeight != cacheTx.txIndex.nBlockHeight)
                            {
                                StdLog("Check", "CheckWalletTx: [block tx] wallet tx height error, wtx height: %d, block height: %d, txid: %s",
                                       pWalletTx->nBlockHeight, cacheTx.txIndex.nBlockHeight, txid.GetHex().c_str());
                                pWalletTx->nBlockHeight = cacheTx.txIndex.nBlockHeight;
                                vAddTx.push_back(*pWalletTx);
                            }
                        }
                    }
                }
            }
        }
    }
    {
        map<uint256, CCheckForkTxPool>::iterator mt = objTxPoolData.mapForkTxPool.begin();
        for (; mt != objTxPoolData.mapForkTxPool.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckForkTxPool& objTxPoolFork = mt->second;
            map<uint256, CAssembledTx>::iterator it = objTxPoolFork.mapTxPoolTx.begin();
            for (; it != objTxPoolFork.mapTxPoolTx.end(); ++it)
            {
                const uint256& txid = it->first;
                const CAssembledTx& tx = it->second;
                bool fIsMine = objWalletAddressWalker.CheckAddress(tx.sendTo);
                bool fFromMe = objWalletAddressWalker.CheckAddress(tx.destIn);
                if (fIsMine || fFromMe)
                {
                    CCheckWalletTx* pWalletTx = objWalletTxWalker.GetWalletTx(hashFork, txid);
                    if (pWalletTx == nullptr)
                    {
                        StdLog("Check", "CheckWalletTx: [pool tx] find wallet tx fail, txid: %s", txid.GetHex().c_str());
                        //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)
                        CWalletTx wtx(txid, tx, hashFork, fIsMine, fFromMe);
                        objWalletTxWalker.AddWalletTx(wtx);
                        vAddTx.push_back(wtx);
                    }
                    else
                    {
                        if (pWalletTx->nBlockHeight != -1)
                        {
                            StdLog("Check", "CheckWalletTx: [pool tx] wallet tx height error, wtx height: %d, txid: %s",
                                   pWalletTx->nBlockHeight, txid.GetHex().c_str());
                            pWalletTx->nBlockHeight = -1;
                            vAddTx.push_back(*pWalletTx);
                        }
                    }
                }
            }
        }
    }
    {
        map<uint256, CCheckWalletForkUnspent>::iterator mt = objWalletTxWalker.mapWalletFork.begin();
        for (; mt != objWalletTxWalker.mapWalletFork.end(); ++mt)
        {
            vector<pair<int, uint256>> vForkRemoveTx;
            const uint256& hashWalletFork = mt->first;
            CCheckWalletForkUnspent& objWalletFork = mt->second;
            map<uint256, CCheckWalletTx>::iterator it = objWalletFork.mapWalletTx.begin();
            for (; it != objWalletFork.mapWalletTx.end(); ++it)
            {
                CCheckWalletTx& wtx = it->second;
                if (wtx.hashFork == hashWalletFork)
                {
                    if (wtx.nBlockHeight >= 0)
                    {
                        int nBlockHeight = 0;
                        if (!objBlockWalker.CheckTxExist(wtx.hashFork, wtx.txid, nBlockHeight))
                        {
                            StdLog("Check", "CheckWalletTx: [wallet tx] find block tx fail, txid: %s", wtx.txid.GetHex().c_str());
                            vForkRemoveTx.push_back(make_pair(wtx.nBlockHeight, wtx.txid));
                            vRemoveTx.push_back(wtx.txid);
                        }
                        else if (wtx.nBlockHeight != nBlockHeight)
                        {
                            StdLog("Check", "CheckWalletTx: [wallet tx] block tx height error, wtx height: %d, block height: %d, txid: %s",
                                   wtx.nBlockHeight, nBlockHeight, wtx.txid.GetHex().c_str());
                            wtx.nBlockHeight = nBlockHeight;
                            vAddTx.push_back(wtx);
                        }
                    }
                    else
                    {
                        if (!objTxPoolData.CheckTxExist(wtx.hashFork, wtx.txid))
                        {
                            StdLog("Check", "CheckWalletTx: [wallet tx] find txpool tx fail, txid: %s", wtx.txid.GetHex().c_str());
                            vForkRemoveTx.push_back(make_pair(wtx.nBlockHeight, wtx.txid));
                            vRemoveTx.push_back(wtx.txid);
                        }
                    }
                }
            }
            for (const auto& txd : vForkRemoveTx)
            {
                objWalletTxWalker.RemoveWalletTx(mt->first, txd.first, txd.second);
            }
        }
    }

    //update unspent
    if (!objWalletTxWalker.UpdateUnspent())
    {
        StdError("Check", "CheckWalletTx UpdateUnspent fail.");
        return false;
    }

    //unspent check
    {
        map<uint256, CCheckWalletForkUnspent>::iterator mt = objWalletTxWalker.mapWalletFork.begin();
        for (; mt != objWalletTxWalker.mapWalletFork.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckWalletForkUnspent& fork = mt->second;
            map<CTxOutPoint, CCheckTxOut>::iterator it = fork.mapWalletUnspent.begin();
            for (; it != fork.mapWalletUnspent.end(); ++it)
            {
                if (!objTxPoolData.CheckTxPoolUnspent(hashFork, it->first, it->second))
                {
                    StdLog("Check", "Check wallet unspent 1: spent fail, height: %d, utxo: [%d] %s.",
                           objWalletTxWalker.GetTxAtBlockHeight(hashFork, it->first.hash), it->first.n, it->first.hash.GetHex().c_str());
                    return false;
                }
            }
        }
    }
    {
        map<uint256, CCheckForkTxPool>::iterator mt = objTxPoolData.mapForkTxPool.begin();
        for (; mt != objTxPoolData.mapForkTxPool.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckForkTxPool& fork = mt->second;
            map<CTxOutPoint, CCheckTxOut>::iterator it = fork.mapTxPoolUnspent.begin();
            for (; it != fork.mapTxPoolUnspent.end(); ++it)
            {
                if (objWalletAddressWalker.CheckAddress(it->second.destTo))
                {
                    if (!objWalletTxWalker.CheckWalletUnspent(hashFork, it->first, it->second))
                    {
                        StdLog("Check", "Check wallet unspent 2: check unspent fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool CCheckRepairData::CheckBlockIndex()
{
    StdLog("Check", "Start check block index.");
    CBlockIndexDB dbBlockIndex;
    if (!dbBlockIndex.Initialize(path(strDataPath)))
    {
        StdLog("Check", "dbBlockIndex Initialize fail");
        return false;
    }

    CCheckBlockIndexWalker walker;
    dbBlockIndex.WalkThroughBlock(walker);
    StdLog("Check", "Fetch block index success, count: %ld", walker.mapBlockIndex.size());

    for (map<uint256, CBlockIndex*>::iterator it = objBlockWalker.mapBlockIndex.begin(); it != objBlockWalker.mapBlockIndex.end(); ++it)
    {
        if (!it->second->IsOrigin())
        {
            if (!walker.CheckBlock(CBlockOutline(it->second)))
            {
                if (!fOnlyCheck)
                {
                    dbBlockIndex.AddNewBlock(CBlockOutline(it->second));
                }
            }
        }
    }

    for (map<uint256, CBlockOutline>::iterator it = walker.mapBlockIndex.begin(); it != walker.mapBlockIndex.end(); ++it)
    {
        const CBlockOutline& blockOut = it->second;
        map<uint256, CBlockEx>::iterator mt = objBlockWalker.mapBlock.find(blockOut.hashBlock);
        if (mt == objBlockWalker.mapBlock.end())
        {
            StdLog("Check", "CheckBlockIndex: Find block hash fail, remove block index, block: %s.", blockOut.hashBlock.GetHex().c_str());
            if (!fOnlyCheck)
            {
                dbBlockIndex.RemoveBlock(blockOut.hashBlock);
            }
        }
    }

    dbBlockIndex.Deinitialize();
    return true;
}

bool CCheckRepairData::CheckTxIndex()
{
    CTxIndexDB dbTxIndex;
    if (!dbTxIndex.Initialize(path(strDataPath), false))
    {
        StdLog("Check", "dbTxIndex Initialize fail");
        return false;
    }

    map<uint256, vector<pair<uint256, CTxIndex>>> mapTxNew;
    map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
    for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
    {
        if (!dbTxIndex.LoadFork(mt->first))
        {
            StdLog("Check", "dbTxIndex LoadFork fail");
            dbTxIndex.Deinitialize();
            return false;
        }
        CBlockIndex* pBlockIndex = mt->second.pLast;
        while (pBlockIndex)
        {
            const uint256& hashBlock = pBlockIndex->GetBlockHash();
            uint256 hashFork = pBlockIndex->GetOriginHash();
            if (hashFork == 0)
            {
                StdLog("Check", "CheckTxIndex: fork is 0");
                pBlockIndex = pBlockIndex->pNext;
                continue;
            }
            map<uint256, CBlockEx>::iterator at = objBlockWalker.mapBlock.find(hashBlock);
            if (at == objBlockWalker.mapBlock.end())
            {
                StdLog("Check", "CheckTxIndex: find block fail");
                return false;
            }
            const CBlockEx& block = at->second;
            if (!block.IsNull())
            {
                CBufStream ss;
                CTxIndex txIndex;

                uint32 nTxOffset = pBlockIndex->nOffset + block.GetTxSerializedOffset();
                if (!dbTxIndex.Retrieve(hashFork, block.txMint.GetHash(), txIndex))
                {
                    StdLog("Check", "Retrieve db tx index fail, height: %d, block: %s, mint tx: %s.",
                           block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.txMint.GetHash().GetHex().c_str());

                    mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                }
                else
                {
                    if (!(txIndex.nFile == pBlockIndex->nFile && txIndex.nOffset == nTxOffset))
                    {
                        StdLog("Check", "Check tx index fail, height: %d, block: %s, mint tx: %s, db offset: %d, block offset: %d.",
                               block.GetBlockHeight(), block.GetHash().GetHex().c_str(),
                               block.txMint.GetHash().GetHex().c_str(), txIndex.nOffset, nTxOffset);

                        mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                    }
                }
                nTxOffset += ss.GetSerializeSize(block.txMint);

                CVarInt var(block.vtx.size());
                nTxOffset += ss.GetSerializeSize(var);
                for (int i = 0; i < block.vtx.size(); i++)
                {
                    if (!dbTxIndex.Retrieve(pBlockIndex->GetOriginHash(), block.vtx[i].GetHash(), txIndex))
                    {
                        StdLog("Check", "Retrieve db tx index fail, height: %d, block: %s, txid: %s.",
                               block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.vtx[i].GetHash().GetHex().c_str());

                        mapTxNew[hashFork].push_back(make_pair(block.vtx[i].GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                    }
                    else
                    {
                        if (!(txIndex.nFile == pBlockIndex->nFile && txIndex.nOffset == nTxOffset))
                        {
                            StdLog("Check", "Check tx index fail, height: %d, block: %s, txid: %s, db offset: %d, block offset: %d.",
                                   block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.vtx[i].GetHash().GetHex().c_str(), txIndex.nOffset, nTxOffset);

                            mapTxNew[hashFork].push_back(make_pair(block.vtx[i].GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                        }
                    }
                    nTxOffset += ss.GetSerializeSize(block.vtx[i]);
                }
            }
            if (block.IsOrigin() || pBlockIndex == mt->second.pOrigin)
            {
                break;
            }
            pBlockIndex = pBlockIndex->pPrev;
        }
    }

    // repair
    if (!fOnlyCheck && !mapTxNew.empty())
    {
        StdLog("Check", "Repair tx index starting");
        for (const auto& fork : mapTxNew)
        {
            if (!dbTxIndex.Update(fork.first, fork.second, vector<uint256>()))
            {
                StdLog("Check", "Repair tx index update fail");
            }
            dbTxIndex.Flush(fork.first);
        }
        StdLog("Check", "Repair tx index success");
    }

    dbTxIndex.Deinitialize();
    return true;
}

bool CCheckRepairData::RemoveTxPoolFile()
{
    CTxPoolData datTxPool;
    if (!datTxPool.Initialize(path(strDataPath)))
    {
        StdLog("Check", "Remove txpool file: Failed to initialize txpool data");
        return false;
    }
    if (!datTxPool.Remove())
    {
        StdLog("Check", "Remove txpool file: Remove failed");
        return false;
    }
    return true;
}

bool CCheckRepairData::RepairUnspent()
{
    CUnspentDB dbUnspent;
    if (!dbUnspent.Initialize(path(strDataPath), false))
    {
        StdLog("Check", "dbUnspent Initialize fail");
        return false;
    }

    map<uint256, CCheckForkUnspentWalker>::iterator it = mapForkUnspentWalker.begin();
    for (; it != mapForkUnspentWalker.end(); ++it)
    {
        if (!it->second.vAddUpdate.empty() || !it->second.vRemove.empty())
        {
            if (!dbUnspent.LoadFork(it->first))
            {
                StdLog("Check", "dbUnspent LoadFork fail.");
                dbUnspent.Deinitialize();
                return false;
            }
            if (!dbUnspent.RepairUnspent(it->first, it->second.vAddUpdate, it->second.vRemove))
            {
                StdLog("Check", "dbUnspent RepairUnspent fail.");
                dbUnspent.Deinitialize();
                return false;
            }
        }
    }

    dbUnspent.Deinitialize();
    return true;
}

bool CCheckRepairData::RepairWalletTx(const vector<CWalletTx>& vAddTx, const vector<uint256>& vRemoveTx)
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("Check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.UpdateTx(vAddTx, vRemoveTx))
    {
        StdLog("Check", "Wallet UpdateTx fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::RestructureWalletTx()
{
    vector<CWalletTx> vAddTx;
    if (!objBlockWalker.GetBlockWalletTx(objWalletAddressWalker.setAddress, vAddTx))
    {
        StdLog("Check", "Restructure wallet tx: Get block wallet tx fail");
        return false;
    }
    int64 nBlockWalletTxCount = vAddTx.size();
    StdLog("Check", "Restructure wallet tx, block tx count: %ld", nBlockWalletTxCount);

    if (!objTxPoolData.GetTxPoolWalletTx(objWalletAddressWalker.setAddress, vAddTx))
    {
        StdLog("Check", "Restructure wallet tx: Get txpool wallet tx fail");
        return false;
    }
    StdLog("Check", "Restructure wallet tx, txpool tx count: %ld, total tx count: %ld",
           vAddTx.size() - nBlockWalletTxCount, vAddTx.size());

    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("Check", "Restructure wallet tx: dbWallet Initialize fail");
        return false;
    }

    dbWallet.ClearTx();
    if (!dbWallet.UpdateTx(vAddTx, vector<uint256>()))
    {
        StdLog("Check", "Restructure wallet tx: Wallet UpdateTx fail");
        dbWallet.Deinitialize();
        return false;
    }

    dbWallet.Deinitialize();
    return true;
}

////////////////////////////////////////////////////////////////
bool CCheckRepairData::CheckRepairData()
{
    StdLog("Check", "Start check and repair, path: %s", strDataPath.c_str());

    objWalletTxWalker.SetForkManager(&objForkManager);

    if (!objForkManager.FetchForkStatus(strDataPath))
    {
        StdLog("Check", "Fetch fork status fail");
        return false;
    }
    StdLog("Check", "Fetch fork status success");

    if (!FetchBlockData())
    {
        StdLog("Check", "Fetch block data fail");
        return false;
    }
    StdLog("Check", "Fetch block data success");

    if (!FetchUnspent())
    {
        StdLog("Check", "Fetch unspent fail");
        return false;
    }
    StdLog("Check", "Fetch unspent success");

    if (!FetchTxPool())
    {
        StdLog("Check", "Fetch txpool data fail");
        if (!fOnlyCheck)
        {
            if (!RemoveTxPoolFile())
            {
                StdLog("Check", "Remove txpool file fail");
                return false;
            }
        }
    }
    StdLog("Check", "Fetch txpool data success");

    if (!FetchWalletAddress())
    {
        StdLog("Check", "Fetch wallet address fail");
        return false;
    }
    StdLog("Check", "Fetch wallet address success");

    if (!FetchWalletTx())
    {
        StdLog("Check", "Fetch wallet tx fail");
        return false;
    }
    StdLog("Check", "Fetch wallet tx success, wallet tx count: %ld", objWalletTxWalker.nWalletTxCount);

    if (!CheckRepairFork())
    {
        StdLog("Check", "Fetch wallet tx fail");
        return false;
    }
    StdLog("Check", "Check and repair fork success");

    StdLog("Check", "Check block unspent starting");
    if (!CheckBlockUnspent())
    {
        StdLog("Check", "Check block unspent fail");
        if (!fOnlyCheck)
        {
            if (!RepairUnspent())
            {
                StdLog("Check", "Repair unspent fail");
                return false;
            }
            StdLog("Check", "Repair block unspent success");
        }
    }
    else
    {
        StdLog("Check", "Check block unspent success");
    }

    StdLog("Check", "Check wallet tx starting");
    vector<CWalletTx> vAddTx;
    vector<uint256> vRemoveTx;
    if (CheckWalletTx(vAddTx, vRemoveTx))
    {
        if (!vAddTx.empty() || !vRemoveTx.empty())
        {
            if (!fOnlyCheck)
            {
                StdLog("Check", "Check wallet tx fail, start repair wallet tx, add tx: %ld, remove tx: %ld", vAddTx.size(), vRemoveTx.size());
                if (!RepairWalletTx(vAddTx, vRemoveTx))
                {
                    StdLog("Check", "Repair wallet tx fail");
                    return false;
                }
                StdLog("Check", "Repair wallet tx success");
            }
            else
            {
                StdLog("Check", "Check wallet tx fail, add tx: %ld, remove tx: %ld", vAddTx.size(), vRemoveTx.size());
            }
        }
        else
        {
            StdLog("Check", "Check wallet tx success");
        }
    }
    else
    {
        if (!fOnlyCheck)
        {
            StdLog("Check", "Check wallet tx fail, start restructure wallet tx");
            if (!RestructureWalletTx())
            {
                StdLog("Check", "Restructure wallet tx fail");
                return false;
            }
            StdLog("Check", "Restructure wallet tx success");
        }
        else
        {
            StdLog("Check", "Check wallet tx fail, need to restructure wallet tx");
        }
    }

    StdLog("Check", "Check block index starting");
    if (!objBlockWalker.CheckBlockIndex())
    {
        StdLog("Check", "Check block index fail");
        return false;
    }
    StdLog("Check", "Check block index complete");

    StdLog("Check", "Check tx index starting");
    if (!CheckTxIndex())
    {
        StdLog("Check", "Check tx index fail");
        return false;
    }
    StdLog("Check", "Check tx index complete");
    return true;
}

} // namespace minemon
