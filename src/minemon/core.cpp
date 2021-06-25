// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "core.h"

#include "../common/template/dexmatch.h"
#include "../common/template/dexorder.h"
#include "../common/template/exchange.h"
#include "../common/template/mint.h"
#include "../common/template/mintpledge.h"
#include "../common/template/mintredeem.h"
#include "../common/template/proof.h"
#include "address.h"
#include "crypto.h"
#include "param.h"
#include "wallet.h"

using namespace std;
using namespace xengine;

#define DEBUG(err, ...) Debug((err), __FUNCTION__, __VA_ARGS__)

namespace minemon
{
///////////////////////////////
// CCoreProtocol

CCoreProtocol::CCoreProtocol()
{
    nProofOfWorkLowerLimit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_LOWER_LIMIT);
    nProofOfWorkLowerLimit.SetCompact(nProofOfWorkLowerLimit.GetCompact());
    nProofOfWorkUpperLimit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_UPPER_LIMIT);
    nProofOfWorkUpperLimit.SetCompact(nProofOfWorkUpperLimit.GetCompact());
    nProofOfWorkInit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_INIT);
    nProofOfWorkDifficultyInterval = PROOF_OF_WORK_DIFFICULTY_INTERVAL;

    pBlockChain = nullptr;
}

CCoreProtocol::~CCoreProtocol()
{
}

bool CCoreProtocol::HandleInitialize()
{
    CBlock block;
    GetGenesisBlock(block);
    hashGenesisBlock = block.GetHash();

    if (!GetObject("blockchain", pBlockChain))
    {
        return false;
    }
    return true;
}

Errno CCoreProtocol::Debug(const Errno& err, const char* pszFunc, const char* pszFormat, ...)
{
    string strFormat(pszFunc);
    strFormat += string(", ") + string(ErrorString(err)) + string(" : ") + string(pszFormat);
    va_list ap;
    va_start(ap, pszFormat);
    VDebug(strFormat.c_str(), ap);
    va_end(ap);
    return err;
}

const uint256& CCoreProtocol::GetGenesisBlockHash()
{
    return hashGenesisBlock;
}

/*
Address : 12dqbjzz6mann5581zqtn3w2vmqgmv8fv5mz185mx5yzpq47j662wcka3
PubKey : 8531f2906bbf2f9d16143e2dfba14de1a55bf051f5fd019552aba2e67fb96e13
Secret : 62c921d895140d83bd709caef55976cb774535f045c2c4deaafed4f0a58b5698
*/

void CCoreProtocol::GetGenesisBlock(CBlock& block)
{
    const CDestination destOwner = CDestination(minemon::crypto::CPubKey(uint256("8531f2906bbf2f9d16143e2dfba14de1a55bf051f5fd019552aba2e67fb96e13")));

    block.SetNull();

    block.nVersion = CBlock::BLOCK_VERSION;
    block.nType = CBlock::BLOCK_GENESIS;
    block.nTimeStamp = 1624604400;
    block.hashPrev = 0;

    CTransaction& tx = block.txMint;
    tx.nType = CTransaction::TX_GENESIS;
    tx.nTimeStamp = block.nTimeStamp;
    tx.sendTo = destOwner;
    tx.nAmount = BPX_INIT_REWARD_TOKEN;

    string strData("Lord chief justice calls for slimmed down bitcoin, no, it's juries. Under the guidance of the golden ratio, we mine the founding block at UTC+0 14:49:58.");
    tx.vchData.assign(strData.begin(), strData.end());

    CProfile profile;
    profile.strName = "Minemon";
    profile.strSymbol = "MAM";
    profile.destOwner = destOwner;
    profile.nAmount = tx.nAmount;
    profile.nMintReward = BPX_INIT_REWARD_TOKEN;
    profile.nMinTxFee = MIN_TX_FEE;
    profile.nHalveCycle = 0;
    profile.SetFlag(true, false, false);

    profile.Save(block.vchProof);
}

Errno CCoreProtocol::ValidateTransaction(const CTransaction& tx, int nHeight)
{
    /*if (tx.vInput.empty() && !tx.IsMintTx())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "tx vin is empty");
    }
    if (!tx.vInput.empty() && tx.IsMintTx())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "tx vin is not empty for genesis or work tx");
    }
    if (!tx.vchSig.empty() && tx.IsMintTx())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "invalid signature");
    }*/
    if (tx.IsMintTx())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "no mint tx");
    }
    if (tx.sendTo.IsNull())
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "send to null address");
    }
    if (!MoneyRange(tx.nAmount))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "amount overflow %ld", tx.nAmount);
    }

    /*if (!MoneyRange(tx.nTxFee)
        || (tx.nType != CTransaction::TX_TOKEN && tx.nTxFee != 0)
        || (tx.nType == CTransaction::TX_TOKEN
            && (tx.nTxFee < CalcMinTxFee(tx, MIN_TX_FEE))))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "txfee invalid %ld", tx.nTxFee);
    }*/

    if (tx.sendTo.IsTemplate())
    {
        CTemplateId tid;
        if (!tx.sendTo.GetTemplateId(tid))
        {
            return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "send to address invalid 1");
        }
    }

    set<CTxOutPoint> setInOutPoints;
    for (const CTxIn& txin : tx.vInput)
    {
        if (txin.prevout.IsNull() || txin.prevout.n > 1)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "prevout invalid");
        }
        if (!setInOutPoints.insert(txin.prevout).second)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "duplicate inputs");
        }
    }

    if (GetSerializeSize(tx) > MAX_TX_SIZE)
    {
        return DEBUG(ERR_TRANSACTION_OVERSIZE, "transaction size is too large: %lu", GetSerializeSize(tx));
    }

    return OK;
}

