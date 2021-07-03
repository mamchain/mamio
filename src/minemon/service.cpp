// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "service.h"

#include "event.h"

using namespace std;
using namespace xengine;

extern void Shutdown();

namespace minemon
{

//////////////////////////////
// CService

CService::CService()
  : pCoreProtocol(nullptr), pBlockChain(nullptr), pTxPool(nullptr), pDispatcher(nullptr), pWallet(nullptr), pNetwork(nullptr), pForkManager(nullptr), pNetChannel(nullptr)
{
}

CService::~CService()
{
}

bool CService::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
    {
        Error("Failed to request txpool");
        return false;
    }

    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher");
        return false;
    }

    if (!GetObject("wallet", pWallet))
    {
        Error("Failed to request wallet");
        return false;
    }

    if (!GetObject("peernet", pNetwork))
    {
        Error("Failed to request network");
        return false;
    }

    if (!GetObject("forkmanager", pForkManager))
    {
        Error("Failed to request forkmanager");
        return false;
    }

    if (!GetObject("netchannel", pNetChannel))
    {
        Error("Failed to request netchannel");
        return false;
    }

    return true;
}

void CService::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pTxPool = nullptr;
    pDispatcher = nullptr;
    pWallet = nullptr;
    pNetwork = nullptr;
    pForkManager = nullptr;
    pNetChannel = nullptr;
}

bool CService::HandleInvoke()
{
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwForkStatus);
        pBlockChain->GetForkStatus(mapForkStatus);
    }
    return true;
}

void CService::HandleHalt()
{
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwForkStatus);
        mapForkStatus.clear();
    }
}

void CService::NotifyBlockChainUpdate(const CBlockChainUpdate& update)
{
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwForkStatus);
        map<uint256, CForkStatus>::iterator it = mapForkStatus.find(update.hashFork);
        if (it == mapForkStatus.end())
        {
            it = mapForkStatus.insert(make_pair(update.hashFork, CForkStatus(update.hashFork, update.hashParent, update.nOriginHeight))).first;
            if (update.hashParent != 0)
            {
                mapForkStatus[update.hashParent].mapSubline.insert(make_pair(update.nOriginHeight, update.hashFork));
            }
        }

        CForkStatus& status = (*it).second;
        status.hashLastBlock = update.hashLastBlock;
        status.nLastBlockTime = update.nLastBlockTime;
        status.nLastBlockHeight = update.nLastBlockHeight;
        status.nMoneySupply = update.nMoneySupply;
        status.nMintType = update.nLastMintType;
    }
}

void CService::NotifyNetworkPeerUpdate(const CNetworkPeerUpdate& update)
{
    (void)update;
}

void CService::NotifyTransactionUpdate(const CTransactionUpdate& update)
{
    (void)update;
}

void CService::Stop()
{
    Shutdown();
}

int CService::GetPeerCount()
{
    CEventPeerNetGetCount eventGetPeerCount(0);
    if (pNetwork->DispatchEvent(&eventGetPeerCount))
    {
        return eventGetPeerCount.result;
    }
    return 0;
}

void CService::GetPeers(vector<network::CBbPeerInfo>& vPeerInfo)
{
    vPeerInfo.clear();
    CEventPeerNetGetPeers eventGetPeers(0);
    if (pNetwork->DispatchEvent(&eventGetPeers))
    {
        vPeerInfo.reserve(eventGetPeers.result.size());
        for (unsigned int i = 0; i < eventGetPeers.result.size(); i++)
        {
            vPeerInfo.push_back(static_cast<network::CBbPeerInfo&>(eventGetPeers.result[i]));
        }
    }
}

bool CService::AddNode(const CNetHost& node)
{
    CEventPeerNetAddNode eventAddNode(0);
    eventAddNode.data = node;
    return pNetwork->DispatchEvent(&eventAddNode);
}

bool CService::RemoveNode(const CNetHost& node)
{
    CEventPeerNetRemoveNode eventRemoveNode(0);
    eventRemoveNode.data = node;
    return pNetwork->DispatchEvent(&eventRemoveNode);
}

int CService::GetForkCount()
{
    return mapForkStatus.size();
}

