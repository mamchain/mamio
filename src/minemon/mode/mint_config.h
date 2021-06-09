// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_MODE_MINT_CONFIG_H
#define MINEMON_MODE_MINT_CONFIG_H

#include <string>

#include "destination.h"
#include "mode/basic_config.h"
#include "uint256.h"

namespace minemon
{
class CMintConfig : virtual public CBasicConfig, virtual public CMintConfigOption
{
public:
    CMintConfig();
    virtual ~CMintConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

public:
    CDestination destSpent;
};

} // namespace minemon

#endif // MINEMON_MODE_MINT_CONFIG_H