Errno CCoreProtocol::ValidateBlock(const CBlock& block)
{
    // These are checks that are independent of context
    // Only allow CBlock::BLOCK_PRIMARY type in v1.0.0
    /*if (block.nType != CBlock::BLOCK_PRIMARY)
    {
        return DEBUG(ERR_BLOCK_TYPE_INVALID, "Block type error");
    }*/
    // Check timestamp
    if (block.GetBlockTime() > GetNetTime() + MAX_CLOCK_DRIFT)
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "block time error: %ld", block.GetBlockTime());
    }

    // Validate mint tx
    if (!block.txMint.IsMintTx() /*|| ValidateTransaction(block.txMint, block.GetBlockHeight()) != OK*/)
    {
        return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "invalid mint tx");
    }

    size_t nBlockSize = GetSerializeSize(block);
    if (nBlockSize > MAX_BLOCK_SIZE)
    {
        return DEBUG(ERR_BLOCK_OVERSIZE, "size overflow size=%u vtx=%u", nBlockSize, block.vtx.size());
    }

    if (block.nType == CBlock::BLOCK_ORIGIN && !block.vtx.empty())
    {
        return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "origin block vtx is not empty");
    }

    vector<uint256> vMerkleTree;
    if (block.hashMerkle != block.BuildMerkleTree(vMerkleTree))
    {
        return DEBUG(ERR_BLOCK_TXHASH_MISMATCH, "tx merkeroot mismatched");
    }

    set<uint256> setTx;
    setTx.insert(vMerkleTree.begin(), vMerkleTree.begin() + block.vtx.size());
    if (setTx.size() != block.vtx.size())
    {
        return DEBUG(ERR_BLOCK_DUPLICATED_TRANSACTION, "duplicate tx");
    }

    for (size_t i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        if (!tx.IsMintTx())
        {
            if (ValidateTransaction(tx, block.GetBlockHeight()) != OK)
            {
                return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "invalid tx %s", tx.GetHash().GetHex().c_str());
            }
        }
    }
    return OK;
}

Errno CCoreProtocol::ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile)
{
    if (!forkProfile.Load(block.vchProof))
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "load profile error");
    }
    if (forkProfile.IsNull())
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "invalid profile");
    }
    if (!MoneyRange(forkProfile.nAmount))
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "invalid fork amount");
    }
    if (!RewardRange(forkProfile.nMintReward))
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "invalid fork reward");
    }
    if (parentProfile.IsPrivate())
    {
        if (!forkProfile.IsPrivate() || parentProfile.destOwner != forkProfile.destOwner)
        {
            return DEBUG(ERR_BLOCK_INVALID_FORK, "permission denied");
        }
    }
    return OK;
}

Errno CCoreProtocol::VerifyProofOfWork(const CBlock& block, const CBlockIndex* pIndexPrev)
{
    if (block.GetBlockTime() < pIndexPrev->GetBlockTime())
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range 1, height: %d, block time: %d, prev time: %d, block: %s.",
                     block.GetBlockHeight(), block.GetBlockTime(),
                     pIndexPrev->GetBlockTime(), block.GetHash().GetHex().c_str());
    }

    uint32 nBits = 0;
    uint8 nAlgo = 0;
    if (!GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits))
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "get target fail.");
    }

    if (nBits != block.nBits)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "algo or bits error, nAlgo: %d, nBits: %d, vchProof size: %ld.", nAlgo, block.nBits, block.vchProof.size());
    }

    bool fNegative;
    bool fOverflow;
    uint256 hashTarget;
    hashTarget.SetCompact(nBits, &fNegative, &fOverflow);

    if (fNegative || hashTarget == 0 || fOverflow || hashTarget < nProofOfWorkUpperLimit || hashTarget > nProofOfWorkLowerLimit)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "nBits error, nBits: %u, hashTarget: %s, negative: %d, overflow: %d, upper: %s, lower: %s",
                     nBits, hashTarget.ToString().c_str(), fNegative, fOverflow, nProofOfWorkUpperLimit.ToString().c_str(), nProofOfWorkLowerLimit.ToString().c_str());
    }
    if (block.vchProof.size() > CProofOfHashWorkCompact::PROOFHASHWORK_SIZE)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "proof size error: proof[%s] .", xengine::ToHexString(block.vchProof));
    }
    CProofOfHashWorkCompact proof;
    proof.Load(block.vchProof);
    if (block.nTimeStamp != proof.btcTime)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "%d{btc time} != %d{mam time}.", proof.btcTime, block.nTimeStamp);
    }
    xengine::CBufStream ss;
    ss << block.nVersion << block.nType << block.hashPrev << block.hashMerkle << nBits;
    uint256 pow_hash = minemon::crypto::CryptoSHA256(ss.GetData(), ss.GetSize());

    std::vector<uint8> coinbase_merkle_root;
    coinbase_merkle_root.insert(coinbase_merkle_root.end(), proof.coinbase.begin() + proof.auxOffset, proof.coinbase.begin() + proof.auxOffset + 32);
    std::reverse(std::begin(coinbase_merkle_root), std::end(coinbase_merkle_root));
    uint256 merkle_root(coinbase_merkle_root);
    if (!proof.VerificationMerkleTree(merkle_root, pow_hash, proof.auxMerkleBranch))
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "proof aux data error: proof[%s] .", xengine::ToHexString(block.vchProof));
    }

    uint256 coinbase_txid = minemon::crypto::CryptoSHA256D(proof.coinbase.data(), proof.coinbase.size());
    if (!proof.VerificationMerkleTree(proof.btcHashMerkleRoot, coinbase_txid, proof.trMerkleBranch))
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "proof txid data error: proof[%s] .", xengine::ToHexString(block.vchProof));
    }

    xengine::CBufStream btc_ss;
    btc_ss << proof.btcVersion << proof.btcHashPrevBlock << proof.btcHashMerkleRoot << proof.btcTime << proof.btcBits << proof.btcNonce;

    uint256 hash = minemon::crypto::CryptoSHA256D(btc_ss.GetData(), btc_ss.GetSize());
    if (hash >= hashTarget)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "hash error: proof[%s] vs.", xengine::ToHexString(block.vchProof));
    }
    return OK;
}