bool CService::HaveFork(const uint256& hashFork)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);

    map<uint256, CForkStatus>::iterator it = mapForkStatus.find(hashFork);
    if (it != mapForkStatus.end())
    {
        return true;
    }

    return false;
}

int CService::GetForkHeight(const uint256& hashFork)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);

    map<uint256, CForkStatus>::iterator it = mapForkStatus.find(hashFork);
    if (it != mapForkStatus.end())
    {
        return ((*it).second.nLastBlockHeight);
    }
    return 0;
}

void CService::ListFork(std::vector<std::pair<uint256, CProfile>>& vFork, bool fAll)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);
    if (fAll)
    {
        vector<uint256> vForkHash;
        pForkManager->GetForkList(vForkHash);
        vFork.reserve(vForkHash.size());
        for (vector<uint256>::iterator it = vForkHash.begin(); it != vForkHash.end(); ++it)
        {
            CForkContext ctx;
            if (pBlockChain->GetForkContext(*it, ctx))
            {
                vFork.push_back(make_pair(*it, ctx.GetProfile()));
            }
        }
    }
    else
    {
        vFork.reserve(mapForkStatus.size());
        for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
        {
            CProfile profile;
            if (pBlockChain->GetForkProfile((*it).first, profile))
            {
                vFork.push_back(make_pair((*it).first, profile));
            }
        }
    }
}

bool CService::GetForkGenealogy(const uint256& hashFork, vector<pair<uint256, int>>& vAncestry, vector<pair<int, uint256>>& vSubline)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);

    uint256 hashParent, hashJoint;
    int nJointHeight;
    if (!pForkManager->GetJoint(hashFork, hashParent, hashJoint, nJointHeight))
    {
        return false;
    }

    while (hashParent != 0)
    {
        vAncestry.push_back(make_pair(hashParent, nJointHeight));
        pForkManager->GetJoint(hashParent, hashParent, hashJoint, nJointHeight);
    }

    pForkManager->GetSubline(hashFork, vSubline);
    return true;
}

bool CService::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight)
{
    return pBlockChain->GetBlockLocation(hashBlock, hashFork, nHeight);
}

int CService::GetBlockCount(const uint256& hashFork)
{
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        return (GetForkHeight(hashFork) + 1);
    }
    return pBlockChain->GetBlockCount(hashFork);
}

bool CService::GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock)
{
    if (nHeight < 0)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);
        map<uint256, CForkStatus>::iterator it = mapForkStatus.find(hashFork);
        if (it == mapForkStatus.end())
        {
            return false;
        }
        hashBlock = (*it).second.hashLastBlock;
        return true;
    }
    return pBlockChain->GetBlockHash(hashFork, nHeight, hashBlock);
}

bool CService::GetBlockHash(const uint256& hashFork, int nHeight, vector<uint256>& vBlockHash)
{
    return pBlockChain->GetBlockHash(hashFork, nHeight, vBlockHash);
}

bool CService::GetBlock(const uint256& hashBlock, CBlock& block, uint256& hashFork, int& nHeight)
{
    return pBlockChain->GetBlock(hashBlock, block)
           && pBlockChain->GetBlockLocation(hashBlock, hashFork, nHeight);
}

bool CService::GetBlockEx(const uint256& hashBlock, CBlockEx& block, uint256& hashFork, int& nHeight)
{
    return pBlockChain->GetBlockEx(hashBlock, block)
           && pBlockChain->GetBlockLocation(hashBlock, hashFork, nHeight);
}

bool CService::GetLastBlockOfHeight(const uint256& hashFork, const int nHeight, uint256& hashBlock, int64& nTime)
{
    return pBlockChain->GetLastBlockOfHeight(hashFork, nHeight, hashBlock, nTime);
}

void CService::GetTxPool(const uint256& hashFork, vector<pair<uint256, size_t>>& vTxPool)
{
    vTxPool.clear();
    pTxPool->ListTx(hashFork, vTxPool);
}

