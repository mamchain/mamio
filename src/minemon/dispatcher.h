// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_DISPATCHER_H
#define MINEMON_DISPATCHER_H

#include "base.h"
#include "peernet.h"

namespace minemon
{

class CDispatcher : public IDispatcher
{
public:
    CDispatcher();
    ~CDispatcher();
    Errno AddNewBlock(const CBlock& block, uint64 nNonce = 0) override;
    Errno AddNewTx(const CTransaction& tx, uint64 nNonce = 0) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    void ActivateFork(const uint256& hashFork, const uint64& nNonce);
    bool ProcessForkTx(const uint256& txid, const CTransaction& tx);

protected:
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    ITxPool* pTxPool;
    IForkManager* pForkManager;
    IWallet* pWallet;
    IService* pService;
    IBlockMaker* pBlockMaker;
    network::INetChannel* pNetChannel;
    IDataStat* pDataStat;
};

} // namespace minemon

#endif //MINEMON_DISPATCHER_H