Errno CCoreProtocol::VerifyBlockTx(const CTransaction& tx, const CTxContxt& txContxt, CBlockIndex* pIndexPrev, int nForkHeight, const uint256& fork)
{
    if (tx.IsMintTx())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "no mint tx");
    }

    const CDestination& destIn = txContxt.destIn;
    int64 nValueIn = 0;
    for (const CTxInContxt& inctxt : txContxt.vin)
    {
        if (inctxt.nTxTime > tx.nTimeStamp)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "tx time is ahead of input tx");
        }
        if (inctxt.IsLocked(pIndexPrev->GetBlockHeight()))
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input is still locked");
        }
        nValueIn += inctxt.nAmount;
    }

    if (!MoneyRange(nValueIn))
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein invalid %ld", nValueIn);
    }
    if (!MoneyRange(tx.nTxFee)
        || (tx.nType != CTransaction::TX_TOKEN && tx.nTxFee != 0)
        || (tx.nType == CTransaction::TX_TOKEN
            && (tx.nTxFee < CalcMinTxFee(tx, nForkHeight, MIN_TX_FEE))))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "txfee invalid %ld", tx.nTxFee);
    }
    if (nValueIn < tx.nAmount + tx.nTxFee)
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough (%ld : %ld)", nValueIn, tx.nAmount + tx.nTxFee);
    }

    uint16 nToType = 0;
    uint16 nFromType = 0;

    CTemplateId tid;
    if (tx.sendTo.GetTemplateId(tid))
    {
        nToType = tid.GetType();
    }
    if (destIn.GetTemplateId(tid))
    {
        nFromType = tid.GetType();
    }

    if (nToType == TEMPLATE_MINTREDEEM && nFromType != TEMPLATE_MINTPLEDGE)
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "invalid redeem transaction");
    }

    switch (nFromType)
    {
    case TEMPLATE_DEXORDER:
    {
        Errno err = VerifyDexOrderTx(tx, destIn, nValueIn, nForkHeight);
        if (err != OK)
        {
            return DEBUG(err, "invalid dex order tx");
        }
        break;
    }
    case TEMPLATE_DEXMATCH:
    {
        Errno err = VerifyDexMatchTx(tx, nValueIn, nForkHeight);
        if (err != OK)
        {
            return DEBUG(err, "invalid dex match tx");
        }
        break;
    }
    case TEMPLATE_MINTPLEDGE:
    {
        Errno err = VerifyMintPledgeTx(tx);
        if (err != OK)
        {
            return DEBUG(err, "invalid mint pledge tx");
        }
        break;
    }
    }

    vector<uint8> vchSig;
    if (!CTemplate::VerifyDestRecorded(tx, vchSig))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid recoreded destination");
    }

    //if (destIn.IsTemplate() && destIn.GetTemplateId().GetType() == TEMPLATE_DEXMATCH
    //    && nForkHeight < MATCH_VERIFY_ERROR_HEIGHT)
    //{
    nForkHeight -= 1;
    //}

    if (!destIn.VerifyTxSignature(tx.GetSignatureHash(), tx.nType, GetGenesisBlockHash(), tx.sendTo, vchSig, nForkHeight, fork))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature");
    }

    return OK;
}

