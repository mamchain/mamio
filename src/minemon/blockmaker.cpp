// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockmaker.h"

#include <thread>

#include "address.h"
#include "template/mint.h"
#include "template/proof.h"
#include "util.h"

using namespace std;
using namespace xengine;

#define INITIAL_HASH_RATE (8000)
#define WAIT_AGREEMENT_TIME_OFFSET -5
#define WAIT_NEWBLOCK_TIME (BLOCK_TARGET_SPACING + 5)
#define WAIT_LAST_EXTENDED_TIME (BLOCK_TARGET_SPACING - 10)

namespace minemon
{

//////////////////////////////
// CBlockMakerHashAlgo

class CHashAlgo_Sha256 : public minemon::CBlockMakerHashAlgo
{
public:
    CHashAlgo_Sha256(int64 nHashRateIn)
      : CBlockMakerHashAlgo("sha256", nHashRateIn) {}
    uint256 Hash(const std::vector<unsigned char>& vchData)
    {
        return crypto::CryptoPowHash(&vchData[0], vchData.size());
    }
};

//////////////////////////////
// CBlockMakerProfile
bool CBlockMakerProfile::BuildTemplate()
{
    templMint = CTemplateMint::CreateTemplatePtr(new CTemplateProof(destSpent, nPledgeFee));
    return (templMint != nullptr);
}

//////////////////////////////
// CBlockMaker

CBlockMaker::CBlockMaker()
  : thrPow("powmaker", boost::bind(&CBlockMaker::PowThreadFunc, this))
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pService = nullptr;
    mapHashAlgo[CM_SHA256D] = new CHashAlgo_Sha256(INITIAL_HASH_RATE);
}

CBlockMaker::~CBlockMaker()
{
    for (map<int, CBlockMakerHashAlgo*>::iterator it = mapHashAlgo.begin(); it != mapHashAlgo.end(); ++it)
    {
        delete ((*it).second);
    }
    mapHashAlgo.clear();
}

bool CBlockMaker::HandleInitialize()
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

    if (!GetObject("service", pService))
    {
        Error("Failed to request service");
        return false;
    }

    if (!MintConfig()->destSpent.IsNull() && MintConfig()->nPledgeFee < 1000)
    {
        CBlockMakerProfile profile(CM_SHA256D, MintConfig()->destSpent, MintConfig()->nPledgeFee);
        if (profile.IsValid())
        {
            mapWorkProfile.insert(make_pair(CM_SHA256D, profile));
        }
    }

    // print log
    const char* ConsensusMethodName[CM_MAX] = { "sha256" };
    Log("Block maker started");
    for (map<int, CBlockMakerProfile>::iterator it = mapWorkProfile.begin(); it != mapWorkProfile.end(); ++it)
    {
        CBlockMakerProfile& profile = (*it).second;
        Log("Profile [%s] : spent=%s, pledgefee=%d, pow address=%s",
            ConsensusMethodName[(*it).first],
            CAddress(profile.destSpent).ToString().c_str(),
            profile.nPledgeFee,
            CAddress(profile.templMint->GetTemplateId()).ToString().c_str());
    }
    return true;
}

void CBlockMaker::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pService = nullptr;

    mapWorkProfile.clear();
}

bool CBlockMaker::HandleInvoke()
{
    if (!IBlockMaker::HandleInvoke())
    {
        return false;
    }

    fExit = true;
    if (!mapWorkProfile.empty())
    {
        if (!ThreadDelayStart(thrPow))
        {
            return false;
        }
        fExit = false;
    }
    return true;
}

void CBlockMaker::HandleHalt()
{
    fExit = true;
    condExit.notify_all();

    thrPow.Interrupt();
    ThreadExit(thrPow);

    IBlockMaker::HandleHalt();
}

bool CBlockMaker::HandleEvent(CEventBlockMakerUpdate& eventUpdate)
{
    return true;
}

bool CBlockMaker::InterruptedPoW(const uint256& hashPrimary)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    return fExit;
}

bool CBlockMaker::WaitExit(const long nSeconds)
{
    if (nSeconds <= 0)
    {
        return !fExit;
    }
    boost::system_time const timeout = boost::get_system_time() + boost::posix_time::seconds(nSeconds);
    boost::unique_lock<boost::mutex> lock(mutex);
    while (!fExit)
    {
        if (!condExit.timed_wait(lock, timeout))
        {
            break;
        }
    }
    return !fExit;
}