bool CService::GetTransaction(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight, uint256& hashBlock, CDestination& destIn)
{
    CAssembledTx txTemp;
    if (pTxPool->Get(txid, txTemp))
    {
        int nAnchorHeight;
        if (!pBlockChain->GetBlockLocation(/*txTemp.hashAnchor*/ pCoreProtocol->GetGenesisBlockHash(), hashFork, nAnchorHeight))
        {
            //StdLog("CService", "GetTransaction: BlockChain GetBlockLocation fail, txid: %s, hashAnchor: %s", txid.GetHex().c_str(), txTemp.hashAnchor.GetHex().c_str());
            return false;
        }
        tx = txTemp;
        nHeight = -1;
        destIn = txTemp.destIn;
    }
    else
    {
        if (!pBlockChain->GetTransaction(txid, tx, hashFork, nHeight))
        {
            StdLog("CService", "GetTransaction: BlockChain GetTransaction fail, txid: %s", txid.GetHex().c_str());
            return false;
        }

        std::vector<uint256> vHashBlock;
        if (!GetBlockHash(hashFork, nHeight, vHashBlock))
        {
            StdLog("CService", "GetTransaction: GetBlockHash fail, txid: %s, fork: %s, height: %d",
                   txid.GetHex().c_str(), hashFork.GetHex().c_str(), nHeight);
            return false;
        }
        for (const auto& hash : vHashBlock)
        {
            CBlockEx block;
            uint256 tempHashFork;
            int tempHeight = 0;
            if (!GetBlockEx(hash, block, tempHashFork, tempHeight))
            {
                StdLog("CService", "GetTransaction: GetBlockEx fail, txid: %s, block: %s",
                       txid.GetHex().c_str(), hash.GetHex().c_str());
                return false;
            }
            if (txid == block.txMint.GetHash())
            {
                hashBlock = hash;
                destIn.SetNull();
            }
            else
            {
                for (int i = 0; i < block.vtx.size(); i++)
                {
                    if (txid == block.vtx[i].GetHash())
                    {
                        hashBlock = hash;
                        destIn = block.vTxContxt[i].destIn;
                        break;
                    }
                }
            }
            if (hashBlock != 0)
            {
                break;
            }
        }
    }
    return true;
}

Errno CService::SendTransaction(CTransaction& tx)
{
    return pDispatcher->AddNewTx(tx);
}

bool CService::RemovePendingTx(const uint256& txid)
{
    if (!pTxPool->Exists(txid))
    {
        return false;
    }
    pTxPool->Pop(txid);
    return true;
}

bool CService::ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent)
{
    std::vector<CTxUnspent> vUnspentOnChain;
    if (pBlockChain->ListForkUnspent(hashFork, dest, nMax, vUnspentOnChain) && pTxPool->ListForkUnspent(hashFork, dest, nMax, vUnspentOnChain, vUnspent))
    {
        return true;
    }

    return false;
}

bool CService::ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent)
{
    std::map<CDestination, std::vector<CTxUnspent>> mapUnspentOnChain(mapUnspent);
    if (pBlockChain->ListForkUnspentBatch(hashFork, nMax, mapUnspentOnChain) && pTxPool->ListForkUnspentBatch(hashFork, nMax, mapUnspentOnChain, mapUnspent))
    {
        return true;
    }

    return false;
}

bool CService::HaveKey(const crypto::CPubKey& pubkey, const int32 nVersion)
{
    return pWallet->Have(pubkey, nVersion);
}

void CService::GetPubKeys(set<crypto::CPubKey>& setPubKey)
{
    pWallet->GetPubKeys(setPubKey);
}

bool CService::GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime, bool& fPublic)
{
    return pWallet->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime, fPublic);
}

boost::optional<std::string> CService::MakeNewKey(const crypto::CCryptoString& strPassphrase, crypto::CPubKey& pubkey)
{
    crypto::CKey key;
    if (!key.Renew())
    {
        return std::string("Renew Key Failed");
    }
    if (!strPassphrase.empty())
    {
        if (!key.Encrypt(strPassphrase))
        {
            return std::string("Encrypt Key failed");
        }
        key.Lock();
    }
    pubkey = key.GetPubKey();
    return pWallet->AddKey(key);
}

boost::optional<std::string> CService::AddKey(const crypto::CKey& key)
{
    return pWallet->AddKey(key);
}

bool CService::ImportKey(const vector<unsigned char>& vchKey, crypto::CPubKey& pubkey)
{
    return pWallet->Import(vchKey, pubkey);
}

bool CService::ExportKey(const crypto::CPubKey& pubkey, vector<unsigned char>& vchKey)
{
    return pWallet->Export(pubkey, vchKey);
}