Errno CCoreProtocol::VerifyTransaction(const CTransaction& tx, const vector<CTxOut>& vPrevOutput,
                                       int nForkHeight, const uint256& fork)
{
    CDestination destIn = vPrevOutput[0].destTo;
    int64 nValueIn = 0;
    for (const CTxOut& output : vPrevOutput)
    {
        if (destIn != output.destTo)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input destination mismatched");
        }
        if (output.nTxTime > tx.nTimeStamp)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "tx time is ahead of input tx");
        }
        if (output.IsLocked(nForkHeight))
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input is still locked");
        }
        nValueIn += output.nAmount;
    }
    if (!MoneyRange(nValueIn))
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein invalid %ld", nValueIn);
    }
    if (!MoneyRange(tx.nTxFee)
        || (tx.nType != CTransaction::TX_TOKEN && tx.nTxFee != 0)
        || (tx.nType == CTransaction::TX_TOKEN
            && (tx.nTxFee < CalcMinTxFee(tx, nForkHeight + 1, MIN_TX_FEE))))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "txfee invalid %ld", tx.nTxFee);
    }
    if (nValueIn < tx.nAmount + tx.nTxFee)
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough (%ld : %ld)", nValueIn, tx.nAmount + tx.nTxFee);
    }

    uint16 nToType = 0;
    uint16 nFromType = 0;

    CTemplateId tid;
    if (tx.sendTo.GetTemplateId(tid))
    {
        nToType = tid.GetType();
    }
    if (destIn.GetTemplateId(tid))
    {
        nFromType = tid.GetType();
    }

    if (nToType == TEMPLATE_MINTREDEEM && nFromType != TEMPLATE_MINTPLEDGE)
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "invalid redeem transaction");
    }

    switch (nFromType)
    {
    case TEMPLATE_DEXORDER:
    {
        Errno err = VerifyDexOrderTx(tx, destIn, nValueIn, nForkHeight + 1);
        if (err != OK)
        {
            return DEBUG(err, "invalid dex order tx");
        }
        break;
    }
    case TEMPLATE_DEXMATCH:
    {
        Errno err = VerifyDexMatchTx(tx, nValueIn, nForkHeight + 1);
        if (err != OK)
        {
            return DEBUG(err, "invalid dex match tx");
        }
        break;
    }
    case TEMPLATE_MINTPLEDGE:
    {
        Errno err = VerifyMintPledgeTx(tx);
        if (err != OK)
        {
            return DEBUG(err, "invalid mint pledge tx");
        }
        break;
    }
    case TEMPLATE_MINTREDEEM:
    {
        if (fork != GetGenesisBlockHash())
        {
            return DEBUG(ERR_TRANSACTION_INVALID, "invalid mint redeem tx1");
        }
        if (!pBlockChain->VerifyTxMintRedeem(tx, destIn))
        {
            return DEBUG(ERR_TRANSACTION_INVALID, "invalid mint redeem tx2");
        }
        break;
    }
    }

    // record destIn in vchSig
    vector<uint8> vchSig;
    if (!CTemplate::VerifyDestRecorded(tx, vchSig))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid recoreded destination");
    }

    if (!destIn.VerifyTxSignature(tx.GetSignatureHash(), tx.nType, GetGenesisBlockHash(), tx.sendTo, vchSig, nForkHeight + 1, fork))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature");
    }

    return OK;
}

bool CCoreProtocol::GetBlockTrust(const CBlock& block, uint256& nChainTrust)
{
    if (!block.IsPrimary())
    {
        StdError("CCoreProtocol", "GetBlockTrust: block type error");
        return false;
    }

    if (!block.IsProofOfWork())
    {
        StdError("CCoreProtocol", "GetBlockTrust: IsProofOfWork fail");
        return false;
    }

    uint256 nTarget;
    nTarget.SetCompact(block.nBits);
    // nChainTrust = 2**256 / (nTarget+1) = ~nTarget / (nTarget+1) + 1
    nChainTrust = (~nTarget / (nTarget + 1)) + 1;
    return true;
}

bool CCoreProtocol::GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, uint32_t& nBits)
{
    if (pIndexPrev->GetBlockHeight() == 0)
    {
        nBits = nProofOfWorkInit.GetCompact();
    }
    else if ((pIndexPrev->nHeight + 1) % nProofOfWorkDifficultyInterval != 0)
    {
        nBits = pIndexPrev->nProofBits;
    }
    else
    {
        // statistic the sum of nProofOfWorkDifficultyInterval blocks time
        const CBlockIndex* pIndexFirst = pIndexPrev;
        for (int i = 1; i < nProofOfWorkDifficultyInterval && pIndexFirst; i++)
        {
            pIndexFirst = pIndexFirst->pPrev;
        }

        if (!pIndexFirst || pIndexFirst->GetBlockHeight() != (pIndexPrev->GetBlockHeight() - (nProofOfWorkDifficultyInterval - 1)))
        {
            StdError("CCoreProtocol", "Get proof of work target: first block of difficulty interval height is error");
            return false;
        }

        if (pIndexPrev == pIndexFirst)
        {
            StdError("CCoreProtocol", "Get proof of work target: difficulty interval must be large than 1");
            return false;
        }

        // Limit adjustment step
        if (pIndexPrev->GetBlockTime() < pIndexFirst->GetBlockTime())
        {
            StdError("CCoreProtocol", "Get proof of work target: prev block time [%ld] < first block time [%ld]", pIndexPrev->GetBlockTime(), pIndexFirst->GetBlockTime());
            return false;
        }
        uint64 nActualTimespan = pIndexPrev->GetBlockTime() - pIndexFirst->GetBlockTime();
        uint64 nTargetTimespan = (uint64)nProofOfWorkDifficultyInterval * BLOCK_TARGET_SPACING;
        if (nActualTimespan < nTargetTimespan / 4)
        {
            nActualTimespan = nTargetTimespan / 4;
        }
        if (nActualTimespan > nTargetTimespan * 4)
        {
            nActualTimespan = nTargetTimespan * 4;
        }

        uint256 newBits;
        newBits.SetCompact(pIndexPrev->nProofBits);
        // StdLog("CCoreProtocol", "newBits *= nActualTimespan, newBits: %s, nActualTimespan: %lu", newBits.ToString().c_str(), nActualTimespan);
        //newBits /= uint256(nTargetTimespan);
        // StdLog("CCoreProtocol", "newBits: %s", newBits.ToString().c_str());
        //newBits *= nActualTimespan;
        // StdLog("CCoreProtocol", "newBits /= nTargetTimespan, newBits: %s, nTargetTimespan: %lu", newBits.ToString().c_str(), nTargetTimespan);
        if (newBits >= uint256(nTargetTimespan))
        {
            newBits /= uint256(nTargetTimespan);
            newBits *= nActualTimespan;
        }
        else
        {
            newBits *= nActualTimespan;
            newBits /= uint256(nTargetTimespan);
        }
        if (newBits < nProofOfWorkUpperLimit)
        {
            newBits = nProofOfWorkUpperLimit;
        }
        if (newBits > nProofOfWorkLowerLimit)
        {
            newBits = nProofOfWorkLowerLimit;
        }
        nBits = newBits.GetCompact();
        // StdLog("CCoreProtocol", "newBits.GetCompact(), newBits: %s, nBits: %x", newBits.ToString().c_str(), nBits);
    }

    return true;
}

