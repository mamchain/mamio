// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_NETWORK_H
#define MINEMON_NETWORK_H

#include "config.h"
#include "peernet.h"
#include "base.h"

namespace minemon
{

class CNetwork : public network::CBbPeerNet
{
public:
    CNetwork();
    ~CNetwork();
    bool CheckPeerVersion(uint32 nVersionIn, uint64 nServiceIn, const std::string& subVersionIn) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    const CNetworkConfig* NetworkConfig()
    {
        return dynamic_cast<const CNetworkConfig*>(xengine::IBase::Config());
    }
    const CStorageConfig* StorageConfig()
    {
        return dynamic_cast<const CStorageConfig*>(xengine::IBase::Config());
    }

protected:
    ICoreProtocol* pCoreProtocol;
};

} // namespace minemon

#endif //MINEMON_NETWORK_H
