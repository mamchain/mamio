// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockchain.h"

#include "template/mintpledge.h"
#include "template/proof.h"

using namespace std;
using namespace xengine;

#define ENROLLED_CACHE_COUNT (120)
#define AGREEMENT_CACHE_COUNT (16)

namespace minemon
{

//////////////////////////////
// CBlockChain

CBlockChain::CBlockChain()
{
    pCoreProtocol = nullptr;
    pTxPool = nullptr;
}

CBlockChain::~CBlockChain()
{
}

bool CBlockChain::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
    {
        Error("Failed to request txpool");
        return false;
    }

    InitCheckPoints();

    return true;
}

void CBlockChain::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pTxPool = nullptr;
}

bool CBlockChain::HandleInvoke()
{
    if (!cntrBlock.Initialize(Config()->pathData, Config()->fDebug))
    {
        Error("Failed to initialize container");
        return false;
    }

    if (cntrBlock.IsEmpty())
    {
        CBlock block;
        pCoreProtocol->GetGenesisBlock(block);
        if (!InsertGenesisBlock(block))
        {
            Error("Failed to create genesis block");
            return false;
        }
    }

    // Check local block compared to checkpoint
    if (Config()->nMagicNum == MAINNET_MAGICNUM)
    {
        CBlock block;
        if (!FindPreviousCheckPointBlock(block))
        {
            StdError("BlockChain", "Find CheckPoint Error when the node starting, you should purge data(minemon -purge) to resync blockchain");
            return false;
        }
    }

    return true;
}

void CBlockChain::HandleHalt()
{
    cntrBlock.Deinitialize();
}

void CBlockChain::GetForkStatus(map<uint256, CForkStatus>& mapForkStatus)
{
    mapForkStatus.clear();

    multimap<int, CBlockIndex*> mapForkIndex;
    cntrBlock.ListForkIndex(mapForkIndex);
    for (multimap<int, CBlockIndex*>::iterator it = mapForkIndex.begin(); it != mapForkIndex.end(); ++it)
    {
        CBlockIndex* pIndex = (*it).second;
        int nForkHeight = (*it).first;
        uint256 hashFork = pIndex->GetOriginHash();
        uint256 hashParent = pIndex->GetParentHash();

        if (hashParent != 0)
        {
            mapForkStatus[hashParent].mapSubline.insert(make_pair(nForkHeight, hashFork));
        }

        map<uint256, CForkStatus>::iterator mi = mapForkStatus.insert(make_pair(hashFork, CForkStatus(hashFork, hashParent, nForkHeight))).first;
        CForkStatus& status = (*mi).second;
        status.hashLastBlock = pIndex->GetBlockHash();
        status.nLastBlockTime = pIndex->GetBlockTime();
        status.nLastBlockHeight = pIndex->GetBlockHeight();
        status.nMoneySupply = pIndex->GetMoneySupply();
        status.nMintType = pIndex->nMintType;
    }
}

bool CBlockChain::GetForkProfile(const uint256& hashFork, CProfile& profile)
{
    return cntrBlock.RetrieveProfile(hashFork, profile);
}

bool CBlockChain::GetForkContext(const uint256& hashFork, CForkContext& ctxt)
{
    return cntrBlock.RetrieveForkContext(hashFork, ctxt);
}

bool CBlockChain::GetForkAncestry(const uint256& hashFork, vector<pair<uint256, uint256>> vAncestry)
{
    return cntrBlock.RetrieveAncestry(hashFork, vAncestry);
}

int CBlockChain::GetBlockCount(const uint256& hashFork)
{
    int nCount = 0;
    CBlockIndex* pIndex = nullptr;
    if (cntrBlock.RetrieveFork(hashFork, &pIndex))
    {
        while (pIndex != nullptr)
        {
            pIndex = pIndex->pPrev;
            ++nCount;
        }
    }
    return nCount;
}

bool CBlockChain::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex))
    {
        return false;
    }
    hashFork = pIndex->GetOriginHash();
    nHeight = pIndex->GetBlockHeight();
    return true;
}

bool CBlockChain::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight, uint256& hashNext)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex))
    {
        return false;
    }
    hashFork = pIndex->GetOriginHash();
    nHeight = pIndex->GetBlockHeight();
    if (pIndex->pNext != nullptr)
    {
        hashNext = pIndex->pNext->GetBlockHash();
    }
    else
    {
        hashNext = 0;
    }
    return true;
}

bool CBlockChain::GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex) || pIndex->GetBlockHeight() < nHeight)
    {
        return false;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() > nHeight)
    {
        pIndex = pIndex->pPrev;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() == nHeight && pIndex->IsExtended())
    {
        pIndex = pIndex->pPrev;
    }
    hashBlock = !pIndex ? uint64(0) : pIndex->GetBlockHash();
    return (pIndex != nullptr);
}

bool CBlockChain::GetBlockHash(const uint256& hashFork, int nHeight, vector<uint256>& vBlockHash)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex) || pIndex->GetBlockHeight() < nHeight)
    {
        return false;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() > nHeight)
    {
        pIndex = pIndex->pPrev;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() == nHeight)
    {
        vBlockHash.push_back(pIndex->GetBlockHash());
        pIndex = pIndex->pPrev;
    }
    std::reverse(vBlockHash.begin(), vBlockHash.end());
    return (!vBlockHash.empty());
}

bool CBlockChain::GetLastBlockOfHeight(const uint256& hashFork, const int nHeight, uint256& hashBlock, int64& nTime)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex) || pIndex->GetBlockHeight() < nHeight)
    {
        return false;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() > nHeight)
    {
        pIndex = pIndex->pPrev;
    }
    if (pIndex == nullptr || pIndex->GetBlockHeight() != nHeight)
    {
        return false;
    }

    hashBlock = pIndex->GetBlockHash();
    nTime = pIndex->GetBlockTime();

    return true;
}

bool CBlockChain::GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime, uint16& nMintType)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex))
    {
        return false;
    }
    hashBlock = pIndex->GetBlockHash();
    nHeight = pIndex->GetBlockHeight();
    nTime = pIndex->GetBlockTime();
    nMintType = pIndex->nMintType;
    return true;
}

bool CBlockChain::GetLastBlockTime(const uint256& hashFork, int nDepth, vector<int64>& vTime)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex))
    {
        return false;
    }

    vTime.clear();
    while (nDepth > 0 && pIndex != nullptr)
    {
        vTime.push_back(pIndex->GetBlockTime());
        if (!pIndex->IsExtended())
        {
            nDepth--;
        }
        pIndex = pIndex->pPrev;
    }
    return true;
}

bool CBlockChain::GetBlock(const uint256& hashBlock, CBlock& block)
{
    return cntrBlock.Retrieve(hashBlock, block);
}

bool CBlockChain::GetBlockEx(const uint256& hashBlock, CBlockEx& block)
{
    return cntrBlock.Retrieve(hashBlock, block);
}