int64 CCoreProtocol::GetMintWorkReward(const int nHeight)
{
    if (nHeight < 1)
    {
        return 0;
    }
    return (BPX_INIT_BLOCK_REWARD_TOKEN / (int64)pow(2, (nHeight - 1) / BPX_REWARD_HALVE_HEIGHT));
}

int64 CCoreProtocol::GetBlockPowReward(const int nHeight)
{
    return GetMintWorkReward(nHeight) * BPX_POW_REWARD_RATE / BPX_TOTAL_REWARD_RATE;
}

int64 CCoreProtocol::GetBlockPledgeReward(const int nHeight)
{
    return GetMintWorkReward(nHeight) * (BPX_TOTAL_REWARD_RATE - BPX_POW_REWARD_RATE) / BPX_TOTAL_REWARD_RATE;
}

int64 CCoreProtocol::GetMintTotalReward(const int nHeight)
{
    int64 nTotalReward = 0;
    int64 nSectReward = BPX_INIT_BLOCK_REWARD_TOKEN;
    int nCalcHeight = nHeight;
    while (nCalcHeight > 0)
    {
        if (nCalcHeight < BPX_REWARD_HALVE_HEIGHT)
        {
            nTotalReward += (nSectReward * nCalcHeight);
            break;
        }
        nTotalReward += (nSectReward * BPX_REWARD_HALVE_HEIGHT);
        nSectReward /= 2;
        nCalcHeight -= BPX_REWARD_HALVE_HEIGHT;
    }
    return nTotalReward;
}

bool CCoreProtocol::GetPledgeMinMaxValue(const uint256& hashPrevBlock, int64& nPowMinPledge, int64& nStakeMinPledge, int64& nMaxPledge)
{
    if (hashPrevBlock == hashGenesisBlock)
    {
        nPowMinPledge = BPX_MIN_PLEDGE_COIN;
        nMaxPledge = nPowMinPledge * 100;
        nStakeMinPledge = BPX_INIT_BLOCK_REWARD_TOKEN;
        return true;
    }

    int64 nMoneySupply = pBlockChain->GetBlockMoneySupply(hashPrevBlock);
    if (nMoneySupply < 0)
    {
        StdError("CCoreProtocol", "GetPledgeMinMaxValue: GetBlockMoneySupply fail, prev block: %s", hashPrevBlock.GetHex().c_str());
        return false;
    }

    nPowMinPledge = nMoneySupply / 10000;
    if (nPowMinPledge < BPX_MIN_PLEDGE_COIN)
    {
        nPowMinPledge = BPX_MIN_PLEDGE_COIN;
        nMaxPledge = nPowMinPledge * 100;
    }
    else
    {
        nMaxPledge = nMoneySupply / 100;
    }

    nStakeMinPledge = 0;
    int64 nSectValue = BPX_INIT_BLOCK_REWARD_TOKEN;
    int nCalcHeight = CBlock::GetBlockHeightByHash(hashPrevBlock) + 1;
    while (nCalcHeight > 0)
    {
        nStakeMinPledge += nSectValue;
        nSectValue /= 2;
        nCalcHeight -= BPX_REWARD_HALVE_HEIGHT;
    }
    return true;
}

uint32 CCoreProtocol::CalcSingleBlockDistributePledgeRewardTxCount()
{
    static uint32 nDistributeTxCount = 0;
    if (nDistributeTxCount == 0)
    {
        CBlock block;

        CAddress addrSpent("15taqqmde43g4ndtddxwe0ajvgxcrpfjp5brs5qyp3ypmj539ffthjq4j");
        CTemplateMintPtr ptr = CTemplateMint::CreateTemplatePtr(new CTemplateProof(static_cast<CDestination&>(addrSpent), 0));
        if (ptr == nullptr)
        {
            StdError("CCoreProtocol", "Calc tx count: CreateTemplatePtr fail");
            return 0;
        }
        block.txMint.vchSig = ptr->GetTemplateData();

        int64 nPledgeReward = 0;
        xengine::CODataStream tds(block.txMint.vchData);
        tds << nPledgeReward;

        block.vchProof.resize(CProofOfHashWorkCompact::PROOFHASHWORK_SIZE);
        size_t nMaxTxSize = MAX_BLOCK_SIZE - xengine::GetSerializeSize(block);

        CTransaction txDistributePledge;
        xengine::CODataStream ds(txDistributePledge.vchData);
        ds << CDestination();
        nDistributeTxCount = (uint32)(nMaxTxSize / xengine::GetSerializeSize(txDistributePledge));
        if (nDistributeTxCount > 100)
        {
            nDistributeTxCount -= 100;
        }
    }
    return nDistributeTxCount;
}

