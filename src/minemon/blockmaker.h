// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_BLOCKMAKER_H
#define MINEMON_BLOCKMAKER_H

#include <atomic>

#include "base.h"
#include "event.h"
#include "key.h"

namespace minemon
{

class CBlockMakerHashAlgo
{
public:
    CBlockMakerHashAlgo(const std::string& strAlgoIn, int64 nHashRateIn)
      : strAlgo(strAlgoIn), nHashRate(nHashRateIn) {}
    virtual ~CBlockMakerHashAlgo() {}
    virtual uint256 Hash(const std::vector<unsigned char>& vchData) = 0;

public:
    const std::string strAlgo;
    int64 nHashRate;
};

class CBlockMakerProfile
{
public:
    CBlockMakerProfile() {}
    CBlockMakerProfile(int nAlgoIn, const CDestination& destSpentIn, const uint32 nPledgeFeeIn)
      : nAlgo(nAlgoIn), destSpent(destSpentIn), nPledgeFee(nPledgeFeeIn)
    {
        BuildTemplate();
    }

    bool IsValid() const
    {
        return (templMint != nullptr);
    }
    bool BuildTemplate();
    std::size_t GetSignatureSize() const
    {
        std::size_t size = templMint->GetTemplateData().size() + 64;
        xengine::CVarInt var(size);
        return (size + xengine::GetSerializeSize(var));
    }
    const CDestination GetDestination() const
    {
        return (CDestination(templMint->GetTemplateId()));
    }

public:
    int nAlgo;
    CDestination destSpent;
    uint32 nPledgeFee;
    CTemplateMintPtr templMint;
};

class CBlockMaker : public IBlockMaker, virtual public CBlockMakerEventListener
{
public:
    CBlockMaker();
    ~CBlockMaker();
    bool HandleEvent(CEventBlockMakerUpdate& eventUpdate) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool InterruptedPoW(const uint256& hashPrimary);
    bool WaitExit(const long nSeconds);
    bool CreateProofOfWork();

private:
    void PowThreadFunc();

protected:
    xengine::CThread thrPow;
    boost::mutex mutex;
    boost::condition_variable condExit;
    std::atomic<bool> fExit;
    std::map<int, CBlockMakerHashAlgo*> mapHashAlgo;
    std::map<int, CBlockMakerProfile> mapWorkProfile;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    IService* pService;
};

} // namespace minemon

#endif //MINEMON_BLOCKMAKER_H