bool CService::EncryptKey(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
                          const crypto::CCryptoString& strCurrentPassphrase)
{
    return pWallet->Encrypt(pubkey, strPassphrase, strCurrentPassphrase);
}

bool CService::Lock(const crypto::CPubKey& pubkey)
{
    return pWallet->Lock(pubkey);
}

bool CService::Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout)
{
    return pWallet->Unlock(pubkey, strPassphrase, nTimeout);
}

bool CService::SignSignature(const crypto::CPubKey& pubkey, const uint256& hash, vector<unsigned char>& vchSig)
{
    return pWallet->Sign(pubkey, hash, vchSig);
}

bool CService::SignTransaction(CTransaction& tx, const vector<uint8>& vchSendToData, const vector<uint8>& vchSignExtraData, bool& fCompleted)
{
    vector<CTxOut> vUnspent;
    if (!pTxPool->FetchInputs(pCoreProtocol->GetGenesisBlockHash(), tx, vUnspent) || vUnspent.empty())
    {
        StdError("CService", "Sign transaction: FetchInputs fail or vUnspent is empty, txid: %s", tx.GetHash().GetHex().c_str());
        return false;
    }

    const CDestination& destIn = vUnspent[0].destTo;
    int32 nForkHeight = GetForkHeight(pCoreProtocol->GetGenesisBlockHash());
    if (!pWallet->SignTransaction(destIn, tx, vchSendToData, vchSignExtraData, pCoreProtocol->GetGenesisBlockHash(), nForkHeight, fCompleted))
    {
        StdError("CService", "Sign transaction: Sign transaction fail, txid: %s, destIn: %s", tx.GetHash().GetHex().c_str(), destIn.ToString().c_str());
        return false;
    }

    if (!fCompleted
        || (pCoreProtocol->ValidateTransaction(tx, nForkHeight) == OK
            /*&& pCoreProtocol->VerifyTransaction(tx, vUnspent, nForkHeight, pCoreProtocol->GetGenesisBlockHash()) == OK*/))
    {
        return true;
    }
    StdError("CService", "Sign transaction: ValidateTransaction fail, txid: %s, destIn: %s", tx.GetHash().GetHex().c_str(), destIn.ToString().c_str());
    return false;
}

bool CService::HaveTemplate(const CTemplateId& tid)
{
    return pWallet->Have(tid);
}

void CService::GetTemplateIds(set<CTemplateId>& setTid)
{
    pWallet->GetTemplateIds(setTid);
}

bool CService::AddTemplate(CTemplatePtr& ptr)
{
    return pWallet->AddTemplate(ptr);
}

CTemplatePtr CService::GetTemplate(const CTemplateId& tid)
{
    return pWallet->GetTemplate(tid);
}

CTemplatePtr CService::GetDbTemplate(const CTemplateId& tid)
{
    CDestination dest(tid);
    std::vector<uint8> vTemplateData;
    if (!pBlockChain->GetDbTemplateData(dest, vTemplateData))
    {
        StdLog("CService", "Get db template data: Get db template data fail, dest: %s", CAddress(dest).ToString().c_str());
        return nullptr;
    }
    CTemplatePtr ptr = CTemplate::CreateTemplatePtr(tid.GetType(), vTemplateData);
    if (ptr == nullptr)
    {
        StdLog("CService", "Get db template data: CreateTemplatePtr fail, dest: %s", CAddress(dest).ToString().c_str());
        return nullptr;
    }
    return ptr;
}

bool CService::GetBalance(const CDestination& dest, const uint256& hashFork, CWalletBalance& balance)
{
    int nForkHeight = GetForkHeight(hashFork);
    if (nForkHeight <= 0)
    {
        return false;
    }
    return pWallet->GetBalance(dest, hashFork, nForkHeight, balance);
}

bool CService::ListWalletTx(const uint256& hashFork, const CDestination& dest, int nOffset, int nCount, vector<CWalletTx>& vWalletTx)
{
    if (nOffset < 0)
    {
        nOffset = pWallet->GetTxCount() - nCount;
        if (nOffset < 0)
        {
            nOffset = 0;
        }
    }
    return pWallet->ListTx(hashFork, dest, nOffset, nCount, vWalletTx);
}