bool CBlockChain::GetOrigin(const uint256& hashFork, CBlock& block)
{
    return cntrBlock.RetrieveOrigin(hashFork, block);
}

bool CBlockChain::Exists(const uint256& hashBlock)
{
    return cntrBlock.Exists(hashBlock);
}

bool CBlockChain::GetTransaction(const uint256& txid, CTransaction& tx)
{
    return cntrBlock.RetrieveTx(txid, tx);
}

bool CBlockChain::GetTransaction(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight)
{
    return cntrBlock.RetrieveTx(txid, tx, hashFork, nHeight);
}

bool CBlockChain::ExistsTx(const uint256& txid)
{
    return cntrBlock.ExistsTx(txid);
}

bool CBlockChain::GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight)
{
    return cntrBlock.RetrieveTxLocation(txid, hashFork, nHeight);
}

bool CBlockChain::GetTxUnspent(const uint256& hashFork, const vector<CTxIn>& vInput, vector<CTxOut>& vOutput)
{
    vOutput.resize(vInput.size());
    storage::CBlockView view;
    if (!cntrBlock.GetForkBlockView(hashFork, view))
    {
        return false;
    }

    for (std::size_t i = 0; i < vInput.size(); i++)
    {
        if (vOutput[i].IsNull())
        {
            view.RetrieveUnspent(vInput[i].prevout, vOutput[i]);
        }
    }
    return true;
}

bool CBlockChain::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    return cntrBlock.FilterTx(hashFork, filter);
}

bool CBlockChain::FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter)
{
    return cntrBlock.FilterTx(hashFork, nDepth, filter);
}

bool CBlockChain::ListForkContext(vector<CForkContext>& vForkCtxt)
{
    return cntrBlock.ListForkContext(vForkCtxt);
}