bool CBlockMaker::CreateProofOfWork()
{
    int nConsensus = CM_SHA256D;
    map<int, CBlockMakerProfile>::iterator it = mapWorkProfile.find(nConsensus);
    if (it == mapWorkProfile.end())
    {
        StdError("blockmaker", "did not find Work profile");
        return false;
    }
    CBlockMakerProfile& profile = it->second;
    CBlockMakerHashAlgo* pHashAlgo = mapHashAlgo[profile.nAlgo];
    if (pHashAlgo == nullptr)
    {
        StdError("blockmaker", "pHashAlgo is null");
        return false;
    }

    vector<unsigned char> vchWorkData;
    int nPrevBlockHeight = 0;
    uint256 hashPrev;
    uint32 nPrevTime = 0;
    int nAlgo = 0;
    uint32 nBits = 0;

    if (!pService->GetWork(vchWorkData, nPrevBlockHeight, hashPrev, nPrevTime, nAlgo, nBits, profile.templMint))
    {
        //StdTrace("blockmaker", "GetWork fail");
        return false;
    }
    uint16_t nVersion;
    uint16_t nType;
    uint256 hashMerkle;
    xengine::CBufStream ss;
    ss.Write((const char*)vchWorkData.data(), vchWorkData.size());
    ss >> nVersion >> nType >> hashPrev >> hashMerkle >> nBits;

    uint256 mam_pow = crypto::CryptoSHA256(vchWorkData.data(), vchWorkData.size());
    std::reverse(mam_pow.begin(), mam_pow.end());
    CProofOfHashWorkCompact obj;
    string str = "minemon solo";
    std::vector<uint8> data1 = { str.begin(), str.end() };
    str = "good luck";
    std::vector<uint8> data2 = { str.begin(), str.end() };
    std::vector<uint256> auxMerkleBranch;
    auto coinbase = obj.BuildCoinBase(data1, data2, mam_pow, auxMerkleBranch);

    std::vector<uint256> trMerkleBranch;
    uint16_t auxOffset = data1.size();
    int32_t btcVersion = 536870912;
    uint256 btcHashPrevBlock;
    uint32_t btcBits = 0x1d00ffff;
    obj.SetBtcPow(auxOffset, coinbase, auxMerkleBranch, trMerkleBranch, btcVersion, btcHashPrevBlock, btcBits);

    vchWorkData.clear();
    obj.GetBtcPow(vchWorkData);

    uint32& nTime = *((uint32*)&vchWorkData[68]);
    nTime = max(nPrevTime + 1, (uint32_t)GetNetTime());
    uint32_t& nNonce = *((uint32_t*)&vchWorkData[76]);

    int64& nHashRate = pHashAlgo->nHashRate;
    int64 nHashComputeCount = 0;
    int64 nHashComputeBeginTime = GetTime();

    Log("Proof-of-work: start hash compute, target height: %d, difficulty bits: (0x%x)", nPrevBlockHeight + 1, nBits);

    uint256 hashTarget;
    hashTarget.SetCompact(nBits);
    while (!InterruptedPoW(hashPrev))
    {
        if (nHashRate == 0)
        {
            nHashRate = 1;
        }
        for (int i = 0; i < nHashRate; i++)
        {
            uint256 hash = pHashAlgo->Hash(vchWorkData);
            nHashComputeCount++;
            if (hash <= hashTarget)
            {
                int64 nDuration = GetTime() - nHashComputeBeginTime;
                int nCompHashRate = ((nDuration <= 0) ? 0 : (nHashComputeCount / nDuration));

                Log("Proof-of-work: block found (%s), target height: %d, compute: (rate:%ld, count:%ld, duration:%lds, hashrate:%ld), difficulty bits: (0x%x)\nhash :   %s\ntarget : %s",
                    pHashAlgo->strAlgo.c_str(), nPrevBlockHeight + 1, nHashRate, nHashComputeCount, nDuration, nCompHashRate, nBits,
                    hash.GetHex().c_str(), hashTarget.GetHex().c_str());
                obj.SetBtcPow(vchWorkData);
                vchWorkData.clear();
                obj.Save(vchWorkData);

                xengine::CBufStream ss;
                ss << nVersion << nType << nTime << hashPrev << hashMerkle << nBits << vchWorkData;
                std::vector<uint8> data;
                data.assign(ss.GetData(), ss.GetData() + ss.GetSize());
                Errno err = pService->SubmitWork(data, profile.templMint, hashPrev);
                if (err != OK)
                {
                    return false;
                }
                return true;
            }
            nNonce++;
        }

        int64 nNetTime = GetNetTime();
        if (nTime + 1 < nNetTime)
        {
            nHashRate /= (nNetTime - nTime);
            nTime = nNetTime;
        }
        else if (nTime == nNetTime)
        {
            nHashRate *= 2;
        }
    }
    Log("Proof-of-work: target height: %d, compute interrupted.", nPrevBlockHeight + 1);
    return false;
}

void CBlockMaker::PowThreadFunc()
{
    if (!WaitExit(5))
    {
        Log("Pow exited non");
        return;
    }
    while (WaitExit(1))
    {
        CreateProofOfWork();
    }
    Log("Pow exited");
}

} // namespace minemon