boost::optional<std::string> CService::CreateTransaction(const uint256& hashFork, const CDestination& destFrom,
                                                         const CDestination& destSendTo, int64 nAmount, int64 nTxFee,
                                                         const vector<unsigned char>& vchData, CTransaction& txNew)
{
    int nForkHeight = 0;
    txNew.SetNull();
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);
        map<uint256, CForkStatus>::iterator it = mapForkStatus.find(hashFork);
        if (it == mapForkStatus.end())
        {
            StdError("CService", "CreateTransaction: find fork fail, fork: %s", hashFork.GetHex().c_str());
            return std::string("find fork fail, fork: ") + hashFork.GetHex();
        }
        nForkHeight = it->second.nLastBlockHeight;
        //txNew.hashAnchor = hashFork;
    }
    txNew.nType = CTransaction::TX_TOKEN;
    txNew.nTimeStamp = GetNetTime();
    txNew.nLockUntil = 0;
    txNew.sendTo = destSendTo;
    txNew.nAmount = nAmount;
    txNew.nTxFee = nTxFee;
    txNew.vchData = vchData;

    return pWallet->ArrangeInputs(destFrom, hashFork, nForkHeight, txNew) ? boost::optional<std::string>{} : std::string("CWallet::ArrangeInputs failed.");
}

bool CService::SynchronizeWalletTx(const CDestination& destNew)
{
    return pWallet->SynchronizeWalletTx(destNew);
}

bool CService::ResynchronizeWalletTx()
{
    return pWallet->ResynchronizeWalletTx();
}

bool CService::SignOfflineTransaction(const CDestination& destIn, CTransaction& tx, bool& fCompleted)
{
    uint256 hashFork;
    int nHeight;
    if (!pBlockChain->GetBlockLocation(/*tx.hashAnchor*/ pCoreProtocol->GetGenesisBlockHash(), hashFork, nHeight))
    {
        //StdError("CService", "SignOfflineTransaction: GetBlockLocation fail, txid: %s, hashAnchor: %s", tx.GetHash().GetHex().c_str(), tx.hashAnchor.GetHex().c_str());
        return false;
    }

    int32 nForkHeight = GetForkHeight(hashFork);
    if (!pWallet->SignTransaction(destIn, tx, vector<uint8>(), vector<uint8>(), hashFork, nForkHeight, fCompleted))
    {
        StdError("CService", "SignOfflineTransaction: Sign transaction fail, txid: %s, destIn: %s", tx.GetHash().GetHex().c_str(), destIn.ToString().c_str());
        return false;
    }

    return true;
}

Errno CService::SendOfflineSignedTransaction(CTransaction& tx)
{
    /*uint256 hashFork;
    int nHeight;
    if (!pBlockChain->GetBlockLocation(pCoreProtocol->GetGenesisBlockHash(), hashFork, nHeight))
    {
        //StdError("CService", "SendOfflineSignedTransaction: GetBlockLocation fail,"
        //                     " txid: %s, hashAnchor: %s",
        //         tx.GetHash().GetHex().c_str(), tx.hashAnchor.GetHex().c_str());
        return FAILED;
    }
    vector<CTxOut> vUnspent;
    if (!pTxPool->FetchInputs(hashFork, tx, vUnspent) || vUnspent.empty())
    {
        StdError("CService", "SendOfflineSignedTransaction: FetchInputs fail or vUnspent"
                             " is empty, txid: %s",
                 tx.GetHash().GetHex().c_str());
        return FAILED;
    }

    int32 nForkHeight = GetForkHeight(hashFork);
    const CDestination& destIn = vUnspent[0].destTo;
    if (OK != pCoreProtocol->VerifyTransaction(tx, vUnspent, nForkHeight, hashFork))
    {
        StdError("CService", "SendOfflineSignedTransaction: VerifyTransaction fail,"
                             " txid: %s, destIn: %s",
                 tx.GetHash().GetHex().c_str(), destIn.ToString().c_str());
        return FAILED;
    }*/

    return pDispatcher->AddNewTx(tx, 0);
}

bool CService::AesEncrypt(const crypto::CPubKey& pubkeyLocal, const crypto::CPubKey& pubkeyRemote, const std::vector<uint8>& vMessage, std::vector<uint8>& vCiphertext)
{
    return pWallet->AesEncrypt(pubkeyLocal, pubkeyRemote, vMessage, vCiphertext);
}