Errno CBlockChain::AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt)
{
    uint256 txid = txFork.GetHash();

    CBlock block;
    CProfile profile;
    try
    {
        CBufStream ss;
        ss.Write((const char*)&txFork.vchData[0], txFork.vchData.size());
        ss >> block;
        if (!block.IsOrigin() || block.IsPrimary())
        {
            throw std::runtime_error("invalid block");
        }
        if (!profile.Load(block.vchProof))
        {
            throw std::runtime_error("invalid profile");
        }
    }
    catch (...)
    {
        Error("Invalid orign block found in tx (%s)", txid.GetHex().c_str());
        return ERR_BLOCK_INVALID_FORK;
    }
    uint256 hashFork = block.GetHash();

    CForkContext ctxtParent;
    if (!cntrBlock.RetrieveForkContext(profile.hashParent, ctxtParent))
    {
        Log("AddNewForkContext Retrieve parent context Error: %s ", profile.hashParent.ToString().c_str());
        return ERR_MISSING_PREV;
    }

    CProfile forkProfile;
    Errno err = pCoreProtocol->ValidateOrigin(block, ctxtParent.GetProfile(), forkProfile);
    if (err != OK)
    {
        Log("AddNewForkContext Validate Block Error(%s) : %s ", ErrorString(err), hashFork.ToString().c_str());
        return err;
    }

    ctxt = CForkContext(block.GetHash(), block.hashPrev, txid, profile);
    if (!cntrBlock.AddNewForkContext(ctxt))
    {
        Log("AddNewForkContext Already Exists : %s ", hashFork.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    return OK;
}

Errno CBlockChain::AddNewBlock(const CBlock& block, CBlockChainUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (cntrBlock.Exists(hash))
    {
        Log("AddNewBlock Already Exists : %s ", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        Log("AddNewBlock Validate Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        Log("AddNewBlock Retrieve Prev Index Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    err = VerifyBlock(hash, block, pIndexPrev);
    if (err != OK)
    {
        Log("AddNewBlock Verify Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    if (!VerifyRepeatMint(block))
    {
        Log("AddNewBlock repeat mint, block: %s", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    storage::CBlockView view;
    if (!cntrBlock.GetBlockView(block.hashPrev, view, !block.IsOrigin()))
    {
        Log("AddNewBlock Get Block View Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    if (!block.txMint.sendTo.IsNull())
    {
        view.AddTx(block.txMint.GetHash(), block.txMint, block.GetBlockHeight(), CTxContxt());
    }

    CBlockEx blockex(block);
    vector<CTxContxt>& vTxContxt = blockex.vTxContxt;

    if (!VerifyPowRewardTx(block, pIndexPrev->GetBlockTime()))
    {
        Log("AddNewBlock verify block mint reward fail, block: %s", hash.ToString().c_str());
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }

    vector<CTransaction> vPledgeRewardTxList;
    if (!GetDistributePledgeRewardTxList(block.hashPrev, pIndexPrev->GetBlockTime(), vPledgeRewardTxList))
    {
        Log("AddNewBlock Get distribute reward tx fail, prev block: %s", block.hashPrev.ToString().c_str());
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }

    if (block.vtx.size() > 0)
    {
        vTxContxt.reserve(block.vtx.size());
    }
    for (size_t i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        uint256 txid = tx.GetHash();
        CTxContxt txContxt;
        err = GetTxContxt(view, tx, txContxt);
        if (err != OK)
        {
            Log("AddNewBlock Get txContxt Error([%d] %s) : %s", err, ErrorString(err), txid.ToString().c_str());
            return err;
        }
        if (i < vPledgeRewardTxList.size())
        {
            if (tx != vPledgeRewardTxList[i])
            {
                Log("AddNewBlock Verify distribute pledge reward tx fail, txid: %s", txid.ToString().c_str());
                return ERR_BLOCK_TRANSACTIONS_INVALID;
            }
        }
        else
        {
            err = pCoreProtocol->VerifyBlockTx(tx, txContxt, pIndexPrev, pIndexPrev->nHeight + 1, pIndexPrev->GetOriginHash());
            if (err != OK)
            {
                Log("AddNewBlock Verify BlockTx Error(%s) : %s", ErrorString(err), txid.ToString().c_str());
                return err;
            }
        }
        if (tx.nTimeStamp > block.nTimeStamp)
        {
            Log("AddNewBlock Verify BlockTx time fail: tx time: %d, block time: %d, tx: %s, block: %s",
                tx.nTimeStamp, block.nTimeStamp, txid.ToString().c_str(), hash.GetHex().c_str());
            return ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE;
        }

        vTxContxt.push_back(txContxt);
        if (!view.AddTx(txid, tx, block.GetBlockHeight(), txContxt))
        {
            Log("AddNewBlock Add block view tx error, txid: %s", txid.ToString().c_str());
            return ERR_BLOCK_TRANSACTIONS_INVALID;
        }

        StdTrace("BlockChain", "AddNewBlock: verify tx success, new tx: %s, new block: %s", txid.GetHex().c_str(), hash.GetHex().c_str());
    }

    if (!VerifyBlockMintRedeem(blockex))
    {
        Log("AddNewBlock verify block mint redeem fail, block: %s", hash.ToString().c_str());
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }

    view.AddBlock(hash, blockex);

    // Get block trust
    uint256 nChainTrust;
    if (!pCoreProtocol->GetBlockTrust(block, nChainTrust))
    {
        Log("AddNewBlock get block trust fail, block: %s", hash.GetHex().c_str());
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }
    StdTrace("BlockChain", "AddNewBlock block chain trust: %s", nChainTrust.GetHex().c_str());

    CBlockIndex* pIndexNew;
    if (!cntrBlock.AddNew(hash, blockex, &pIndexNew, nChainTrust))
    {
        Log("AddNewBlock Storage AddNew Error : %s", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }
    Log("AddNew Block : %s", pIndexNew->ToString().c_str());

    CBlockIndex* pIndexFork = nullptr;
    if (cntrBlock.RetrieveFork(pIndexNew->GetOriginHash(), &pIndexFork)
        && (pIndexFork->nChainTrust > pIndexNew->nChainTrust
            || (pIndexFork->nChainTrust == pIndexNew->nChainTrust && !pIndexNew->IsEquivalent(pIndexFork))))
    {
        Log("AddNew Block : Short chain, new block height: %d, block type: %s, block: %s, fork chain trust: %s, fork last block: %s, fork: %s",
            pIndexNew->GetBlockHeight(), GetBlockTypeStr(block.nType, block.txMint.nType).c_str(), hash.GetHex().c_str(),
            pIndexFork->nChainTrust.GetHex().c_str(), pIndexFork->GetBlockHash().GetHex().c_str(), pIndexFork->GetOriginHash().GetHex().c_str());
        return OK;
    }

    if (!cntrBlock.CommitBlockView(view, pIndexNew))
    {
        Log("AddNewBlock Storage Commit BlockView Error : %s", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    update = CBlockChainUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    view.GetBlockChanges(update.vBlockAddNew, update.vBlockRemove);

    StdLog("BlockChain", "AddNewBlock: Commit blockchain success, height: %d, block type: %s, add block: %ld, remove block: %ld, block tx count: %ld, block: %s, fork: %s",
           block.GetBlockHeight(), GetBlockTypeStr(block.nType, block.txMint.nType).c_str(),
           update.vBlockAddNew.size(), update.vBlockRemove.size(),
           block.vtx.size(), hash.GetHex().c_str(), pIndexFork->GetOriginHash().GetHex().c_str());

    if (!update.vBlockRemove.empty())
    {
        uint32 nTxAdd = 0;
        for (const auto& b : update.vBlockAddNew)
        {
            Log("Chain rollback occur[added block]: height: %u hash: %s time: %u",
                b.GetBlockHeight(), b.GetHash().ToString().c_str(), b.nTimeStamp);
            Log("Chain rollback occur[added mint tx]: %s", b.txMint.GetHash().ToString().c_str());
            ++nTxAdd;
            for (const auto& t : b.vtx)
            {
                Log("Chain rollback occur[added tx]: %s", t.GetHash().ToString().c_str());
                ++nTxAdd;
            }
        }
        uint32 nTxDel = 0;
        for (const auto& b : update.vBlockRemove)
        {
            Log("Chain rollback occur[removed block]: height: %u hash: %s time: %u",
                b.GetBlockHeight(), b.GetHash().ToString().c_str(), b.nTimeStamp);
            Log("Chain rollback occur[removed mint tx]: %s", b.txMint.GetHash().ToString().c_str());
            ++nTxDel;
            for (const auto& t : b.vtx)
            {
                Log("Chain rollback occur[removed tx]: %s", t.GetHash().ToString().c_str());
                ++nTxDel;
            }
        }
        Log("Chain rollback occur, [height]: %u [hash]: %s "
            "[nBlockAdd]: %u [nBlockDel]: %u [nTxAdd]: %u [nTxDel]: %u",
            pIndexNew->GetBlockHeight(), pIndexNew->GetBlockHash().ToString().c_str(),
            update.vBlockAddNew.size(), update.vBlockRemove.size(), nTxAdd, nTxDel);
    }

    return OK;
}

bool CBlockChain::GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, uint32_t& nBits)
{
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(hashPrev, &pIndexPrev))
    {
        Log("Get proof of work target : Retrieve Prev Index Error: %s ", hashPrev.ToString().c_str());
        return false;
    }
    if (!pIndexPrev->IsPrimary())
    {
        Log("Get proof of work target : Previous is not primary: %s ", hashPrev.ToString().c_str());
        return false;
    }
    if (!pCoreProtocol->GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits))
    {
        Log("Get proof of work target : Unknown proof-of-work algo: %s ", hashPrev.ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockChain::GetBlockLocator(const uint256& hashFork, CBlockLocator& locator, uint256& hashDepth, int nIncStep)
{
    return cntrBlock.GetForkBlockLocator(hashFork, locator, hashDepth, nIncStep);
}

bool CBlockChain::GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, vector<uint256>& vBlockHash, size_t nMaxCount)
{
    return cntrBlock.GetForkBlockInv(hashFork, locator, vBlockHash, nMaxCount);
}

bool CBlockChain::ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent)
{
    return cntrBlock.ListForkUnspent(hashFork, dest, nMax, vUnspent);
}

bool CBlockChain::ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent)
{
    return cntrBlock.ListForkUnspentBatch(hashFork, nMax, mapUnspent);
}

bool CBlockChain::VerifyRepeatBlock(const uint256& hashFork, const CBlock& block)
{
    return cntrBlock.VerifyRepeatBlock(hashFork, block.GetBlockHeight(), block.txMint.sendTo);
}

int64 CBlockChain::GetBlockMoneySupply(const uint256& hashBlock)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex) || pIndex == nullptr)
    {
        return -1;
    }
    return pIndex->GetMoneySupply();
}

int64 CBlockChain::GetRedeemLockAmount(const CDestination& destRedeem, int64& nLastBlockBalance)
{
    uint256 hashLastBlock;
    int nLastHeight;
    int64 nLastTime;
    uint16 nLastMintType;
    if (!GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashLastBlock, nLastHeight, nLastTime, nLastMintType))
    {
        StdLog("BlockChain", "Get redeem lock amount: GetLastBlock fail, dest: %s", CAddress(destRedeem).ToString().c_str());
        return -1;
    }

    CRedeemContext redeemData;
    if (!cntrBlock.GetBlockRedeemContext(hashLastBlock, redeemData))
    {
        StdLog("BlockChain", "Get redeem lock amount: GetBlockRedeemContext fail, dest: %s", CAddress(destRedeem).ToString().c_str());
        return -1;
    }

    auto it = redeemData.mapRedeem.find(destRedeem);
    if (it == redeemData.mapRedeem.end())
    {
        StdLog("BlockChain", "Get redeem lock amount: Find dest fail, dest: %s", CAddress(destRedeem).ToString().c_str());
        return -1;
    }
    CDestRedeem& redeem = it->second;

    int nLockHeightCount = nLastHeight + 1 - redeem.nLockBeginHeight;
    if (nLockHeightCount <= 0)
    {
        StdLog("BlockChain", "Get redeem lock amount: nLockHeightCount error, nLockHeightCount: %d, dest: %s", nLockHeightCount, CAddress(destRedeem).ToString().c_str());
        return -1;
    }
    nLastBlockBalance = redeem.nBalance;

    int nRedeemDayCount = 0;
    int nRedeemDayHeight = 0;
    pCoreProtocol->GetRedeemLimitParam(nRedeemDayCount, nRedeemDayHeight);

    int64 nCurLockAmount = 0;
    if (nLockHeightCount < nRedeemDayCount * nRedeemDayHeight)
    {
        nCurLockAmount = redeem.nLockAmount - ((redeem.nLockAmount / nRedeemDayCount) * (nLockHeightCount / nRedeemDayHeight));
    }
    StdLog("BlockChain", "Get redeem lock amount: Lock amount: %f, Total lock amount: %f, Single unlock amount: %f, Unlock height: %d, Lock start height: %d",
           (double)nCurLockAmount / COIN, (double)(redeem.nLockAmount) / COIN, (double)(redeem.nLockAmount / nRedeemDayCount) / COIN, nLockHeightCount, redeem.nLockBeginHeight);
    return nCurLockAmount;
}

bool CBlockChain::VerifyBlockMintRedeem(const CBlockEx& block)
{
    CRedeemContext redeemData;
    if (block.hashPrev != 0)
    {
        if (!cntrBlock.GetBlockRedeemContext(block.hashPrev, redeemData))
        {
            StdLog("BlockChain", "Verify block mint redeem: Get block redeem context fail, prev block: %s", block.hashPrev.GetHex().c_str());
            return false;
        }
    }

    int nRedeemDayCount = 0;
    int nRedeemDayHeight = 0;
    pCoreProtocol->GetRedeemLimitParam(nRedeemDayCount, nRedeemDayHeight);

    for (size_t i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        const CTxContxt& txContxt = block.vTxContxt[i];

        if (tx.sendTo.IsTemplate() && tx.sendTo.GetTemplateId().GetType() == TEMPLATE_MINTREDEEM)
        {
            CDestRedeem& redeem = redeemData.mapRedeem[tx.sendTo];
            redeem.nBalance += tx.nAmount;
            redeem.nLockAmount = redeem.nBalance;
            redeem.nLockBeginHeight = block.GetBlockHeight();
        }
        if (txContxt.destIn.IsTemplate() && txContxt.destIn.GetTemplateId().GetType() == TEMPLATE_MINTREDEEM)
        {
            auto it = redeemData.mapRedeem.find(txContxt.destIn);
            if (it == redeemData.mapRedeem.end())
            {
                StdLog("BlockChain", "Verify block mint redeem: Find redeem dest fail, block: %s", block.GetHash().GetHex().c_str());
                return false;
            }
            CDestRedeem& redeem = it->second;
            int nLockHeightCount = block.GetBlockHeight() - redeem.nLockBeginHeight;
            if (nLockHeightCount <= 0)
            {
                StdLog("BlockChain", "Verify block mint redeem: nLockHeightCount error, block: %s", block.GetHash().GetHex().c_str());
                return false;
            }
            if (nLockHeightCount < nRedeemDayCount * nRedeemDayHeight)
            {
                int64 nCurLocakAmount = redeem.nLockAmount - ((redeem.nLockAmount / nRedeemDayCount) * (nLockHeightCount / nRedeemDayHeight));
                if (redeem.nBalance - tx.nAmount - tx.nTxFee < nCurLocakAmount)
                {
                    StdLog("BlockChain", "Verify block mint redeem: nBalance error, block: %s", block.GetHash().GetHex().c_str());
                    return false;
                }
            }
            redeem.nBalance -= (tx.nAmount + tx.nTxFee);
        }
    }
    return true;
}

bool CBlockChain::VerifyTxMintRedeem(const CTransaction& tx, const CDestination& destIn)
{
    int64 nDestTxPoolAmount = pTxPool->GetDestAmount(destIn);
    if (nDestTxPoolAmount > 0)
    {
        StdLog("BlockChain", "Verify tx mint redeem: no allow redeem, txpool amount: %f, tx: %s", (double)nDestTxPoolAmount / COIN, tx.GetHash().GetHex().c_str());
        return false;
    }

    int64 nLastBlockBalance = 0;
    int64 nLockAmount = GetRedeemLockAmount(destIn, nLastBlockBalance);
    if (nLockAmount < 0)
    {
        StdLog("BlockChain", "Verify tx mint redeem: GetRedeemLockAmount fail,tx: %s", tx.GetHash().GetHex().c_str());
        return false;
    }
    if (nLockAmount > 0)
    {
        nLastBlockBalance += nDestTxPoolAmount;
        if ((nLastBlockBalance - (tx.nAmount + tx.nTxFee)) < nLockAmount)
        {
            StdLog("BlockChain", "Verify tx mint redeem: Locked amount, balance: %f, lock amount: %f, tx amount: %f, diff: %f, tx: %s",
                   (double)nLastBlockBalance / COIN, (double)nLockAmount / COIN, (double)(tx.nAmount + tx.nTxFee) / COIN,
                   (double)((nLastBlockBalance - (tx.nAmount + tx.nTxFee)) - nLockAmount) / COIN, tx.GetHash().GetHex().c_str());
            return false;
        }
    }
    return true;
}

int64 CBlockChain::GetMintPledgeReward(const uint256& hashPrevBlock, const CDestination& destMintPow)
{
    int64 nPowMinPledge = 0;
    int64 nStakeMinPledge = 0;
    int64 nMaxPledge = 0;
    if (!pCoreProtocol->GetPledgeMinMaxValue(hashPrevBlock, nPowMinPledge, nStakeMinPledge, nMaxPledge))
    {
        StdLog("BlockChain", "Get mint pledge reward: Get pledge min max value fail, prev block: %s", hashPrevBlock.GetHex().c_str());
        return -1;
    }

    map<CDestination, int64> mapValidPledge;
    int64 nTotalPledge = 0;
    if (!cntrBlock.GetMintPledgeData(hashPrevBlock, destMintPow, nStakeMinPledge, nMaxPledge, mapValidPledge, nTotalPledge))
    {
        StdLog("BlockChain", "Get mint pledge reward: Get mint pledge fail, prev block: %s", hashPrevBlock.GetHex().c_str());
        return 0;
    }

    int64 nReward = CalcPledgeRewardValue(hashPrevBlock, nTotalPledge, nPowMinPledge, nMaxPledge);
    if (nReward < 0)
    {
        StdLog("BlockChain", "Get mint pledge reward: Calculate pledge reward value fail, prev block: %s", hashPrevBlock.GetHex().c_str());
        return -1;
    }
    return nReward;
}

bool CBlockChain::VerifyRepeatMint(const CBlock& block)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(block.hashPrev, &pIndex))
    {
        StdLog("BlockChain", "Verify repeat mint: RetrieveIndex fail, prev block: %s", block.hashPrev.GetHex().c_str());
        return false;
    }
    int nRepeatHeight = pCoreProtocol->GetRepeatMintHeight();
    for (int i = 1; pIndex && i < nRepeatHeight; i++)
    {
        if (pIndex->destMint == block.txMint.sendTo)
        {
            return false;
        }
        pIndex = pIndex->pPrev;
    }
    return true;
}

bool CBlockChain::GetPledgeTemplateParam(const CDestination& destMintPledge, CDestination& destOwner, CDestination& destPowMint, int& nRewardMode, vector<uint8>& vTemplateData)
{
    if (!destMintPledge.IsTemplate() || destMintPledge.GetTemplateId().GetType() != TEMPLATE_MINTPLEDGE)
    {
        StdLog("BlockChain", "Get pledge template param: destMintPledge error, destMintPledge: %s", CAddress(destMintPledge).ToString().c_str());
        return false;
    }

    if (!cntrBlock.RetrieveTemplateData(destMintPledge, vTemplateData))
    {
        StdLog("BlockChain", "Get pledge template param: RetrieveTemplateData fail, destMintPledge: %s", CAddress(destMintPledge).ToString().c_str());
        return false;
    }

    auto ptrPledge = CTemplate::CreateTemplatePtr(destMintPledge.GetTemplateId().GetType(), vTemplateData);
    if (ptrPledge == nullptr)
    {
        StdLog("BlockChain", "Get pledge template param: CreateTemplatePtr fail, destMintPledge: %s", CAddress(destMintPledge).ToString().c_str());
        return false;
    }
    auto objPledge = boost::dynamic_pointer_cast<CTemplateMintPledge>(ptrPledge);

    destOwner = objPledge->destOwner;
    destPowMint = objPledge->destPowMint;
    nRewardMode = objPledge->nRewardMode;

    return true;
}

bool CBlockChain::GetPowMintTemplateParam(const CDestination& destMint, CDestination& destSpent, uint32& nPledgeFee)
{
    if (!destMint.IsTemplate() || destMint.GetTemplateId().GetType() != TEMPLATE_PROOF)
    {
        StdLog("BlockChain", "Get mint template param: destMint error, destMint: %s", CAddress(destMint).ToString().c_str());
        return false;
    }

    vector<uint8> vTemplateData;
    if (!cntrBlock.RetrieveTemplateData(destMint, vTemplateData))
    {
        StdLog("BlockChain", "Get mint template param: RetrieveTemplateData fail, destMint: %s", CAddress(destMint).ToString().c_str());
        return false;
    }

    auto ptrMint = CTemplate::CreateTemplatePtr(destMint.GetTemplateId().GetType(), vTemplateData);
    if (ptrMint == nullptr)
    {
        StdLog("BlockChain", "Get mint template param: CreateTemplatePtr fail, destMint: %s", CAddress(destMint).ToString().c_str());
        return false;
    }
    auto objMint = boost::dynamic_pointer_cast<CTemplateProof>(ptrMint);

    destSpent = objMint->destSpent;
    nPledgeFee = objMint->nPledgeFee;
    return true;
}

bool CBlockChain::CalcDistributePledgeReward(const uint256& hashBlock, std::map<CDestination, int64>& mapPledgeReward)
{
    int nDistributeHeight = pCoreProtocol->GetPledgeRewardDistributeHeight();
    if ((CBlock::GetBlockHeightByHash(hashBlock) % nDistributeHeight) != 0)
    {
        StdLog("BlockChain", "calculate distribute pledge reward: height error, hashBlock: %s", hashBlock.GetHex().c_str());
        return false;
    }
    mapPledgeReward.clear();

    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex) || pIndex == nullptr)
    {
        StdLog("BlockChain", "calculate distribute pledge reward: RetrieveIndex fail, hashBlock: %s", hashBlock.GetHex().c_str());
        return false;
    }

    for (int i = 0; i < nDistributeHeight && pIndex && pIndex->pPrev; i++)
    {
        if (!CalcBlockPledgeReward(pIndex, mapPledgeReward))
        {
            StdLog("BlockChain", "calculate distribute pledge reward: Calc block pledge reward fail, hashBlock: %s", hashBlock.GetHex().c_str());
            return false;
        }
        pIndex = pIndex->pPrev;
    }
    return true;
}

bool CBlockChain::GetDistributePledgeRewardTxList(const uint256& hashPrevBlock, const uint32 nPrevBlockTime, std::vector<CTransaction>& vPledgeRewardTxList)
{
    int nDistributeHeight = pCoreProtocol->GetPledgeRewardDistributeHeight();
    if ((CBlock::GetBlockHeightByHash(hashPrevBlock) % nDistributeHeight) > 100)
    {
        StdDebug("BlockChain", "Get distribute pledge reward tx: not distribute, hashPrevBlock: %s", hashPrevBlock.GetHex().c_str());
        return true;
    }
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashPrevBlock, &pIndex) || pIndex == nullptr)
    {
        StdError("BlockChain", "Get distribute pledge reward tx: RetrieveIndex fail, hashPrevBlock: %s", hashPrevBlock.GetHex().c_str());
        return false;
    }
    while (pIndex && (pIndex->GetBlockHeight() % nDistributeHeight) > 0)
    {
        pIndex = pIndex->pPrev;
    }
    if (pIndex == nullptr)
    {
        StdError("BlockChain", "Get distribute pledge reward tx: pIndex is null, hashPrevBlock: %s", hashPrevBlock.GetHex().c_str());
        return false;
    }
    uint256 hashCalcEndBlock = pIndex->GetBlockHash();

    auto rit = mapCacheDistributePledgeReward.begin();
    while (mapCacheDistributePledgeReward.size() > 16)
    {
        if (rit->first != hashCalcEndBlock)
        {
            mapCacheDistributePledgeReward.erase(rit++);
        }
        else
        {
            ++rit;
        }
    }

    bool fCalcReward = false;
    if (mapCacheDistributePledgeReward.find(hashCalcEndBlock) == mapCacheDistributePledgeReward.end())
    {
        fCalcReward = true;
    }
    vector<vector<CTransaction>>& vRewardList = mapCacheDistributePledgeReward[hashCalcEndBlock];
    if (fCalcReward)
    {
        map<CDestination, int64> mapPledgeReward;
        if (!CalcDistributePledgeReward(hashCalcEndBlock, mapPledgeReward))
        {
            StdError("BlockChain", "Get distribute pledge reward tx: Calc distribute pledge reward fail, hashCalcEndBlock: %s", hashCalcEndBlock.GetHex().c_str());
            return false;
        }

        if (!mapPledgeReward.empty())
        {
            uint32 nSingleBlockTxCount = pCoreProtocol->CalcSingleBlockDistributePledgeRewardTxCount();
            if (nSingleBlockTxCount == 0)
            {
                StdError("BlockChain", "Get distribute pledge reward tx: CalcSingleBlockDistributePledgeRewardTxCount fail, hashPrevBlock: %s", hashPrevBlock.GetHex().c_str());
                return false;
            }
            uint32 nAddBlockCount = mapPledgeReward.size() / nSingleBlockTxCount;
            if ((mapPledgeReward.size() % nSingleBlockTxCount) > 0)
            {
                nAddBlockCount++;
            }
            StdDebug("BlockChain", "Get distribute pledge reward tx: Single block tx count: %d, Pledge reward address count: %lu, Add block count: %d",
                     nSingleBlockTxCount, mapPledgeReward.size(), nAddBlockCount);

            vRewardList.resize(nAddBlockCount);

            uint32 nAddTxCount = 0;
            for (const auto& kv : mapPledgeReward)
            {
                if (!kv.first.IsTemplate())
                {
                    StdError("BlockChain", "Get distribute pledge reward tx: address error, pledge: %s", CAddress(kv.first).ToString().c_str());
                    return false;
                }
                CTransaction txPledgeReward;
                txPledgeReward.nType = CTransaction::TX_STAKE;
                txPledgeReward.nTimeStamp = nPrevBlockTime + 1;
                if (kv.first.GetTemplateId().GetType() == TEMPLATE_MINTPLEDGE)
                {
                    CDestination destOwner;
                    CDestination destPowMint;
                    int nRewardMode = 0;
                    vector<uint8> vTemplateData;
                    if (!GetPledgeTemplateParam(kv.first, destOwner, destPowMint, nRewardMode, vTemplateData))
                    {
                        StdError("BlockChain", "Get distribute pledge reward tx: GetPledgeTemplateParam fail, pledge: %s", CAddress(kv.first).ToString().c_str());
                        return false;
                    }

                    if (nRewardMode == 1)
                    {
                        txPledgeReward.sendTo = kv.first;
                    }
                    else
                    {
                        txPledgeReward.sendTo = destOwner;
                    }
                    txPledgeReward.nAmount = kv.second;

                    xengine::CODataStream ds(txPledgeReward.vchData);
                    ds << kv.first;
                }
                else if (kv.first.GetTemplateId().GetType() == TEMPLATE_PROOF)
                {
                    txPledgeReward.sendTo = kv.first;
                    txPledgeReward.nAmount = kv.second;

                    xengine::CODataStream ds(txPledgeReward.vchData);
                    ds << kv.first;
                }
                else
                {
                    StdError("BlockChain", "Get distribute pledge reward tx: address type error, pledge: %s", CAddress(kv.first).ToString().c_str());
                    return false;
                }

                StdDebug("BlockChain", "Get distribute pledge reward tx: distribute reward tx: calc height: %d, dist height: %d, reward amount: %ld, dest: %s, tx: %s, hashPrevBlock: %s",
                         CBlock::GetBlockHeightByHash(hashCalcEndBlock), CBlock::GetBlockHeightByHash(hashCalcEndBlock) + 1 + (nAddTxCount / nSingleBlockTxCount),
                         txPledgeReward.nAmount, CAddress(txPledgeReward.sendTo).ToString().c_str(),
                         txPledgeReward.GetHash().GetHex().c_str(), hashPrevBlock.GetHex().c_str());

                vRewardList[nAddTxCount / nSingleBlockTxCount].push_back(txPledgeReward);
                nAddTxCount++;
            }
        }
    }

    int nTxListIndex = CBlock::GetBlockHeightByHash(hashPrevBlock) % nDistributeHeight;
    if (nTxListIndex < vRewardList.size())
    {
        vPledgeRewardTxList = vRewardList[nTxListIndex];

        StdDebug("BlockChain", "Get distribute pledge reward tx: distribute pledge reward, prev height: %d, reward list size: %lu, index: %d, reward tx count: %lu, hashPrevBlock: %s",
                 CBlock::GetBlockHeightByHash(hashPrevBlock), vRewardList.size(), nTxListIndex, vPledgeRewardTxList.size(), hashPrevBlock.GetHex().c_str());
    }
    return true;
}

bool CBlockChain::InsertGenesisBlock(CBlock& block)
{
    return cntrBlock.Initiate(block.GetHash(), block, uint256());
}

Errno CBlockChain::GetTxContxt(storage::CBlockView& view, const CTransaction& tx, CTxContxt& txContxt)
{
    txContxt.SetNull();
    for (const CTxIn& txin : tx.vInput)
    {
        CTxOut output;
        if (!view.RetrieveUnspent(txin.prevout, output))
        {
            Log("GetTxContxt: RetrieveUnspent fail, prevout: [%d]:%s", txin.prevout.n, txin.prevout.hash.GetHex().c_str());
            return ERR_MISSING_PREV;
        }
        if (txContxt.destIn.IsNull())
        {
            txContxt.destIn = output.destTo;
        }
        else if (txContxt.destIn != output.destTo)
        {
            Log("GetTxContxt: destIn error, destIn: %s, destTo: %s",
                txContxt.destIn.ToString().c_str(), output.destTo.ToString().c_str());
            return ERR_TRANSACTION_INVALID;
        }
        txContxt.vin.push_back(CTxInContxt(output));
    }
    return OK;
}

bool CBlockChain::GetBlockChanges(const CBlockIndex* pIndexNew, const CBlockIndex* pIndexFork,
                                  vector<CBlockEx>& vBlockAddNew, vector<CBlockEx>& vBlockRemove)
{
    while (pIndexNew != pIndexFork)
    {
        int64 nLastBlockTime = pIndexFork ? pIndexFork->GetBlockTime() : -1;
        if (pIndexNew->GetBlockTime() >= nLastBlockTime)
        {
            CBlockEx block;
            if (!cntrBlock.Retrieve(pIndexNew, block))
            {
                return false;
            }
            vBlockAddNew.push_back(block);
            pIndexNew = pIndexNew->pPrev;
        }
        else
        {
            CBlockEx block;
            if (!cntrBlock.Retrieve(pIndexFork, block))
            {
                return false;
            }
            vBlockRemove.push_back(block);
            pIndexFork = pIndexFork->pPrev;
        }
    }
    return true;
}

Errno CBlockChain::VerifyBlock(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev)
{
    if (block.IsOrigin())
    {
        return ERR_BLOCK_INVALID_FORK;
    }
    if (!block.IsPrimary())
    {
        return ERR_BLOCK_TYPE_INVALID;
    }
    if (!pIndexPrev->IsPrimary())
    {
        return ERR_BLOCK_INVALID_FORK;
    }
    return pCoreProtocol->VerifyProofOfWork(block, pIndexPrev);
}

bool CBlockChain::VerifyPowRewardTx(const CBlock& block, const uint32 nPrevBlockTime)
{
    const CTransaction& tx = block.txMint;
    const uint256 txid = tx.GetHash();

    if (tx.nLockUntil != 0)
    {
        Log("Verify pow reward tx: nLockUntil error, txid: %s", txid.ToString().c_str());
        return false;
    }
    if (!tx.vInput.empty())
    {
        Log("Verify pow reward tx: vInput is not empty, txid: %s", txid.ToString().c_str());
        return false;
    }
    if (block.GetBlockHeight() > 0)
    {
        if (tx.vchData.size() != sizeof(int64))
        {
            Log("Verify pow reward tx: vchData error, txid: %s", txid.ToString().c_str());
            return false;
        }
        CAddress addrSpent("15taqqmde43g4ndtddxwe0ajvgxcrpfjp5brs5qyp3ypmj539ffthjq4j");
        CTemplateMintPtr ptr = CTemplateMint::CreateTemplatePtr(new CTemplateProof(static_cast<CDestination&>(addrSpent), 0));
        if (ptr == nullptr)
        {
            Error("Verify pow reward tx: CreateTemplatePtr fail, txid: %s", txid.ToString().c_str());
            return false;
        }
        if (tx.vchSig.size() != ptr->GetTemplateData().size())
        {
            Log("Verify pow reward tx: vchSig error, txid: %s", txid.ToString().c_str());
            return false;
        }
    }
    if (tx.nType != CTransaction::TX_WORK)
    {
        Log("Verify pow reward tx: nType error, txid: %s", txid.ToString().c_str());
        return false;
    }
    if (tx.nTxFee != 0)
    {
        Log("Verify pow reward tx: nTxFee error, txid: %s", txid.ToString().c_str());
        return false;
    }
    if (tx.nTimeStamp != (nPrevBlockTime + 1))
    {
        Log("Verify pow reward tx: nTimeStamp error, txid: %s", txid.ToString().c_str());
        return false;
    }
    if (tx.sendTo.IsNull() || !(tx.sendTo.IsTemplate() && tx.sendTo.GetTemplateId().GetType() == TEMPLATE_PROOF))
    {
        Log("Verify pow reward tx: sendto error, txid: %s", txid.ToString().c_str());
        return false;
    }
    int64 nTotalFee = 0;
    for (const auto& ntx : block.vtx)
    {
        nTotalFee += ntx.nTxFee;
    }
    if (tx.nAmount != nTotalFee + pCoreProtocol->GetBlockPowReward(block.GetBlockHeight()))
    {
        Log("Verify pow reward tx: Pow mint reward error, txid: %s", txid.ToString().c_str());
        return false;
    }
    return true;
}

int64 CBlockChain::CalcPledgeRewardValue(const uint256& hashPrevBlock, const int64 nTotalPledge, const int64 nPowMinPledge, const int64 nMaxPledge)
{
    int64 nCalcTotalPledge = nTotalPledge;
    if (nCalcTotalPledge < nPowMinPledge)
    {
        // no pledge reward
        return 0;
    }
    if (nCalcTotalPledge > nMaxPledge)
    {
        nCalcTotalPledge = nMaxPledge;
    }

    int nPrevHeight = CBlock::GetBlockHeightByHash(hashPrevBlock);
    int64 nMoneySupply = GetBlockMoneySupply(hashPrevBlock);
    int64 nTotalReward = pCoreProtocol->GetMintTotalReward(nPrevHeight);
    int64 nSurplusReward = nTotalReward - nMoneySupply;
    if (nSurplusReward < 0)
    {
        StdLog("BlockChain", "Calculate pledge reward value: Surplus reward error, nTotalReward: %ld, nMoneySupply: %ld, prev block: %s",
               nTotalReward, nMoneySupply, hashPrevBlock.GetHex().c_str());
        return -1;
    }
    nSurplusReward += pCoreProtocol->GetBlockPledgeReward(nPrevHeight + 1);

    // R'=[T*(100%-38.2%)+S]*(D/M)
    //return (nSurplusReward * nCalcTotalPledge / nMaxPledge);
    int64 nReward = (int64)(((__uint128_t)nSurplusReward * nCalcTotalPledge) / nMaxPledge);

    StdLog("BlockChain", "CalcPledgeRewardValue: height: %d, nTotalReward: %f, nMoneySupply: %f, Surplus: %f + %f = %f, Pledge: %f / %f = %f, Reward: %f * %f = %f",
           nPrevHeight + 1, (double)nTotalReward / COIN, (double)nMoneySupply / COIN,
           (double)(nTotalReward - nMoneySupply) / COIN, (double)(pCoreProtocol->GetBlockPledgeReward(nPrevHeight + 1)) / COIN, (double)nSurplusReward / COIN,
           (double)nCalcTotalPledge / COIN, (double)nMaxPledge / COIN, ((double)nCalcTotalPledge) / ((double)nMaxPledge),
           (double)nSurplusReward / COIN, ((double)nCalcTotalPledge) / ((double)nMaxPledge), (double)nReward / COIN);
    return nReward;
}

bool CBlockChain::CalcBlockPledgeReward(const CBlockIndex* pIndex, map<CDestination, int64>& mapPledgeReward)
{
    if (pIndex == nullptr || pIndex->pPrev == nullptr)
    {
        StdLog("BlockChain", "Calculate block pledge reward: pIndex or pPrev is null");
        return false;
    }
    uint256 hashPrevBlock = pIndex->pPrev->GetBlockHash();

    int64 nPowMinPledge = 0;
    int64 nStakeMinPledge = 0;
    int64 nMaxPledge = 0;
    if (!pCoreProtocol->GetPledgeMinMaxValue(hashPrevBlock, nPowMinPledge, nStakeMinPledge, nMaxPledge))
    {
        StdLog("BlockChain", "Calculate block pledge reward: Get pledge min and max value fail, prev block: %s", hashPrevBlock.GetHex().c_str());
        return false;
    }

    // get pledge
    map<CDestination, int64> mapValidPledge;
    int64 nTotalPledge = 0;
    if (!cntrBlock.GetMintPledgeData(hashPrevBlock, pIndex->destMint, nStakeMinPledge, nMaxPledge, mapValidPledge, nTotalPledge))
    {
        StdLog("BlockChain", "Calculate block pledge reward: Get mint pledge data fail, prev block: %s", hashPrevBlock.GetHex().c_str());
        return false;
    }

    // calculate reward
    int64 nBlockPledgeReward = CalcPledgeRewardValue(hashPrevBlock, nTotalPledge, nPowMinPledge, nMaxPledge);
    if (nBlockPledgeReward < 0)
    {
        StdLog("BlockChain", "Calculate block pledge reward: Calculate pledge reward value fail, prev block: %s", hashPrevBlock.GetHex().c_str());
        return false;
    }
    if (nBlockPledgeReward == 0 || mapValidPledge.empty())
    {
        // no pledge reward
        return true;
    }

    CDestination destSpent;
    uint32 nPledgeFee = 0;
    if (!GetPowMintTemplateParam(pIndex->destMint, destSpent, nPledgeFee))
    {
        StdLog("BlockChain", "Calculate block pledge reward: Get mint param fail, destMint: %s, prev block: %s", CAddress(pIndex->destMint).ToString().c_str(), hashPrevBlock.GetHex().c_str());
        return false;
    }

    // distribute reward
    int64 nDistributeTotalReward = 0;
    for (const auto& kv : mapValidPledge)
    {
        int64 nDestReward = (int64)((__uint128_t)nBlockPledgeReward * kv.second / nTotalPledge);
        if (nPledgeFee > 0 && nPledgeFee <= 1000)
        {
            nDestReward -= (int64)((__uint128_t)nDestReward * nPledgeFee / 1000);
        }
        mapPledgeReward[kv.first] += nDestReward;
        nDistributeTotalReward += nDestReward;
    }
    if (nBlockPledgeReward - nDistributeTotalReward > 0)
    {
        mapPledgeReward[pIndex->destMint] += (nBlockPledgeReward - nDistributeTotalReward);
    }
    return true;
}

void CBlockChain::InitCheckPoints()
{
    if (Config()->nMagicNum == MAINNET_MAGICNUM)
    {
#ifdef MINEMON_TESTNET
        vecCheckPoints.push_back(CCheckPoint(0, pCoreProtocol->GetGenesisBlockHash()));
#else
        // vecCheckPoints.assign(
        //     { { 0, uint256("00000000b0a9be545f022309e148894d1e1c853ccac3ef04cb6f5e5c70f41a70") },
        //       { 100, uint256("000000649ec479bb9944fb85905822cb707eb2e5f42a5d58e598603b642e225d") },
        //       { 1000, uint256("000003e86cc97e8b16aaa92216a66c2797c977a239bbd1a12476bad68580be73") },
        //       { 2000, uint256("000007d07acd442c737152d0cd9d8e99b6f0177781323ccbe20407664e01da8f") },
        //       { 5000, uint256("00001388dbb69842b373352462b869126b9fe912b4d86becbb3ad2bf1d897840") },
        //       { 10000, uint256("00002710c3f3cd6c931f568169c645e97744943e02b0135aae4fcb3139c0fa6f") },
        //       { 16000, uint256("00003e807c1e13c95e8601d7e870a1e13bc708eddad137a49ba6c0628ce901df") },
        //       { 23000, uint256("000059d889977b9d0cd3d3fa149aa4c6e9c9da08c05c016cb800d52b2ecb620c") },
        //       { 31000, uint256("000079188913bbe13cb3ff76df2ba2f9d2180854750ab9a37dc8d197668d2215") },
        //       { 40000, uint256("00009c40c22952179a522909e8bec05617817952f3b9aebd1d1e096413fead5b") },
        //       { 50000, uint256("0000c3506e5e7fae59bee39965fb45e284f86c993958e5ce682566810832e7e8") },
        //       { 70000, uint256("000111701e15e979b4633e45b762067c6369e6f0ca8284094f6ce476b10f50de") },
        //       { 90000, uint256("00015f902819ebe9915f30f0faeeb08e7cd063b882d9066af898a1c67257927c") },
        //       { 110000, uint256("0001adb06ed43e55b0f960a212590674c8b10575de7afa7dc0bb0e53e971f21b") },
        //       { 130000, uint256("0001fbd054458ec9f75e94d6779def1ee6c6d009dbbe2f7759f5c6c75c4f9630") },
        //       { 150000, uint256("000249f070fe5b5fcb1923080c5dcbd78a6f31182ae32717df84e708b225370b") },
        //       { 170000, uint256("00029810ac925d321a415e2fb83d703dcb2ebb2d42b66584c3666eb5795d8ad6") },
        //       { 190000, uint256("0002e6304834d0f859658c939b77f9077073f42e91bf3f512bee644bd48180e1") },
        //       { 210000, uint256("000334508ed90eb9419392e1fce660467973d3dede5ca51f6e457517d03f2138") },
        //       { 230000, uint256("00038270812d3b2f338b5f8c9d00edfd084ae38580c6837b6278f20713ff20cc") },
        //       { 238000, uint256("0003a1b031248f0c0060fd8afd807f30ba34f81b6fcbbe84157e380d2d7119bc") },
        //       { 285060, uint256("00045984ae81f672b42525e0465dd05239c742fe0b6723a15c4fd03215362eae") } });
        vecCheckPoints.push_back(CCheckPoint(0, pCoreProtocol->GetGenesisBlockHash()));
#endif
    }

    for (const auto& point : vecCheckPoints)
    {
        mapCheckPoints.insert(std::make_pair(point.nHeight, point));
    }
}

bool CBlockChain::HasCheckPoints() const
{
    return mapCheckPoints.size() > 0;
}

bool CBlockChain::GetCheckPointByHeight(int nHeight, CCheckPoint& point)
{
    if (mapCheckPoints.count(nHeight) == 0)
    {
        return false;
    }
    else
    {
        point = mapCheckPoints[nHeight];
        return true;
    }
}

std::vector<IBlockChain::CCheckPoint> CBlockChain::CheckPoints() const
{
    return vecCheckPoints;
}

IBlockChain::CCheckPoint CBlockChain::LatestCheckPoint() const
{
    if (!HasCheckPoints())
    {
        return CCheckPoint();
    }

    return vecCheckPoints.back();
}

bool CBlockChain::VerifyCheckPoint(int nHeight, const uint256& nBlockHash)
{
    if (!HasCheckPoints())
    {
        return true;
    }

    CCheckPoint point;
    if (!GetCheckPointByHeight(nHeight, point))
    {
        return true;
    }

    if (nBlockHash != point.nBlockHash)
    {
        return false;
    }

    Log("Verified checkpoint at height %d/block %s", point.nHeight, point.nBlockHash.ToString().c_str());

    return true;
}

bool CBlockChain::FindPreviousCheckPointBlock(CBlock& block)
{
    if (!HasCheckPoints())
    {
        return true;
    }

    const auto& points = CheckPoints();
    int numCheckpoints = points.size();
    for (int i = numCheckpoints - 1; i >= 0; i--)
    {
        const CCheckPoint& point = points[i];

        uint256 hashBlock;
        if (!GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), point.nHeight, hashBlock))
        {
            StdTrace("BlockChain", "CheckPoint(%d, %s) doest not exists and continuely try to get previous checkpoint",
                     point.nHeight, point.nBlockHash.ToString().c_str());

            continue;
        }

        if (hashBlock != point.nBlockHash)
        {
            StdError("BlockChain", "CheckPoint(%d, %s)  does not match block hash %s",
                     point.nHeight, point.nBlockHash.ToString().c_str(), hashBlock.ToString().c_str());
            return false;
        }

        return GetBlock(hashBlock, block);
    }

    return true;
}

} // namespace minemon