Errno CCoreProtocol::VerifyDexOrderTx(const CTransaction& tx, const CDestination& destIn, int64 nValueIn, int nHeight)
{
    uint16 nSendToTemplateType = 0;
    if (tx.sendTo.IsTemplate())
    {
        nSendToTemplateType = tx.sendTo.GetTemplateId().GetType();
    }

    vector<uint8> vchSig;
    if (!CTemplate::VerifyDestRecorded(tx, vchSig))
    {
        return ERR_TRANSACTION_SIGNATURE_INVALID;
    }

    auto ptrOrder = CTemplate::CreateTemplatePtr(TEMPLATE_DEXORDER, vchSig);
    if (ptrOrder == nullptr)
    {
        Log("Verify dexorder tx: Create order template fail, tx: %s", tx.GetHash().GetHex().c_str());
        return ERR_TRANSACTION_SIGNATURE_INVALID;
    }
    auto objOrder = boost::dynamic_pointer_cast<CTemplateDexOrder>(ptrOrder);
    if (nSendToTemplateType == TEMPLATE_DEXMATCH)
    {
        CTemplatePtr ptrMatch = CTemplate::CreateTemplatePtr(nSendToTemplateType, tx.vchSig);
        if (!ptrMatch)
        {
            Log("Verify dexorder tx: Create match template fail, tx: %s", tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }
        auto objMatch = boost::dynamic_pointer_cast<CTemplateDexMatch>(ptrMatch);

        set<CDestination> setSubDest;
        vector<uint8> vchSubSig;
        if (!objOrder->GetSignDestination(tx, uint256(), nHeight, tx.vchSig, setSubDest, vchSubSig))
        {
            Log("Verify dexorder tx: GetSignDestination fail, tx: %s", tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }
        if (setSubDest.empty() || objMatch->destMatch != *setSubDest.begin())
        {
            Log("Verify dexorder tx: destMatch error, tx: %s", tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }

        if (objMatch->destSellerOrder != destIn)
        {
            Log("Verify dexorder tx: destSellerOrder error, tx: %s", tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }
        if (objMatch->destSeller != objOrder->destSeller)
        {
            Log("Verify dexorder tx: destSeller error, tx: %s", tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }
        /*if (objMatch->nSellerValidHeight != objOrder->nValidHeight)
        {
            Log("Verify dexorder tx: nSellerValidHeight error, match nSellerValidHeight: %d, order nValidHeight: %d, tx: %s",
                objMatch->nSellerValidHeight, objOrder->nValidHeight, tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }*/
        if ((tx.nAmount != objMatch->nMatchAmount) || (tx.nAmount < (TNS_DEX_MIN_TX_FEE * 3 + TNS_DEX_MIN_MATCH_AMOUNT)))
        {
            Log("Verify dexorder tx: nAmount error, match nMatchAmount: %lu, tx amount: %lu, tx: %s",
                objMatch->nMatchAmount, tx.nAmount, tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }
        if (objMatch->nFee != objOrder->nFee)
        {
            Log("Verify dexorder tx: nFee error, match fee: %ld, order fee: %ld, tx: %s",
                objMatch->nFee, objMatch->nFee, tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }
    }
    return OK;
}

Errno CCoreProtocol::VerifyDexMatchTx(const CTransaction& tx, int64 nValueIn, int nHeight)
{
    vector<uint8> vchSig;
    if (!CTemplate::VerifyDestRecorded(tx, vchSig))
    {
        return ERR_TRANSACTION_SIGNATURE_INVALID;
    }

    auto ptrMatch = CTemplate::CreateTemplatePtr(TEMPLATE_DEXMATCH, vchSig);
    if (ptrMatch == nullptr)
    {
        Log("Verify dex match tx: Create match template fail, tx: %s", tx.GetHash().GetHex().c_str());
        return ERR_TRANSACTION_SIGNATURE_INVALID;
    }
    auto objMatch = boost::dynamic_pointer_cast<CTemplateDexMatch>(ptrMatch);
    if (nHeight <= objMatch->nSellerValidHeight)
    {
        if (tx.sendTo == objMatch->destBuyer)
        {
            int64 nBuyerAmount = ((uint64)(objMatch->nMatchAmount - TNS_DEX_MIN_TX_FEE * 3) * (FEE_PRECISION - objMatch->nFee)) / FEE_PRECISION;
            if (nValueIn != objMatch->nMatchAmount)
            {
                Log("Verify dex match tx: Send buyer nValueIn error, nValueIn: %lu, nMatchAmount: %lu, tx: %s",
                    nValueIn, objMatch->nMatchAmount, tx.GetHash().GetHex().c_str());
                return ERR_TRANSACTION_SIGNATURE_INVALID;
            }
            if (tx.nAmount != nBuyerAmount)
            {
                Log("Verify dex match tx: Send buyer tx nAmount error, nAmount: %lu, need amount: %lu, nMatchAmount: %lu, nFee: %ld, nTxFee: %lu, tx: %s",
                    tx.nAmount, nBuyerAmount, objMatch->nMatchAmount, objMatch->nFee, tx.nTxFee, tx.GetHash().GetHex().c_str());
                return ERR_TRANSACTION_SIGNATURE_INVALID;
            }
            if (tx.nTxFee != TNS_DEX_MIN_TX_FEE)
            {
                Log("Verify dex match tx: Send buyer tx nTxFee error, nAmount: %lu, need amount: %lu, nMatchAmount: %lu, nFee: %ld, nTxFee: %lu, tx: %s",
                    tx.nAmount, nBuyerAmount, objMatch->nMatchAmount, objMatch->nFee, tx.nTxFee, tx.GetHash().GetHex().c_str());
                return ERR_TRANSACTION_SIGNATURE_INVALID;
            }
        }
        else if (tx.sendTo == objMatch->destMatch)
        {
            int64 nBuyerAmount = ((uint64)(objMatch->nMatchAmount - TNS_DEX_MIN_TX_FEE * 3) * (FEE_PRECISION - objMatch->nFee)) / FEE_PRECISION;
            int64 nRewardAmount = ((uint64)(objMatch->nMatchAmount - TNS_DEX_MIN_TX_FEE * 3) * (objMatch->nFee / 2)) / FEE_PRECISION;
            if (nValueIn != (objMatch->nMatchAmount - nBuyerAmount - TNS_DEX_MIN_TX_FEE))
            {
                Log("Verify dex match tx: Send match nValueIn error, nValueIn: %lu, need amount: %lu, nMatchAmount: %lu, nFee: %ld, nTxFee: %lu, tx: %s",
                    nValueIn, objMatch->nMatchAmount - nBuyerAmount, objMatch->nMatchAmount, objMatch->nFee, tx.nTxFee, tx.GetHash().GetHex().c_str());
                return ERR_TRANSACTION_SIGNATURE_INVALID;
            }
            if (tx.nAmount != nRewardAmount)
            {
                Log("Verify dex match tx: Send match tx nAmount error, nAmount: %lu, need amount: %lu, nMatchAmount: %lu, nRewardAmount: %lu, nFee: %ld, nTxFee: %lu, tx: %s",
                    tx.nAmount, nRewardAmount, objMatch->nMatchAmount, nRewardAmount, objMatch->nFee, tx.nTxFee, tx.GetHash().GetHex().c_str());
                return ERR_TRANSACTION_SIGNATURE_INVALID;
            }
            if (tx.nTxFee != TNS_DEX_MIN_TX_FEE)
            {
                Log("Verify dex match tx: Send match tx nTxFee error, nAmount: %lu, need amount: %lu, nMatchAmount: %lu, nRewardAmount: %lu, nFee: %ld, nTxFee: %lu, tx: %s",
                    tx.nAmount, nRewardAmount, objMatch->nMatchAmount, nRewardAmount, objMatch->nFee, tx.nTxFee, tx.GetHash().GetHex().c_str());
                return ERR_TRANSACTION_SIGNATURE_INVALID;
            }
        }
        else
        {
            set<CDestination> setSubDest;
            vector<uint8> vchSubSig;
            if (!objMatch->GetSignDestination(tx, uint256(), nHeight, tx.vchSig, setSubDest, vchSubSig))
            {
                Log("Verify dex match tx: GetSignDestination fail, tx: %s", tx.GetHash().GetHex().c_str());
                return ERR_TRANSACTION_SIGNATURE_INVALID;
            }
            if (tx.sendTo == *setSubDest.begin())
            {
                int64 nBuyerAmount = ((uint64)(objMatch->nMatchAmount - TNS_DEX_MIN_TX_FEE * 3) * (FEE_PRECISION - objMatch->nFee)) / FEE_PRECISION;
                int64 nRewardAmount = ((uint64)(objMatch->nMatchAmount - TNS_DEX_MIN_TX_FEE * 3) * (objMatch->nFee / 2)) / FEE_PRECISION;
                if (nValueIn != (objMatch->nMatchAmount - nBuyerAmount - nRewardAmount - TNS_DEX_MIN_TX_FEE * 2))
                {
                    Log("Verify dex match tx: Send deal nValueIn error, nValueIn: %lu, need amount: %lu, nMatchAmount: %lu, nRewardAmount: %lu, nFee: %ld, nTxFee: %lu, tx: %s",
                        nValueIn, objMatch->nMatchAmount - nBuyerAmount - nRewardAmount, objMatch->nMatchAmount, nRewardAmount, objMatch->nFee, tx.nTxFee, tx.GetHash().GetHex().c_str());
                    return ERR_TRANSACTION_SIGNATURE_INVALID;
                }
                if (tx.nAmount != (nValueIn - TNS_DEX_MIN_TX_FEE))
                {
                    Log("Verify dex match tx: Send deal tx nAmount error, nAmount: %lu, need amount: %lu, nMatchAmount: %lu, nRewardAmount: %lu, nFee: %ld, nTxFee: %lu, tx: %s",
                        tx.nAmount, nValueIn - TNS_DEX_MIN_TX_FEE, objMatch->nMatchAmount, nRewardAmount, objMatch->nFee, tx.nTxFee, tx.GetHash().GetHex().c_str());
                    return ERR_TRANSACTION_SIGNATURE_INVALID;
                }
                if (tx.nTxFee != TNS_DEX_MIN_TX_FEE)
                {
                    Log("Verify dex match tx: Send deal tx nTxFee error, nAmount: %lu, need amount: %lu, nMatchAmount: %lu, nRewardAmount: %lu, nFee: %ld, nTxFee: %lu, tx: %s",
                        tx.nAmount, nValueIn - TNS_DEX_MIN_TX_FEE, objMatch->nMatchAmount, nRewardAmount, objMatch->nFee, tx.nTxFee, tx.GetHash().GetHex().c_str());
                    return ERR_TRANSACTION_SIGNATURE_INVALID;
                }
            }
            else
            {
                Log("Verify dex match tx: sendTo error, tx: %s", tx.GetHash().GetHex().c_str());
                return ERR_TRANSACTION_SIGNATURE_INVALID;
            }
        }

        set<CDestination> setSubDest;
        vector<uint8> vchSigOut;
        if (!ptrMatch->GetSignDestination(tx, uint256(), 0, vchSig, setSubDest, vchSigOut))
        {
            Log("Verify dex match tx: get sign data fail, tx: %s", tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }

        vector<uint8> vms;
        vector<uint8> vss;
        vector<uint8> vchSigSub;
        try
        {
            vector<uint8> head;
            xengine::CIDataStream is(vchSigOut);
            is >> vms >> vss >> vchSigSub;
        }
        catch (std::exception& e)
        {
            Log("Verify dex match tx: get vms and vss fail, tx: %s", tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }

        if (crypto::CryptoSHA256(&(vss[0]), vss.size()) != objMatch->hashBuyerSecret)
        {
            Log("Verify dex match tx: hashBuyerSecret error, vss: %s, secret: %s, tx: %s",
                ToHexString(vss).c_str(), objMatch->hashBuyerSecret.GetHex().c_str(), tx.GetHash().GetHex().c_str());
            return ERR_TRANSACTION_SIGNATURE_INVALID;
        }
    }
    return OK;
}

Errno CCoreProtocol::VerifyMintPledgeTx(const CTransaction& tx)
{
    if (!tx.sendTo.IsTemplate() || tx.sendTo.GetTemplateId().GetType() != TEMPLATE_MINTREDEEM)
    {
        Log("Verify mit pledge tx: sendto error, tx: %s", tx.GetHash().GetHex().c_str());
        return ERR_TRANSACTION_INVALID;
    }

    vector<uint8> vchSig;
    if (!CTemplate::VerifyDestRecorded(tx, vchSig))
    {
        Log("Verify mit pledge tx: verify dest recorded fail, tx: %s", tx.GetHash().GetHex().c_str());
        return ERR_TRANSACTION_INVALID;
    }

    // to
    auto ptrRedeem = CTemplate::CreateTemplatePtr(TEMPLATE_MINTREDEEM, tx.vchSig);
    if (ptrRedeem == nullptr)
    {
        Log("Verify mit pledge tx: Create redeem template fail, tx: %s", tx.GetHash().GetHex().c_str());
        return ERR_TRANSACTION_INVALID;
    }
    auto objRedeem = boost::dynamic_pointer_cast<CTemplateMintRedeem>(ptrRedeem);

    // from
    auto ptrPledge = CTemplate::CreateTemplatePtr(TEMPLATE_MINTPLEDGE, vchSig);
    if (ptrPledge == nullptr)
    {
        Log("Verify mit pledge tx: Create pledge template fail, tx: %s", tx.GetHash().GetHex().c_str());
        return ERR_TRANSACTION_INVALID;
    }
    auto objPledge = boost::dynamic_pointer_cast<CTemplateMintPledge>(ptrPledge);

    if (objPledge->destOwner != objRedeem->destOwner)
    {
        Log("Verify mit pledge tx: owner address error, tx: %s, pledge owner: %s, redeem owner: %s",
            tx.GetHash().GetHex().c_str(),
            CAddress(objPledge->destOwner).ToString().c_str(),
            CAddress(objRedeem->destOwner).ToString().c_str());
        return ERR_TRANSACTION_INVALID;
    }
    return OK;
}

///////////////////////////////
// CTestNetCoreProtocol

CTestNetCoreProtocol::CTestNetCoreProtocol()
{
}

/*
Address: 1ge5wyx5x93yxcawhhq67wywa4ngrd8gghtaqwn22njhha4vztfmr8cpn
PubKey : e9d37f1315a3ac42547e958e10a28661258a7b7ecc8d912bd6fd48bd74cf8b83
Secret : 338f01358e2d0fa66a496c16f1838f9464a85a04d800923a757e722a0daf093e
*/
void CTestNetCoreProtocol::GetGenesisBlock(CBlock& block)
{
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    const CDestination destOwner = CDestination(minemon::crypto::CPubKey(uint256("e9d37f1315a3ac42547e958e10a28661258a7b7ecc8d912bd6fd48bd74cf8b83")));

    block.SetNull();

    block.nVersion = CBlock::BLOCK_VERSION;
    block.nType = CBlock::BLOCK_GENESIS;
    block.nTimeStamp = 1624604400;
    block.hashPrev = 0;

    CTransaction& tx = block.txMint;
    tx.nType = CTransaction::TX_GENESIS;
    tx.nTimeStamp = block.nTimeStamp;
    tx.sendTo = destOwner;
    tx.nAmount = BPX_INIT_REWARD_TOKEN; // initial number of token

    CProfile profile;
    profile.strName = "Minemon Test";
    profile.strSymbol = "MAMT";
    profile.destOwner = destOwner;
    profile.nAmount = tx.nAmount;
    profile.nMintReward = BPX_INIT_REWARD_TOKEN;
    profile.nMinTxFee = MIN_TX_FEE;
    profile.nHalveCycle = 0;
    profile.SetFlag(true, false, false);

    profile.Save(block.vchProof);
}

///////////////////////////////
// CProofOfWorkParam

CProofOfWorkParam::CProofOfWorkParam(bool fTestnet)
{
    if (fTestnet)
    {
        CBlock block;
        CTestNetCoreProtocol core;
        core.GetGenesisBlock(block);
        hashGenesisBlock = block.GetHash();
    }
    else
    {
        CBlock block;
        CCoreProtocol core;
        core.GetGenesisBlock(block);
        hashGenesisBlock = block.GetHash();
    }

    nProofOfWorkLowerLimit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_LOWER_LIMIT);
    nProofOfWorkLowerLimit.SetCompact(nProofOfWorkLowerLimit.GetCompact());
    nProofOfWorkUpperLimit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_UPPER_LIMIT);
    nProofOfWorkUpperLimit.SetCompact(nProofOfWorkUpperLimit.GetCompact());

    nProofOfWorkInit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_INIT);
    nProofOfWorkDifficultyInterval = PROOF_OF_WORK_DIFFICULTY_INTERVAL;
}

} // namespace minemon