bool CService::AesDecrypt(const crypto::CPubKey& pubkeyLocal, const crypto::CPubKey& pubkeyRemote, const std::vector<uint8>& vCiphertext, std::vector<uint8>& vMessage)
{
    return pWallet->AesDecrypt(pubkeyLocal, pubkeyRemote, vCiphertext, vMessage);
}

bool CService::GetWork(vector<unsigned char>& vchWorkData, int& nPrevBlockHeight,
                       uint256& hashPrev, uint32& nPrevTime, int& nAlgo,
                       uint32_t& nBits, const CTemplateMintPtr& templMint)
{
    CBlock block;
    block.nType = CBlock::BLOCK_PRIMARY;
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);
        map<uint256, CForkStatus>::iterator it = mapForkStatus.find(pCoreProtocol->GetGenesisBlockHash());
        if (it == mapForkStatus.end())
        {
            StdError("CService", "Get work: mapForkStatus find fail");
            return false;
        }
        hashPrev = (*it).second.hashLastBlock;
        nPrevTime = (*it).second.nLastBlockTime;
        nPrevBlockHeight = (*it).second.nLastBlockHeight;
        block.hashPrev = hashPrev;

        block.nTimeStamp = max((*it).second.nLastBlockTime + 1, GetNetTime());
    }

    nAlgo = CM_SHA256D;
    if (!pBlockChain->GetProofOfWorkTarget(block.hashPrev, nAlgo, nBits))
    {
        StdDebug("CService", "Get work: Get proof of work target fail");
        return false;
    }
    block.nBits = nBits;

    if (!GetWorkTx(templMint, nPrevTime, block))
    {
        StdError("CService", "Get work: Get work tx fail");
        return false;
    }
    block.hashMerkle = block.CalcMerkleTreeRoot();

    block.GetSerializedProofOfWorkData(vchWorkData);
    return true;
}

Errno CService::SubmitWork(const vector<unsigned char>& vchWorkData, const CTemplateMintPtr& templMint, uint256& hashBlock)
{
    if (vchWorkData.empty())
    {
        StdError("CService", "Submit work: vchWorkData is empty");
        return FAILED;
    }
    CBlock block;
    CBufStream ss;
    ss.Write((char*)vchWorkData.data(), vchWorkData.size());
    try
    {
        ss >> block.nVersion >> block.nType >> block.nTimeStamp >> block.hashPrev >> block.hashMerkle >> block.nBits >> block.vchProof;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return FAILED;
    }
    if (block.vchProof.size() > CProofOfHashWorkCompact::PROOFHASHWORK_SIZE)
    {
        StdError("CService", "proof cannot be greater than %d", CProofOfHashWorkCompact::PROOFHASHWORK_SIZE);
        return FAILED;
    }
    CBlock blockPrev;
    if (!pBlockChain->GetBlock(block.hashPrev, blockPrev))
    {
        StdError("CService", "Submit work: Get prev block fail");
        return FAILED;
    }

    if (!GetWorkTx(templMint, blockPrev.nTimeStamp, block))
    {
        StdError("CService", "Submit work: Get work tx fail");
        return FAILED;
    }

    if (block.hashMerkle != block.CalcMerkleTreeRoot())
    {
        StdError("CService", "Submit work: hashMerkle is not correct");
        return FAILED;
    }
    hashBlock = block.GetHash();

    Errno err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        StdError("CService", "Submit work: ValidateBlock fail");
        return err;
    }

    err = pDispatcher->AddNewBlock(block);
    if (err != OK)
    {
        StdError("CService", "Submit work: Dispatch::AddNewBlock fail");
        return err;
    }
    return OK;
}

