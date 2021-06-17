// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockbase.h"

#include <boost/timer/timer.hpp>
#include <cstdio>

#include "../minemon/address.h"
#include "template/mintpledge.h"
#include "template/template.h"
#include "util.h"

using namespace std;
using namespace boost::filesystem;
using namespace xengine;

#define BLOCKFILE_PREFIX "block"
#define LOGFILE_NAME "storage.log"

namespace minemon
{
namespace storage
{

//////////////////////////////
// CBlockBaseDBWalker

class CBlockWalker : public CBlockDBWalker
{
public:
    CBlockWalker(CBlockBase* pBaseIn)
      : pBase(pBaseIn) {}
    bool Walk(CBlockOutline& outline) override
    {
        return pBase->LoadIndex(outline);
    }

public:
    CBlockBase* pBase;
};

//////////////////////////////
// CBlockView

CBlockView::CBlockView()
  : pBlockBase(nullptr), hashFork(uint64(0)), fCommittable(false)
{
}

CBlockView::~CBlockView()
{
    Deinitialize();
}

void CBlockView::Initialize(CBlockBase* pBlockBaseIn, boost::shared_ptr<CBlockFork> spForkIn,
                            const uint256& hashForkIn, bool fCommittableIn)
{
    Deinitialize();

    pBlockBase = pBlockBaseIn;
    spFork = spForkIn;
    hashFork = hashForkIn;
    fCommittable = fCommittableIn;
    if (pBlockBase && spFork)
    {
        if (fCommittable)
        {
            spFork->UpgradeLock();
        }
        else
        {
            spFork->ReadLock();
        }
    }
    vTxRemove.clear();
    vAddrTxRemove.clear();
    vTxAddNew.clear();
}

void CBlockView::Deinitialize()
{
    if (pBlockBase)
    {
        if (spFork)
        {
            if (fCommittable)
            {
                spFork->UpgradeUnlock();
            }
            else
            {
                spFork->ReadUnlock();
            }
            spFork = nullptr;
        }
        pBlockBase = nullptr;
        hashFork = 0;
        mapTx.clear();
        mapUnspent.clear();
    }
}

bool CBlockView::ExistsTx(const uint256& txid) const
{
    map<uint256, CTransaction>::const_iterator it = mapTx.find(txid);
    if (it != mapTx.end())
    {
        return (!(*it).second.IsNull());
    }
    return (!!(pBlockBase->ExistsTx(txid)));
}

bool CBlockView::RetrieveTx(const uint256& txid, CTransaction& tx)
{
    map<uint256, CTransaction>::const_iterator it = mapTx.find(txid);
    if (it != mapTx.end())
    {
        tx = (*it).second;
        return (!tx.IsNull());
    }
    return pBlockBase->RetrieveTx(txid, tx);
}

bool CBlockView::RetrieveUnspent(const CTxOutPoint& out, CTxOut& unspent)
{
    map<CTxOutPoint, CViewUnspent>::const_iterator it = mapUnspent.find(out);
    if (it != mapUnspent.end())
    {
        if ((*it).second.IsSpent())
        {
            StdTrace("CBlockView", "RetrieveUnspent: unspent is spent, unspent: [%d]:%s", out.n, out.hash.GetHex().c_str());
            return false;
        }
        unspent = it->second.output;
    }
    else
    {
        if (!pBlockBase->GetTxUnspent(hashFork, out, unspent))
        {
            StdTrace("CBlockView", "RetrieveUnspent: Blockchain unspent don't exist, unspent: [%d]:%s", out.n, out.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CBlockView::AddTx(const uint256& txid, const CTransaction& tx, int nHeight, const CTxContxt& txContxt)
{
    mapTx[txid] = tx;
    vTxAddNew.push_back(txid);

    const CDestination& destIn = txContxt.destIn;
    int64 nValueIn = txContxt.GetValueIn();

    for (int i = 0; i < tx.vInput.size(); i++)
    {
        const CTxInContxt& txin = txContxt.vin[i];
        mapUnspent[tx.vInput[i].prevout].Disable(CTxOut(destIn, txin.nAmount, txin.nTxTime, txin.nLockUntil), -1, -1);
    }
    CTxOut output0(tx);
    if (!output0.IsNull())
    {
        mapUnspent[CTxOutPoint(txid, 0)].Enable(output0, tx.nType, nHeight);
    }
    CTxOut output1(tx, destIn, nValueIn);
    if (!output1.IsNull())
    {
        mapUnspent[CTxOutPoint(txid, 1)].Enable(output1, tx.nType, nHeight);
    }
    return true;
}

void CBlockView::RemoveTx(const uint256& txid, const CTransaction& tx, const int nHeight, const int nBlockSeq, const int nTxSeq, const CTxContxt& txContxt, const bool fAddrTxIndexIn)
{
    mapTx[txid].SetNull();
    vTxRemove.push_back(txid);

    if (fAddrTxIndexIn)
    {
        if (txContxt.destIn == tx.sendTo)
        {
            vAddrTxRemove.push_back(CAddrTxIndex(txContxt.destIn, nHeight, nBlockSeq, nTxSeq, txid));
        }
        else
        {
            if (!txContxt.destIn.IsNull())
            {
                vAddrTxRemove.push_back(CAddrTxIndex(txContxt.destIn, nHeight, nBlockSeq, nTxSeq, txid));
            }
            vAddrTxRemove.push_back(CAddrTxIndex(tx.sendTo, nHeight, nBlockSeq, nTxSeq, txid));
        }
    }

    for (int i = 0; i < tx.vInput.size(); i++)
    {
        const CTxInContxt& in = txContxt.vin[i];
        mapUnspent[tx.vInput[i].prevout].Enable(CTxOut(txContxt.destIn, in.nAmount, in.nTxTime, in.nLockUntil), -1, -1);
    }

    CTxOut output0(tx);
    if (!output0.IsNull())
    {
        mapUnspent[CTxOutPoint(txid, 0)].Disable(output0, tx.nType, nHeight);
    }
    CTxOut output1(tx, txContxt.destIn, txContxt.GetValueIn());
    if (!output1.IsNull())
    {
        mapUnspent[CTxOutPoint(txid, 1)].Disable(output1, tx.nType, nHeight);
    }
}

void CBlockView::AddBlock(const uint256& hash, const CBlockEx& block)
{
    InsertBlockList(hash, block, vBlockAddNew);
}

void CBlockView::RemoveBlock(const uint256& hash, const CBlockEx& block)
{
    InsertBlockList(hash, block, vBlockRemove);
}

void CBlockView::GetUnspentChanges(vector<CTxUnspent>& vAddNew, vector<CTxOutPoint>& vRemove)
{
    vAddNew.reserve(mapUnspent.size());
    vRemove.reserve(mapUnspent.size());

    for (map<CTxOutPoint, CViewUnspent>::iterator it = mapUnspent.begin(); it != mapUnspent.end(); ++it)
    {
        const CTxOutPoint& out = (*it).first;
        CViewUnspent& unspent = (*it).second;
        if (unspent.IsModified())
        {
            if (!unspent.IsSpent())
            {
                vAddNew.push_back(CTxUnspent(out, unspent.output, unspent.nTxType, unspent.nHeight));
            }
            else
            {
                vRemove.push_back(out);
            }
        }
    }
}

void CBlockView::GetUnspentChanges(vector<CTxUnspent>& vAddNewUnspent, vector<CTxUnspent>& vRemoveUnspent)
{
    vAddNewUnspent.reserve(mapUnspent.size());
    vRemoveUnspent.reserve(mapUnspent.size());

    for (map<CTxOutPoint, CViewUnspent>::iterator it = mapUnspent.begin(); it != mapUnspent.end(); ++it)
    {
        const CTxOutPoint& out = (*it).first;
        CViewUnspent& unspent = (*it).second;
        if (unspent.IsModified())
        {
            if (!unspent.IsSpent())
            {
                if (unspent.nTxType == -1 || unspent.nHeight == -1)
                {
                    CTransaction tx;
                    uint256 hashFork;
                    int nHeight;
                    if (pBlockBase->RetrieveTx(out.hash, tx, hashFork, nHeight))
                    {
                        unspent.nTxType = tx.nType;
                        unspent.nHeight = nHeight;
                    }
                    else
                    {
                        StdError("CBlockView", "Get Unspent Changes: RetrieveTx fail, tx: %s", out.hash.GetHex().c_str());
                    }
                }
                vAddNewUnspent.push_back(CTxUnspent(out, unspent.output, unspent.nTxType, unspent.nHeight));
            }
            else
            {
                vRemoveUnspent.push_back(CTxUnspent(out, unspent.output, unspent.nTxType, unspent.nHeight));
            }
        }
    }
}

void CBlockView::GetTxUpdated(set<uint256>& setUpdate)
{
    for (int i = 0; i < vTxRemove.size(); i++)
    {
        const uint256& txid = vTxRemove[i];
        if (!mapTx[txid].IsNull())
        {
            setUpdate.insert(txid);
        }
    }
}

void CBlockView::GetTxRemoved(vector<uint256>& vRemove, vector<CAddrTxIndex>& vAddrTxIndexRemove, const bool fAddrTxIndexIn)
{
    vRemove.reserve(vTxRemove.size());
    for (size_t i = 0; i < vTxRemove.size(); i++)
    {
        const uint256& txid = vTxRemove[i];
        if (mapTx[txid].IsNull())
        {
            vRemove.push_back(txid);
        }
    }
    if (fAddrTxIndexIn)
    {
        vAddrTxIndexRemove.reserve(vAddrTxRemove.size());
        for (size_t i = 0; i < vAddrTxRemove.size(); i++)
        {
            const CAddrTxIndex& addrTxIndex = vAddrTxRemove[i];
            if (mapTx[addrTxIndex.txid].IsNull())
            {
                vAddrTxIndexRemove.push_back(addrTxIndex);
            }
        }
    }
}

void CBlockView::GetBlockChanges(vector<CBlockEx>& vAdd, vector<CBlockEx>& vRemove) const
{
    vAdd.clear();
    vAdd.reserve(vBlockAddNew.size());
    for (auto& pair : vBlockAddNew)
    {
        vAdd.push_back(pair.second);
    }

    vRemove.clear();
    vRemove.reserve(vBlockRemove.size());
    for (auto& pair : vBlockRemove)
    {
        vRemove.push_back(pair.second);
    }
}

void CBlockView::InsertBlockList(const uint256& hash, const CBlockEx& block, list<pair<uint256, CBlockEx>>& blockList)
{
    // store reserve block order
    auto pair = make_pair(hash, block);
    if (blockList.empty())
    {
        blockList.push_back(pair);
    }
    else if (block.hashPrev == blockList.front().first)
    {
        blockList.push_front(pair);
    }
    else if (blockList.back().second.hashPrev == hash)
    {
        blockList.push_back(pair);
    }
    else
    {
        StdError("CBlockView", "InsertBlockList error, no prev and next of block: %s", hash.ToString().c_str());
    }
}

//////////////////////////////
// CForkHeightIndex

void CForkHeightIndex::AddHeightIndex(uint32 nHeight, const uint256& hashBlock, uint32 nBlockTimeStamp, const CDestination& destMint)
{
    mapHeightIndex[nHeight][hashBlock] = CBlockHeightIndex(nBlockTimeStamp, destMint);
}

void CForkHeightIndex::RemoveHeightIndex(uint32 nHeight, const uint256& hashBlock)
{
    mapHeightIndex[nHeight].erase(hashBlock);
}

map<uint256, CBlockHeightIndex>* CForkHeightIndex::GetBlockMintList(uint32 nHeight)
{
    return &(mapHeightIndex[nHeight]);
}

//////////////////////////////
// CBlockBase

CBlockBase::CBlockBase()
  : fDebugLog(false), fCfgAddrTxIndex(false)
{
}

CBlockBase::~CBlockBase()
{
    dbBlock.Deinitialize();
    tsBlock.Deinitialize();
}

bool CBlockBase::Initialize(const path& pathDataLocation, bool fDebug, bool fRenewDB)
{
    if (!SetupLog(pathDataLocation, fDebug))
    {
        return false;
    }

    Log("B", "Initializing... (Path : %s)", pathDataLocation.string().c_str());

    if (!dbBlock.Initialize(pathDataLocation))
    {
        Error("B", "Failed to initialize block db");
        return false;
    }

    if (!tsBlock.Initialize(pathDataLocation / "block", BLOCKFILE_PREFIX))
    {
        dbBlock.Deinitialize();
        Error("B", "Failed to initialize block tsfile");
        return false;
    }

    if (fRenewDB)
    {
        Clear();
    }
    else if (!LoadDB())
    {
        dbBlock.Deinitialize();
        tsBlock.Deinitialize();
        {
            CWriteLock wlock(rwAccess);

            ClearCache();
        }
        Error("B", "Failed to load block db");
        return false;
    }
    Log("B", "Initialized");
    return true;
}

void CBlockBase::Deinitialize()
{
    dbBlock.Deinitialize();
    tsBlock.Deinitialize();
    {
        CWriteLock wlock(rwAccess);

        ClearCache();
    }
    Log("B", "Deinitialized");
}

bool CBlockBase::Exists(const uint256& hash) const
{
    CReadLock rlock(rwAccess);

    return (!!mapIndex.count(hash));
}

bool CBlockBase::ExistsTx(const uint256& txid)
{
    uint256 hashFork;
    CTxIndex txIndex;
    return dbBlock.RetrieveTxIndex(txid, txIndex, hashFork);
}

bool CBlockBase::IsEmpty() const
{
    CReadLock rlock(rwAccess);

    return mapIndex.empty();
}

void CBlockBase::Clear()
{
    CWriteLock wlock(rwAccess);

    dbBlock.RemoveAll();
    ClearCache();
}

bool CBlockBase::Initiate(const uint256& hashGenesis, const CBlock& blockGenesis, const uint256& nChainTrust)
{
    if (!IsEmpty())
    {
        StdTrace("BlockBase", "Is not empty");
        return false;
    }
    uint32 nFile, nOffset;
    if (!tsBlock.Write(CBlockEx(blockGenesis), nFile, nOffset))
    {
        StdTrace("BlockBase", "Write genesis %s block failed", hashGenesis.ToString().c_str());
        return false;
    }

    uint32 nTxOffset = nOffset + blockGenesis.GetTxSerializedOffset();
    uint256 txidMintTx = blockGenesis.txMint.GetHash();

    vector<pair<uint256, CTxIndex>> vTxNew;
    vector<pair<CAddrTxIndex, CAddrTxInfo>> vAddrTxNew;
    vTxNew.push_back(make_pair(txidMintTx, CTxIndex(0, nFile, nTxOffset)));

    CAddrTxInfo txInfo(CAddrTxInfo::TXI_DIRECTION_TO, CDestination(), blockGenesis.txMint);
    vAddrTxNew.push_back(make_pair(CAddrTxIndex(blockGenesis.txMint.sendTo, 0, 0, 0, txidMintTx), txInfo));

    vector<CTxUnspent> vAddNew;
    vAddNew.push_back(CTxUnspent(CTxOutPoint(txidMintTx, 0), CTxOut(blockGenesis.txMint), blockGenesis.txMint.nType, blockGenesis.GetBlockHeight()));

    {
        CWriteLock wlock(rwAccess);
        CBlockIndex* pIndexNew = AddNewIndex(hashGenesis, blockGenesis, nFile, nOffset, nChainTrust);
        if (pIndexNew == nullptr)
        {
            StdTrace("BlockBase", "Add New Index %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        if (!dbBlock.AddNewBlock(CBlockOutline(pIndexNew)))
        {
            StdTrace("BlockBase", "Add New genesis Block %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        CPledgeContext ctxtPledge;
        if (!dbBlock.UpdatePledgeContext(hashGenesis, ctxtPledge))
        {
            StdTrace("BlockBase", "Update Pledge Contetxt %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        CRedeemContext redeemData;
        if (!dbBlock.UpdateRedeemData(hashGenesis, redeemData))
        {
            StdTrace("BlockBase", "Update redeem Contetxt %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        CProfile profile;
        if (!profile.Load(blockGenesis.vchProof))
        {
            StdTrace("BlockBase", "Load genesis %s block Proof failed", hashGenesis.ToString().c_str());
            return false;
        }

        CForkContext ctxt(hashGenesis, uint64(0), uint64(0), profile);
        if (!dbBlock.AddNewForkContext(ctxt))
        {
            StdTrace("BlockBase", "Add New Fork COntext %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        if (!dbBlock.AddNewFork(hashGenesis))
        {
            StdTrace("BlockBase", "Add New Fork %s  failed", hashGenesis.ToString().c_str());
            return false;
        }

        boost::shared_ptr<CBlockFork> spFork = AddNewFork(profile, pIndexNew);
        if (spFork != nullptr)
        {
            CWriteLock wForkLock(spFork->GetRWAccess());

            if (!dbBlock.UpdateFork(hashGenesis, hashGenesis, uint64(0), vTxNew, vector<uint256>(), vAddrTxNew, vector<CAddrTxIndex>(), vAddNew, vector<CTxUnspent>()))
            {
                StdTrace("BlockBase", "Update Fork %s failed", hashGenesis.ToString().c_str());
                return false;
            }
            spFork->UpdateLast(pIndexNew);
        }
        else
        {
            StdTrace("BlockBase", "Add New Fork profile  %s  failed", hashGenesis.ToString().c_str());
            return false;
        }

        Log("B", "Initiate genesis %s", hashGenesis.ToString().c_str());
    }
    return true;
}

bool CBlockBase::AddNew(const uint256& hash, CBlockEx& block, CBlockIndex** ppIndexNew, const uint256& nChainTrust)
{
    if (Exists(hash))
    {
        StdTrace("BlockBase", "Add new block: Exist Block: %s", hash.ToString().c_str());
        return false;
    }

    uint32 nFile, nOffset;
    if (!tsBlock.Write(block, nFile, nOffset))
    {
        StdError("BlockBase", "Add new block: write block failed, block: %s", hash.ToString().c_str());
        return false;
    }

    if (!UpdateTxTemplateData(hash, block))
    {
        StdError("BlockBase", "Add new block: Update template data failed, block: %s", hash.ToString().c_str());
        return false;
    }
    if (!UpdatePledge(hash, block))
    {
        StdError("BlockBase", "Add new block: Update pledge failed, block: %s", hash.ToString().c_str());
        return false;
    }
    if (!UpdateRedeem(hash, block))
    {
        StdError("BlockBase", "Add new block: Update redeem failed, block: %s", hash.ToString().c_str());
        return false;
    }

    {
        CWriteLock wlock(rwAccess);

        CBlockIndex* pIndexNew = AddNewIndex(hash, block, nFile, nOffset, nChainTrust);
        if (pIndexNew == nullptr)
        {
            StdError("BlockBase", "Add new block: Add New Index failed, block: %s", hash.ToString().c_str());
            return false;
        }

        if (!dbBlock.AddNewBlock(CBlockOutline(pIndexNew)))
        {
            StdError("BlockBase", "Add new block: AddNewBlock failed, block: %s", hash.ToString().c_str());
            //mapIndex.erase(hash);
            RemoveBlockIndex(pIndexNew->GetOriginHash(), hash);
            delete pIndexNew;
            return false;
        }

        *ppIndexNew = pIndexNew;
    }

    Log("B", "AddNew block, hash=%s", hash.ToString().c_str());
    return true;
}

bool CBlockBase::AddNewForkContext(const CForkContext& ctxt)
{
    if (!dbBlock.AddNewForkContext(ctxt))
    {
        Error("F", "Failed to addnew forkcontext in %s", ctxt.hashFork.GetHex().c_str());
        return false;
    }
    Log("F", "AddNew forkcontext,hash=%s", ctxt.hashFork.GetHex().c_str());
    return true;
}

bool CBlockBase::Retrieve(const uint256& hash, CBlock& block)
{
    block.SetNull();

    CBlockIndex* pIndex;
    {
        CReadLock rlock(rwAccess);

        if (!(pIndex = GetIndex(hash)))
        {
            StdTrace("BlockBase", "Retrieve::GetIndex %s block failed", hash.ToString().c_str());
            return false;
        }
    }
    if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset, false))
    {
        StdTrace("BlockBase", "Retrieve::Read %s block failed", hash.ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockBase::Retrieve(const CBlockIndex* pIndex, CBlock& block)
{
    block.SetNull();

    if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset, false))
    {
        StdTrace("BlockBase", "RetrieveFromIndex::Read %s block failed, File: %d, Offset: %d",
                 pIndex->GetBlockHash().ToString().c_str(), pIndex->nFile, pIndex->nOffset);
        return false;
    }
    return true;
}

bool CBlockBase::Retrieve(const uint256& hash, CBlockEx& block)
{
    block.SetNull();

    CBlockIndex* pIndex;
    {
        CReadLock rlock(rwAccess);

        if (!(pIndex = GetIndex(hash)))
        {
            StdTrace("BlockBase", "RetrieveBlockEx::GetIndex %s block failed", hash.ToString().c_str());
            return false;
        }
    }
    if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
    {
        StdTrace("BlockBase", "RetrieveBlockEx::Read %s block failed", hash.ToString().c_str());

        return false;
    }
    return true;
}

bool CBlockBase::Retrieve(const CBlockIndex* pIndex, CBlockEx& block)
{
    block.SetNull();

    if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
    {
        StdTrace("BlockBase", "RetrieveFromIndex::GetIndex %s block failed", pIndex->GetBlockHash().ToString().c_str());

        return false;
    }
    return true;
}

bool CBlockBase::RetrieveIndex(const uint256& hash, CBlockIndex** ppIndex)
{
    CReadLock rlock(rwAccess);

    *ppIndex = GetIndex(hash);
    return (*ppIndex != nullptr);
}

bool CBlockBase::RetrieveFork(const uint256& hash, CBlockIndex** ppIndex)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hash);
    if (spFork != nullptr)
    {
        CReadLock rForkLock(spFork->GetRWAccess());

        *ppIndex = spFork->GetLast();

        return true;
    }

    return false;
}

bool CBlockBase::RetrieveFork(const string& strName, CBlockIndex** ppIndex)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(strName);
    if (spFork != nullptr)
    {
        CReadLock rForkLock(spFork->GetRWAccess());

        *ppIndex = spFork->GetLast();

        return true;
    }

    return false;
}

bool CBlockBase::RetrieveProfile(const uint256& hash, CProfile& profile)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hash);
    if (spFork == nullptr)
    {
        return false;
    }

    profile = spFork->GetProfile();

    return true;
}

bool CBlockBase::RetrieveForkContext(const uint256& hash, CForkContext& ctxt)
{
    return dbBlock.RetrieveForkContext(hash, ctxt);
}

bool CBlockBase::RetrieveAncestry(const uint256& hash, vector<pair<uint256, uint256>> vAncestry)
{
    CForkContext ctxt;
    if (!dbBlock.RetrieveForkContext(hash, ctxt))
    {
        StdTrace("BlockBase", "Ancestry Retrieve hashFork %s failed", hash.ToString().c_str());
        return false;
    }

    while (ctxt.hashParent != 0)
    {
        vAncestry.push_back(make_pair(ctxt.hashParent, ctxt.hashJoint));
        if (!dbBlock.RetrieveForkContext(ctxt.hashParent, ctxt))
        {
            return false;
        }
    }

    std::reverse(vAncestry.begin(), vAncestry.end());
    return true;
}

bool CBlockBase::RetrieveOrigin(const uint256& hash, CBlock& block)
{
    block.SetNull();

    CForkContext ctxt;
    if (!dbBlock.RetrieveForkContext(hash, ctxt))
    {
        StdTrace("BlockBase", "RetrieveOrigin::RetrieveForkContext %s block failed", hash.ToString().c_str());
        return false;
    }

    CTransaction tx;
    if (!RetrieveTx(ctxt.txidEmbedded, tx))
    {
        StdTrace("BlockBase", "RetrieveOrigin::RetrieveTx %s tx failed", ctxt.txidEmbedded.ToString().c_str());
        return false;
    }

    try
    {
        CBufStream ss;
        ss.Write((const char*)&tx.vchData[0], tx.vchData.size());
        ss >> block;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CBlockBase::RetrieveTx(const uint256& txid, CTransaction& tx)
{
    tx.SetNull();
    uint256 hashFork;
    CTxIndex txIndex;
    if (!dbBlock.RetrieveTxIndex(txid, txIndex, hashFork))
    {
        StdTrace("BlockBase", "RetrieveTx::RetrieveTxIndex %s tx failed", txid.ToString().c_str());
        return false;
    }

    if (!tsBlock.Read(tx, txIndex.nFile, txIndex.nOffset))
    {
        StdTrace("BlockBase", "RetrieveTx::Read %s tx failed", txid.ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockBase::RetrieveTx(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight)
{
    tx.SetNull();
    CTxIndex txIndex;
    if (!dbBlock.RetrieveTxIndex(txid, txIndex, hashFork))
    {
        StdTrace("BlockBase", "RetrieveTx::RetrieveTxIndex %s tx failed", txid.ToString().c_str());
        return false;
    }
    if (!tsBlock.Read(tx, txIndex.nFile, txIndex.nOffset))
    {
        StdTrace("BlockBase", "RetrieveTx::Read %s tx failed", txid.ToString().c_str());
        return false;
    }
    nHeight = txIndex.nBlockHeight;
    return true;
}

bool CBlockBase::RetrieveTx(const uint256& hashFork, const uint256& txid, CTransaction& tx)
{
    tx.SetNull();

    CTxIndex txIndex;
    if (!dbBlock.RetrieveTxIndex(hashFork, txid, txIndex))
    {
        StdTrace("BlockBase", "RetrieveTxFromFork::RetrieveTxIndex fork:%s txid: %s tx failed",
                 hashFork.ToString().c_str(), txid.ToString().c_str());
        return false;
    }

    if (!tsBlock.Read(tx, txIndex.nFile, txIndex.nOffset))
    {
        StdTrace("BlockBase", "RetrieveTxFromFork::Read %s tx failed",
                 txid.ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockBase::RetrieveTxLocation(const uint256& txid, uint256& hashFork, int& nHeight)
{
    CTxIndex txIndex;
    if (!dbBlock.RetrieveTxIndex(txid, txIndex, hashFork))
    {
        StdTrace("BlockBase", "RetrieveTxLocation::RetrieveTxIndex %s tx failed",
                 txid.ToString().c_str());
        return false;
    }

    nHeight = txIndex.nBlockHeight;
    return true;
}

void CBlockBase::ListForkIndex(multimap<int, CBlockIndex*>& mapForkIndex)
{
    CReadLock rlock(rwAccess);

    mapForkIndex.clear();
    for (map<uint256, boost::shared_ptr<CBlockFork>>::iterator it = mapFork.begin(); it != mapFork.end(); ++it)
    {
        CReadLock rForkLock((*it).second->GetRWAccess());

        CBlockIndex* pIndex = (*it).second->GetLast();
        mapForkIndex.insert(make_pair(pIndex->pOrigin->GetBlockHeight() - 1, pIndex));
    }
}

bool CBlockBase::GetBlockView(CBlockView& view)
{
    boost::shared_ptr<CBlockFork> spFork;
    view.Initialize(this, spFork, uint64(0), false);
    return true;
}

bool CBlockBase::GetBlockView(const uint256& hash, CBlockView& view, bool fCommitable)
{
    CBlockIndex* pIndex = nullptr;
    uint256 hashOrigin;
    boost::shared_ptr<CBlockFork> spFork;

    {
        CReadLock rlock(rwAccess);
        pIndex = GetIndex(hash);
        if (pIndex == nullptr)
        {
            StdTrace("BlockBase", "GetBlockView::GetIndex %s block failed", hash.ToString().c_str());
            return false;
        }

        hashOrigin = pIndex->GetOriginHash();
        spFork = GetFork(hashOrigin);
        if (spFork == nullptr)
        {
            StdTrace("BlockBase", "GetBlockView::GetFork %s  failed", hashOrigin.ToString().c_str());
            return false;
        }
    }

    view.Initialize(this, spFork, hashOrigin, fCommitable);

    {
        CReadLock rlock(rwAccess);
        CBlockIndex* pForkLast = spFork->GetLast();

        vector<CBlockIndex*> vPath;
        CBlockIndex* pBranch = GetBranch(pForkLast, pIndex, vPath);

        uint64 nBlockRemoved = 0;
        uint64 nTxRemoved = 0;
        for (CBlockIndex* p = pForkLast; p != pBranch; p = p->pPrev)
        {
            // remove block tx;
            StdTrace("BlockBase",
                     "Chain rollback attempt[removed block]: height: %u hash: %s time: %u supply: %u bits: %u trust: %s",
                     p->nHeight, p->GetBlockHash().ToString().c_str(), p->nTimeStamp,
                     p->nMoneySupply, p->nProofBits, p->nChainTrust.ToString().c_str());
            ++nBlockRemoved;
            CBlockEx block;
            if (!tsBlock.Read(block, p->nFile, p->nOffset))
            {
                StdTrace("BlockBase",
                         "Chain rollback attempt[remove]: Failed to read block`%s` from file",
                         p->GetBlockHash().ToString().c_str());
                return false;
            }
            int nBlockSeq = 0;
            for (int j = block.vtx.size() - 1; j >= 0; j--)
            {
                StdTrace("BlockBase",
                         "Chain rollback attempt[removed tx]: %s",
                         block.vtx[j].GetHash().ToString().c_str());
                view.RemoveTx(block.vtx[j].GetHash(), block.vtx[j], block.GetBlockHeight(), nBlockSeq, j + 1, block.vTxContxt[j], fCfgAddrTxIndex);
                ++nTxRemoved;
            }
            if (!block.txMint.sendTo.IsNull())
            {
                StdTrace("BlockBase",
                         "Chain rollback attempt[removed mint tx]: %s",
                         block.txMint.GetHash().ToString().c_str());
                view.RemoveTx(block.txMint.GetHash(), block.txMint, block.GetBlockHeight(), nBlockSeq, 0, CTxContxt(), fCfgAddrTxIndex);
                ++nTxRemoved;
            }
            view.RemoveBlock(p->GetBlockHash(), block);
        }
        StdTrace("BlockBase",
                 "Chain rollback attempt[removed block amount]: %lu, [removed tx amount]: %lu",
                 nBlockRemoved, nTxRemoved);

        uint64 nBlockAdded = 0;
        uint64 nTxAdded = 0;
        for (int i = vPath.size() - 1; i >= 0; i--)
        {
            // add block tx;
            StdTrace("BlockBase",
                     "Chain rollback attempt[added block]: height: %u hash: %s time: %u supply: %u bits: %u trust: %s",
                     vPath[i]->nHeight, vPath[i]->GetBlockHash().ToString().c_str(),
                     vPath[i]->nTimeStamp, vPath[i]->nMoneySupply,
                     vPath[i]->nProofBits, vPath[i]->nChainTrust.ToString().c_str());
            ++nBlockAdded;
            CBlockEx block;
            if (!tsBlock.Read(block, vPath[i]->nFile, vPath[i]->nOffset))
            {
                StdTrace("BlockBase",
                         "Chain rollback attempt[add]: Failed to read block`%s` from file",
                         vPath[i]->GetBlockHash().ToString().c_str());
                return false;
            }
            if (!block.txMint.sendTo.IsNull())
            {
                view.AddTx(block.txMint.GetHash(), block.txMint, block.GetBlockHeight(), CTxContxt());
            }
            ++nTxAdded;
            for (int j = 0; j < block.vtx.size(); j++)
            {
                StdTrace("BlockBase",
                         "Chain rollback attempt[added tx]: %s",
                         block.vtx[j].GetHash().ToString().c_str());
                const CTxContxt& txContxt = block.vTxContxt[j];
                view.AddTx(block.vtx[j].GetHash(), block.vtx[j], block.GetBlockHeight(), txContxt);
                ++nTxAdded;
            }
            view.AddBlock(vPath[i]->GetBlockHash(), block);
        }
        StdTrace("BlockBase",
                 "Chain rollback attempt[added block amount]: %lu, [added tx amount]: %lu",
                 nBlockAdded, nTxAdded);
    }
    return true;
}

bool CBlockBase::GetForkBlockView(const uint256& hashFork, CBlockView& view)
{
    boost::shared_ptr<CBlockFork> spFork;
    {
        CReadLock rlock(rwAccess);
        spFork = GetFork(hashFork);
        if (spFork == nullptr)
        {
            return false;
        }
    }
    view.Initialize(this, spFork, hashFork, false);
    return true;
}

bool CBlockBase::CommitBlockView(CBlockView& view, CBlockIndex* pIndexNew)
{
    const uint256 hashFork = pIndexNew->GetOriginHash();

    boost::shared_ptr<CBlockFork> spFork;

    if (hashFork == view.GetForkHash())
    {
        if (!view.IsCommittable())
        {
            StdTrace("BlockBase", "CommitBlockView Is not COmmitable");
            return false;
        }
        spFork = view.GetFork();
    }
    else
    {
        CProfile profile;
        if (!LoadForkProfile(pIndexNew->pOrigin, profile))
        {
            StdTrace("BlockBase", "CommitBlockView::LoadForkProfile %s block failed", pIndexNew->pOrigin->GetBlockHash().ToString().c_str());
            return false;
        }
        if (!dbBlock.AddNewFork(hashFork))
        {
            StdTrace("BlockBase", "CommitBlockView::AddNewFork %s  failed", hashFork.ToString().c_str());
            return false;
        }
        spFork = AddNewFork(profile, pIndexNew);
    }

    vector<pair<uint256, CTxIndex>> vTxNew;
    vector<pair<CAddrTxIndex, CAddrTxInfo>> vAddrTxNew;
    if (!GetTxNewIndex(view, pIndexNew, vTxNew, vAddrTxNew))
    {
        StdTrace("BlockBase", "CommitBlockView: Get tx new index failed");
        return false;
    }

    vector<uint256> vTxDel;
    vector<CAddrTxIndex> vAddrTxDel;
    view.GetTxRemoved(vTxDel, vAddrTxDel, fCfgAddrTxIndex);

    vector<CTxUnspent> vAddNewUnspent;
    vector<CTxUnspent> vRemoveUnspent;
    view.GetUnspentChanges(vAddNewUnspent, vRemoveUnspent);

    if (hashFork == view.GetForkHash())
    {
        spFork->UpgradeToWrite();
    }

    if (!dbBlock.UpdateFork(hashFork, pIndexNew->GetBlockHash(), view.GetForkHash(), vTxNew, vTxDel, vAddrTxNew, vAddrTxDel, vAddNewUnspent, vRemoveUnspent))
    {
        StdTrace("BlockBase", "CommitBlockView::Update fork %s  failed", hashFork.ToString().c_str());
        return false;
    }
    spFork->UpdateLast(pIndexNew);

    Log("B", "Update fork %s, last block hash=%s", hashFork.ToString().c_str(),
        pIndexNew->GetBlockHash().ToString().c_str());
    return true;
}

bool CBlockBase::LoadIndex(CBlockOutline& outline)
{
    uint256 hash = outline.GetBlockHash();
    CBlockIndex* pIndexNew = nullptr;

    map<uint256, CBlockIndex*>::iterator mi = mapIndex.find(hash);
    if (mi != mapIndex.end())
    {
        pIndexNew = (*mi).second;
        *pIndexNew = static_cast<CBlockIndex&>(outline);
    }
    else
    {
        pIndexNew = new CBlockIndex(static_cast<CBlockIndex&>(outline));
        if (pIndexNew == nullptr)
        {
            Log("B", "LoadIndex: new CBlockIndex fail");
            return false;
        }
        mi = mapIndex.insert(make_pair(hash, pIndexNew)).first;
    }

    pIndexNew->phashBlock = &((*mi).first);
    pIndexNew->pPrev = nullptr;
    pIndexNew->pOrigin = pIndexNew;

    if (outline.hashPrev != 0)
    {
        pIndexNew->pPrev = GetOrCreateIndex(outline.hashPrev);
        if (pIndexNew->pPrev == nullptr)
        {
            Log("B", "LoadIndex: GetOrCreateIndex prev block index fail");
            return false;
        }
    }

    if (!pIndexNew->IsOrigin())
    {
        pIndexNew->pOrigin = GetOrCreateIndex(outline.hashOrigin);
        if (pIndexNew->pOrigin == nullptr)
        {
            Log("B", "LoadIndex: GetOrCreateIndex origin block index fail");
            return false;
        }
    }

    UpdateBlockHeightIndex(pIndexNew->GetOriginHash(), hash, pIndexNew->nTimeStamp, pIndexNew->destMint);
    return true;
}

bool CBlockBase::LoadTx(CTransaction& tx, uint32 nTxFile, uint32 nTxOffset, uint256& hashFork)
{
    tx.SetNull();
    if (!tsBlock.Read(tx, nTxFile, nTxOffset))
    {
        StdTrace("BlockBase", "LoadTx::Read %s block failed", tx.GetHash().ToString().c_str());
        return false;
    }
    //CBlockIndex* pIndex = (tx.hashAnchor != 0 ? GetIndex(tx.hashAnchor) : GetOriginIndex(tx.GetHash()));
    CBlockIndex* pIndex = GetOriginIndex(tx.GetHash());
    if (pIndex == nullptr)
    {
        return false;
    }
    hashFork = pIndex->GetOriginHash();
    return true;
}

bool CBlockBase::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        StdTrace("BlockBase", "FilterTx::GetFork %s  failed", hashFork.ToString().c_str());
        return false;
    }

    CReadLock rForkLock(spFork->GetRWAccess());

    for (CBlockIndex* pIndex = spFork->GetOrigin(); pIndex != nullptr; pIndex = pIndex->pNext)
    {
        CBlockEx block;
        if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
        {
            StdLog("BlockBase", "FilterTx: Block read fail, nFile: %d, nOffset: %d.", pIndex->nFile, pIndex->nOffset);
            return false;
        }
        int nBlockHeight = pIndex->GetBlockHeight();
        if (block.txMint.nAmount > 0 && filter.setDest.count(block.txMint.sendTo))
        {
            if (!filter.FoundTx(hashFork, CAssembledTx(block.txMint, nBlockHeight)))
            {
                StdLog("BlockBase", "FilterTx: FoundTx mint tx fail, txid: %s.", block.txMint.GetHash().GetHex().c_str());
                return false;
            }
        }
        for (int i = 0; i < block.vtx.size(); i++)
        {
            CTransaction& tx = block.vtx[i];
            CTxContxt& ctxt = block.vTxContxt[i];

            if (filter.setDest.count(tx.sendTo) || filter.setDest.count(ctxt.destIn))
            {
                if (!filter.FoundTx(hashFork, CAssembledTx(tx, nBlockHeight, ctxt.destIn, ctxt.GetValueIn())))
                {
                    StdLog("BlockBase", "FilterTx: FoundTx tx fail, txid: %s.", tx.GetHash().GetHex().c_str());
                    return false;
                }
            }
        }
    }
    return true;
}

bool CBlockBase::FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        StdTrace("BlockBase", "FilterTx2::GetFork %s  failed", hashFork.ToString().c_str());
        return false;
    }

    CReadLock rForkLock(spFork->GetRWAccess());

    int nCount = 0;
    for (CBlockIndex* pIndex = spFork->GetLast(); pIndex != nullptr && nCount++ < nDepth; pIndex = pIndex->pPrev)
    {
        CBlockEx block;
        if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
        {
            StdLog("BlockBase", "FilterTx2: Block read fail, nFile: %d, nOffset: %d, block: %s.",
                   pIndex->nFile, pIndex->nOffset, pIndex->GetBlockHash().GetHex().c_str());
            return false;
        }
        int nBlockHeight = pIndex->GetBlockHeight();
        if (block.txMint.nAmount > 0 && filter.setDest.count(block.txMint.sendTo))
        {
            if (!filter.FoundTx(hashFork, CAssembledTx(block.txMint, nBlockHeight)))
            {
                StdLog("BlockBase", "FilterTx2: FoundTx mint tx fail, height: %d, txid: %s, block: %s, fork: %s.",
                       nBlockHeight, block.txMint.GetHash().GetHex().c_str(), pIndex->GetBlockHash().GetHex().c_str(), pIndex->GetOriginHash().GetHex().c_str());
                return false;
            }
        }
        for (int i = 0; i < block.vtx.size(); i++)
        {
            CTransaction& tx = block.vtx[i];
            CTxContxt& ctxt = block.vTxContxt[i];

            if (filter.setDest.count(tx.sendTo) || filter.setDest.count(ctxt.destIn))
            {
                if (!filter.FoundTx(hashFork, CAssembledTx(tx, nBlockHeight, ctxt.destIn, ctxt.GetValueIn())))
                {
                    StdLog("BlockBase", "FilterTx2: FoundTx tx fail, height: %d, txid: %s, block: %s, fork: %s.",
                           nBlockHeight, tx.GetHash().GetHex().c_str(), pIndex->GetBlockHash().GetHex().c_str(), pIndex->GetOriginHash().GetHex().c_str());
                    return false;
                }
            }
        }
    }
    return true;
}

bool CBlockBase::ListForkContext(std::vector<CForkContext>& vForkCtxt)
{
    return dbBlock.ListForkContext(vForkCtxt);
}

bool CBlockBase::GetForkBlockLocator(const uint256& hashFork, CBlockLocator& locator, uint256& hashDepth, int nIncStep)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        StdTrace("BlockBase", "GetForkBlockLocator GetFork failed, hashFork: %s", hashFork.ToString().c_str());
        return false;
    }

    CBlockIndex* pIndex = nullptr;
    {
        CReadLock rForkLock(spFork->GetRWAccess());
        pIndex = spFork->GetLast();
        if (pIndex == nullptr)
        {
            StdTrace("BlockBase", "GetForkBlockLocator GetLast failed, hashFork: %s", hashFork.ToString().c_str());
            return false;
        }
    }

    if (hashDepth != 0)
    {
        CBlockIndex* pStartIndex = GetIndex(hashDepth);
        if (pStartIndex != nullptr && pStartIndex->pNext != nullptr)
        {
            pIndex = pStartIndex;
        }
    }

    while (pIndex)
    {
        if (pIndex->GetOriginHash() != hashFork)
        {
            hashDepth = 0;
            break;
        }
        locator.vBlockHash.push_back(pIndex->GetBlockHash());
        if (pIndex->IsOrigin())
        {
            hashDepth = 0;
            break;
        }
        if (locator.vBlockHash.size() >= nIncStep / 2)
        {
            pIndex = pIndex->pPrev;
            if (pIndex == nullptr)
            {
                hashDepth = 0;
            }
            else
            {
                hashDepth = pIndex->GetBlockHash();
            }
            break;
        }
        for (int i = 0; i < nIncStep && !pIndex->IsOrigin(); i++)
        {
            pIndex = pIndex->pPrev;
            if (pIndex == nullptr)
            {
                hashDepth = 0;
                break;
            }
        }
    }

    return true;
}

bool CBlockBase::GetForkBlockInv(const uint256& hashFork, const CBlockLocator& locator, vector<uint256>& vBlockHash, size_t nMaxCount)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        StdTrace("BlockBase", "GetForkBlockInv::GetFork %s failed", hashFork.ToString().c_str());
        return false;
    }

    CReadLock rForkLock(spFork->GetRWAccess());
    CBlockIndex* pIndexLast = spFork->GetLast();
    CBlockIndex* pIndex = nullptr;
    for (const uint256& hash : locator.vBlockHash)
    {
        pIndex = GetIndex(hash);
        if (pIndex != nullptr && (pIndex == pIndexLast || pIndex->pNext != nullptr))
        {
            if (pIndex->GetOriginHash() != hashFork)
            {
                StdTrace("BlockBase", "GetForkBlockInv GetOriginHash error, fork: %s", hashFork.ToString().c_str());
                return false;
            }
            break;
        }
        pIndex = nullptr;
    }

    if (pIndex != nullptr)
    {
        pIndex = pIndex->pNext;
        while (pIndex != nullptr && vBlockHash.size() < nMaxCount)
        {
            vBlockHash.push_back(pIndex->GetBlockHash());
            pIndex = pIndex->pNext;
        }
    }
    return true;
}

bool CBlockBase::ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent)
{
    vUnspent.clear();
    CListUnspentWalker walker(hashFork, dest, nMax);
    dbBlock.WalkThroughUnspent(hashFork, walker);
    vUnspent = walker.vUnspent;
    return true;
}

bool CBlockBase::ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent)
{
    CListUnspentBatchWalker walker(hashFork, mapUnspent, nMax);
    dbBlock.WalkThroughUnspent(hashFork, walker);
    return true;
}

bool CBlockBase::VerifyRepeatBlock(const uint256& hashFork, uint32 height, const CDestination& destMint)
{
    CWriteLock wlock(rwAccess);

    map<uint256, CForkHeightIndex>::iterator it = mapForkHeightIndex.find(hashFork);
    if (it != mapForkHeightIndex.end())
    {
        map<uint256, CBlockHeightIndex>* pBlockMint = it->second.GetBlockMintList(height);
        if (pBlockMint != nullptr)
        {
            for (auto& mt : *pBlockMint)
            {
                if (mt.second.destMint.IsNull())
                {
                    CTransaction tx;
                    CBlockIndex* pBlockIndex = nullptr;
                    if (RetrieveIndex(mt.first, &pBlockIndex)
                        && pBlockIndex != nullptr
                        && RetrieveTx(pBlockIndex->txidMint, tx))
                    {
                        mt.second.destMint = tx.sendTo;
                    }
                }
                if (mt.second.destMint == destMint)
                {
                    StdTrace("CBlockBase", "VerifyRepeatBlock: repeat block: %s, destMint: %s", mt.first.GetHex().c_str(), CAddress(destMint).ToString().c_str());
                    return false;
                }
            }
        }
    }
    return true;
}

bool CBlockBase::GetBlockRedeemContext(const uint256& hashBlock, CRedeemContext& redeemData)
{
    return dbBlock.RetrieveRedeemData(hashBlock, redeemData);
}

bool CBlockBase::GetMintPledgeData(const uint256& hashBlock, const CDestination& destMintPow, const int64 nMinPledge, const int64 nMaxPledge,
                                   map<CDestination, int64>& mapValidPledge, int64& nTotalPledge)
{
    mapValidPledge.clear();
    nTotalPledge = 0;

    CPledgeContext ctxtPledge;
    if (!dbBlock.RetrievePledge(hashBlock, ctxtPledge))
    {
        StdError("CBlockBase", "Get Mint Pledge Data: RetrievePledge fail, block: %s", hashBlock.GetHex().c_str());
        return false;
    }
    auto it = ctxtPledge.mapPledge.find(destMintPow);
    if (it == ctxtPledge.mapPledge.end())
    {
        StdDebug("CBlockBase", "Get Mint Pledge Data: destMintPow find fail, block: %s, destMintPow: %s, pledge size: %ld",
                 hashBlock.GetHex().c_str(), CAddress(destMintPow).ToString().c_str(), ctxtPledge.mapPledge.size());
    }
    else
    {
        for (const auto& kv : it->second)
        {
            if (kv.second >= nMinPledge)
            {
                int64& pledge = mapValidPledge[kv.first];
                pledge = kv.second;
                if (pledge > nMaxPledge)
                {
                    pledge = nMaxPledge;
                }
                nTotalPledge += pledge;
            }
        }
    }
    return true;
}

bool CBlockBase::RetrieveTemplateData(const CDestination& dest, std::vector<uint8>& vTemplateData)
{
    return dbBlock.RetrieveTemplateData(dest, vTemplateData);
}

CBlockIndex* CBlockBase::GetIndex(const uint256& hash) const
{
    map<uint256, CBlockIndex*>::const_iterator mi = mapIndex.find(hash);
    return (mi != mapIndex.end() ? (*mi).second : nullptr);
}

CBlockIndex* CBlockBase::GetOrCreateIndex(const uint256& hash)
{
    map<uint256, CBlockIndex*>::const_iterator mi = mapIndex.find(hash);
    if (mi == mapIndex.end())
    {
        CBlockIndex* pIndexNew = new CBlockIndex();
        mi = mapIndex.insert(make_pair(hash, pIndexNew)).first;
        if (mi == mapIndex.end())
        {
            return nullptr;
        }
        pIndexNew->phashBlock = &((*mi).first);
    }
    return ((*mi).second);
}

CBlockIndex* CBlockBase::GetBranch(CBlockIndex* pIndexRef, CBlockIndex* pIndex, vector<CBlockIndex*>& vPath)
{
    vPath.clear();
    while (pIndex != pIndexRef)
    {
        if (pIndexRef->GetBlockTime() > pIndex->GetBlockTime())
        {
            pIndexRef = pIndexRef->pPrev;
        }
        else if (pIndex->GetBlockTime() > pIndexRef->GetBlockTime())
        {
            vPath.push_back(pIndex);
            pIndex = pIndex->pPrev;
        }
        else
        {
            vPath.push_back(pIndex);
            pIndex = pIndex->pPrev;
            pIndexRef = pIndexRef->pPrev;
        }
    }
    return pIndex;
}

CBlockIndex* CBlockBase::GetOriginIndex(const uint256& txidMint) const
{
    for (map<uint256, boost::shared_ptr<CBlockFork>>::const_iterator mi = mapFork.begin(); mi != mapFork.end(); ++mi)
    {
        CBlockIndex* pIndex = (*mi).second->GetOrigin();
        if (pIndex->txidMint == txidMint)
        {
            return pIndex;
        }
    }
    return nullptr;
}

void CBlockBase::UpdateBlockHeightIndex(const uint256& hashFork, const uint256& hashBlock, uint32 nBlockTimeStamp, const CDestination& destMint)
{
    mapForkHeightIndex[hashFork].AddHeightIndex(CBlock::GetBlockHeightByHash(hashBlock), hashBlock, nBlockTimeStamp, destMint);
}

void CBlockBase::RemoveBlockIndex(const uint256& hashFork, const uint256& hashBlock)
{
    std::map<uint256, CForkHeightIndex>::iterator it = mapForkHeightIndex.find(hashFork);
    if (it != mapForkHeightIndex.end())
    {
        it->second.RemoveHeightIndex(CBlock::GetBlockHeightByHash(hashBlock), hashBlock);
    }
    mapIndex.erase(hashBlock);
}

CBlockIndex* CBlockBase::AddNewIndex(const uint256& hash, const CBlock& block, uint32 nFile, uint32 nOffset, const uint256& nChainTrust)
{
    CBlockIndex* pIndexNew = new CBlockIndex(block, nFile, nOffset);
    if (pIndexNew != nullptr)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapIndex.insert(make_pair(hash, pIndexNew)).first;
        pIndexNew->phashBlock = &((*mi).first);
        pIndexNew->nChainTrust = nChainTrust;

        int64 nMoneySupply = block.GetBlockMint();
        if (nMoneySupply < 0)
        {
            mapIndex.erase(hash);
            delete pIndexNew;
            return nullptr;
        }
        uint64 nRandBeacon = block.GetBlockBeacon();
        CBlockIndex* pIndexPrev = nullptr;
        map<uint256, CBlockIndex*>::iterator miPrev = mapIndex.find(block.hashPrev);
        if (miPrev != mapIndex.end())
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

        UpdateBlockHeightIndex(pIndexNew->GetOriginHash(), hash, block.nTimeStamp, block.txMint.sendTo);
    }
    return pIndexNew;
}

boost::shared_ptr<CBlockFork> CBlockBase::GetFork(const uint256& hash)
{
    map<uint256, boost::shared_ptr<CBlockFork>>::iterator mi = mapFork.find(hash);
    return (mi != mapFork.end() ? (*mi).second : nullptr);
}

boost::shared_ptr<CBlockFork> CBlockBase::GetFork(const std::string& strName)
{
    for (map<uint256, boost::shared_ptr<CBlockFork>>::iterator mi = mapFork.begin(); mi != mapFork.end(); ++mi)
    {
        const CProfile& profile = (*mi).second->GetProfile();
        if (profile.strName == strName)
        {
            return ((*mi).second);
        }
    }
    return nullptr;
}

boost::shared_ptr<CBlockFork> CBlockBase::AddNewFork(const CProfile& profileIn, CBlockIndex* pIndexLast)
{
    boost::shared_ptr<CBlockFork> spFork = boost::shared_ptr<CBlockFork>(new CBlockFork(profileIn, pIndexLast));
    if (spFork != nullptr)
    {
        spFork->UpdateNext();
        mapFork.insert(make_pair(pIndexLast->GetOriginHash(), spFork));
    }

    return spFork;
}

bool CBlockBase::LoadForkProfile(const CBlockIndex* pIndexOrigin, CProfile& profile)
{
    profile.SetNull();

    CBlock block;
    if (!Retrieve(pIndexOrigin, block))
    {
        return false;
    }

    if (!profile.Load(block.vchProof))
    {
        return false;
    }

    return true;
}

bool CBlockBase::GetTxUnspent(const uint256 fork, const CTxOutPoint& out, CTxOut& unspent)
{
    return dbBlock.RetrieveTxUnspent(fork, out, unspent);
}

bool CBlockBase::GetTxNewIndex(CBlockView& view, CBlockIndex* pIndexNew, vector<pair<uint256, CTxIndex>>& vTxNew, vector<pair<CAddrTxIndex, CAddrTxInfo>>& vAddrTxNew)
{
    vector<CBlockIndex*> vPath;
    if (view.GetFork() != nullptr && view.GetFork()->GetLast() != nullptr)
    {
        GetBranch(view.GetFork()->GetLast(), pIndexNew, vPath);
    }
    else
    {
        vPath.push_back(pIndexNew);
    }

    CBufStream ss;
    for (int i = vPath.size() - 1; i >= 0; i--)
    {
        CBlockIndex* pIndex = vPath[i];
        CBlockEx block;
        if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
        {
            return false;
        }
        int nHeight = pIndex->GetBlockHeight();
        uint32 nOffset = pIndex->nOffset + block.GetTxSerializedOffset();

        int nBlockSeq = 0;
        if (!block.txMint.sendTo.IsNull())
        {
            CTxIndex txIndex(nHeight, pIndex->nFile, nOffset);
            vTxNew.push_back(make_pair(block.txMint.GetHash(), txIndex));

            if (fCfgAddrTxIndex)
            {
                CAddrTxInfo txInfo(CAddrTxInfo::TXI_DIRECTION_TO, CDestination(), block.txMint);
                vAddrTxNew.push_back(make_pair(CAddrTxIndex(block.txMint.sendTo, nHeight, nBlockSeq, 0, block.txMint.GetHash()), txInfo));
            }
        }
        nOffset += ss.GetSerializeSize(block.txMint);

        CVarInt var(block.vtx.size());
        nOffset += ss.GetSerializeSize(var);
        for (int i = 0; i < block.vtx.size(); i++)
        {
            CTransaction& tx = block.vtx[i];
            uint256 txid = tx.GetHash();
            CTxIndex txIndex(nHeight, pIndex->nFile, nOffset);
            vTxNew.push_back(make_pair(txid, txIndex));

            if (fCfgAddrTxIndex)
            {
                const CTxContxt& txContxt = block.vTxContxt[i];
                if (tx.sendTo == txContxt.destIn)
                {
                    CAddrTxInfo txInfo(CAddrTxInfo::TXI_DIRECTION_TWO, tx.sendTo, tx);
                    vAddrTxNew.push_back(make_pair(CAddrTxIndex(txContxt.destIn, nHeight, nBlockSeq, i + 1, txid), txInfo));
                }
                else
                {
                    if (!txContxt.destIn.IsNull())
                    {
                        CAddrTxInfo txFromInfo(CAddrTxInfo::TXI_DIRECTION_FROM, tx.sendTo, tx);
                        vAddrTxNew.push_back(make_pair(CAddrTxIndex(txContxt.destIn, nHeight, nBlockSeq, i + 1, txid), txFromInfo));
                    }
                    CAddrTxInfo txToInfo(CAddrTxInfo::TXI_DIRECTION_TO, txContxt.destIn, tx);
                    vAddrTxNew.push_back(make_pair(CAddrTxIndex(tx.sendTo, nHeight, nBlockSeq, i + 1, txid), txToInfo));
                }
            }
            nOffset += ss.GetSerializeSize(tx);
        }
    }
    return true;
}

bool CBlockBase::UpdateTxTemplateData(const uint256& hashBlock, const CBlockEx& block)
{
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
                    return false;
                }
                if (ptr->GetTemplateId() != tx.sendTo.GetTemplateId())
                {
                    return false;
                }
                if (!dbBlock.UpdateTemplateData(tx.sendTo, ptr->GetTemplateData()))
                {
                    return false;
                }
                set<CDestination> setSubDest;
                if (!ptr->GetSignDestination(tx, uint256(), 0, tx.vchSig, setSubDest, vDestInData))
                {
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
                    return false;
                }
                if (ptr->GetTemplateId() != txcontxt.destIn.GetTemplateId())
                {
                    return false;
                }
                if (!dbBlock.UpdateTemplateData(txcontxt.destIn, ptr->GetTemplateData()))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool CBlockBase::UpdatePledge(const uint256& hashBlock, const CBlockEx& block)
{
    map<CDestination, pair<CDestination, int64>> mapBlockPledge; // pledge address, pow address
    for (size_t i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        const CTxContxt& txcontxt = block.vTxContxt[i];
        if (tx.nType == CTransaction::TX_STAKE)
        {
            StdDebug("CBlockBase", "Update Pledge: Distribute reward tx: height: %d, reward: %ld, dest: %s, tx: %s",
                     block.GetBlockHeight(), tx.nAmount, CAddress(tx.sendTo).ToString().c_str(), tx.GetHash().GetHex().c_str());
        }
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
                if (!dbBlock.RetrieveTemplateData(tx.sendTo, vTemplateData))
                {
                    StdLog("CBlockBase", "Update Pledge: sendTo RetrieveTemplateData fail");
                    return false;
                }
                ptr = CTemplate::CreateTemplatePtr(TEMPLATE_MINTPLEDGE, vTemplateData);
                if (!ptr)
                {
                    StdLog("CBlockBase", "Update Pledge: sendTo CreateTemplatePtr fail");
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
                    StdLog("CBlockBase", "Update Pledge: destIn sendTo CreateTemplatePtr fail");
                    return false;
                }
                set<CDestination> setSubDest;
                if (!ptr->GetSignDestination(tx, uint256(), 0, tx.vchSig, setSubDest, vDestInData))
                {
                    StdLog("CBlockBase", "Update Pledge: destIn GetSignDestination fail");
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
                if (!dbBlock.RetrieveTemplateData(txcontxt.destIn, vTemplateData))
                {
                    StdLog("CBlockBase", "Update Pledge: destIn RetrieveTemplateData fail");
                    return false;
                }
                ptr = CTemplate::CreateTemplatePtr(TEMPLATE_MINTPLEDGE, vTemplateData);
                if (!ptr)
                {
                    StdLog("CBlockBase", "Update Pledge: destIn CreateTemplatePtr fail");
                    return false;
                }
            }
            auto pledge = boost::dynamic_pointer_cast<CTemplateMintPledge>(ptr);
            auto& md = mapBlockPledge[txcontxt.destIn];
            md.first = pledge->destPowMint;
            md.second -= (tx.nAmount + tx.nTxFee);
        }
    }
    if (!dbBlock.AddBlockPledge(hashBlock, block.hashPrev, mapBlockPledge))
    {
        StdError("CBlockBase", "Update Pledge: Add block pledge fail");
        return false;
    }
    return true;
}

bool CBlockBase::UpdateRedeem(const uint256& hashBlock, const CBlockEx& block)
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
    if (!dbBlock.AddBlockRedeem(hashBlock, block.hashPrev, vTxRedeem))
    {
        StdError("CBlockBase", "Update Redeem: Add block redeem fail");
        return false;
    }
    return true;
}

void CBlockBase::ClearCache()
{
    map<uint256, CBlockIndex*>::iterator mi;
    for (mi = mapIndex.begin(); mi != mapIndex.end(); ++mi)
    {
        delete (*mi).second;
    }
    mapIndex.clear();
    mapForkHeightIndex.clear();
    mapFork.clear();
}

bool CBlockBase::LoadDB()
{
    CWriteLock wlock(rwAccess);

    ClearCache();
    CBlockWalker walker(this);
    if (!dbBlock.WalkThroughBlock(walker))
    {
        StdLog("CBlockBase", "LoadDB: WalkThroughBlock fail");
        ClearCache();
        return false;
    }

    vector<pair<uint256, uint256>> vFork;
    if (!dbBlock.ListFork(vFork))
    {
        StdLog("CBlockBase", "LoadDB: ListFork fail");
        ClearCache();
        return false;
    }
    for (int i = 0; i < vFork.size(); i++)
    {
        CBlockIndex* pIndex = GetIndex(vFork[i].second);
        if (pIndex == nullptr)
        {
            StdLog("CBlockBase", "LoadDB: GetIndex fail, forkid: %s, lastblock: %s", vFork[i].first.GetHex().c_str(), vFork[i].second.GetHex().c_str());
            ClearCache();
            return false;
        }
        CProfile profile;
        if (!LoadForkProfile(pIndex->pOrigin, profile))
        {
            StdLog("CBlockBase", "LoadDB: LoadForkProfile fail, forkid: %s", vFork[i].first.GetHex().c_str());
            return false;
        }
        boost::shared_ptr<CBlockFork> spFork = AddNewFork(profile, pIndex);
        if (spFork == nullptr)
        {
            StdLog("CBlockBase", "LoadDB: AddNewFork fail, forkid: %s", vFork[i].first.GetHex().c_str());
            return false;
        }
    }

    return true;
}

bool CBlockBase::SetupLog(const path& pathLocation, bool fDebug)
{

    if (!log.SetLogFilePath((pathLocation / LOGFILE_NAME).string()))
    {
        return false;
    }
    fDebugLog = fDebug;
    return true;
}

} // namespace storage
} // namespace minemon
