// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_MODE_MODULE_TYPE_H
#define MINEMON_MODE_MODULE_TYPE_H

namespace minemon
{
// module type
enum class EModuleType
{
    LOCK,             // lock file
    BLOCKMAKER,       // CBlockMaker
    COREPROTOCOL,     // CCoreProtocol
    DISPATCHER,       // CDispatcher
    HTTPGET,          // CHttpGet
    HTTPSERVER,       // CHttpServer
    NETCHANNEL,       // CNetChannel
    DELEGATEDCHANNEL, // CDelegatedChannel
    NETWORK,          // CNetwork
    RPCCLIENT,        // CRPCClient
    RPCMODE,          // CRPCMod
    SERVICE,          // CService
    TXPOOL,           // CTxPool
    WALLET,           // CWallet
    BLOCKCHAIN,       // CBlockChain
    CONSENSUS,        // CConsensus
    FORKMANAGER,      // CForkManager
    DATASTAT,         // CDataStat
    RECOVERY,         // CRecovery
};

} // namespace minemon

#endif // MINEMON_MODE_MODULE_TYPE_H