bool CService::GetWorkTx(const CTemplateMintPtr& templMint, const uint32 nPrevBlockTime, CBlock& block)
{
    CDestination destMintPow(templMint->GetTemplateId());

    int64 nPledgeReward = pBlockChain->GetMintPledgeReward(block.hashPrev, destMintPow);
    if (nPledgeReward < 0)
    {
        StdError("CService", "Get work tx: GetMintPledgeReward fail");
        return false;
    }

    // tx mint
    CTransaction& txMint = block.txMint;
    txMint.nType = CTransaction::TX_WORK;
    txMint.nTimeStamp = nPrevBlockTime + 1;
    txMint.sendTo = destMintPow;
    txMint.nAmount = pCoreProtocol->GetBlockPowReward(block.GetBlockHeight());
    txMint.vchSig = templMint->GetTemplateData();

    xengine::CODataStream ds(txMint.vchData);
    ds << nPledgeReward;

    int64 nTotalTxFee = 0;
    vector<CTransaction> vtx;
    if (!pTxPool->ArrangeBlockTx(pCoreProtocol->GetGenesisBlockHash(), block.hashPrev, block.nTimeStamp, vtx, nTotalTxFee))
    {
        StdError("CService", "Get work tx: ArrangeBlockTx fail");
        return false;
    }

    CBlock blockCalcSize;
    blockCalcSize.vchProof.resize(CProofOfHashWorkCompact::PROOFHASHWORK_SIZE);
    size_t nMaxTxSize = MAX_BLOCK_SIZE - GetSerializeSize(blockCalcSize);

    size_t nSerializeSize = 0;
    nTotalTxFee = 0;

    vector<CTransaction> vPledgeRewardTxList;
    if (!pBlockChain->GetDistributePledgeRewardTxList(block.hashPrev, nPrevBlockTime, vPledgeRewardTxList))
    {
        StdError("CService", "Get work tx: Get distribute reward tx fail");
        return false;
    }
    for (const CTransaction& txPledgeReward : vPledgeRewardTxList)
    {
        nSerializeSize += xengine::GetSerializeSize(txPledgeReward);
        if (nSerializeSize > nMaxTxSize)
        {
            StdError("CService", "Get work tx: Distribute reward tx too much");
            return false;
        }
        block.vtx.push_back(txPledgeReward);
    }

    for (const CTransaction& tx : vtx)
    {
        nSerializeSize += xengine::GetSerializeSize(tx);
        if (nSerializeSize > nMaxTxSize)
        {
            break;
        }
        block.vtx.push_back(tx);
        nTotalTxFee += tx.nTxFee;
    }

    txMint.nAmount += nTotalTxFee;
    return true;
}

bool CService::GetPledgeStatus(uint256& hashBlock, const int nHeight, int64& nMinPowPledge, int64& nMaxPowPledge, int64& nMinStakePledge, int64& nTotalReward, int64& nMoneySupply, int64& SurplusReward)
{
    if (hashBlock == 0)
    {
        if (nHeight > 0)
        {
            int64 nTime = 0;
            if (!pBlockChain->GetLastBlockOfHeight(pCoreProtocol->GetGenesisBlockHash(), nHeight, hashBlock, nTime))
            {
                return false;
            }
        }
        else
        {
            int nHeight;
            int64 nTime;
            uint16 nMintType;
            if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashBlock, nHeight, nTime, nMintType))
            {
                return false;
            }
        }
    }
    if (!pCoreProtocol->GetPledgeMinMaxValue(hashBlock, nMinPowPledge, nMinStakePledge, nMaxPowPledge))
    {
        return false;
    }
    nMoneySupply = pBlockChain->GetBlockMoneySupply(hashBlock);
    nTotalReward = pCoreProtocol->GetMintTotalReward(CBlock::GetBlockHeightByHash(hashBlock));
    SurplusReward = nTotalReward - nMoneySupply;
    return true;
}

bool CService::GetAddressPledge(uint256& hashBlock, const int nHeight, const CDestination& destPledge, int64& nPledgeAmount, int& nPledgeHeight)
{
    if (hashBlock == 0)
    {
        if (nHeight > 0)
        {
            int64 nTime = 0;
            if (!pBlockChain->GetLastBlockOfHeight(pCoreProtocol->GetGenesisBlockHash(), nHeight, hashBlock, nTime))
            {
                return false;
            }
        }
        else
        {
            int nHeight;
            int64 nTime;
            uint16 nMintType;
            if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashBlock, nHeight, nTime, nMintType))
            {
                return false;
            }
        }
    }
    if (!pBlockChain->GetAddressPledgeAmount(hashBlock, destPledge, nPledgeAmount, nPledgeHeight))
    {
        return false;
    }
    return true;
}

} // namespace minemon
