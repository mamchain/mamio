// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_MODE_STORAGE_CONFIG_H
#define MINEMON_MODE_STORAGE_CONFIG_H

#include <string>

#include "mode/basic_config.h"

namespace minemon
{
class CStorageConfig : virtual public CBasicConfig, virtual public CStorageConfigOption
{
public:
    CStorageConfig();
    virtual ~CStorageConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;
};

} // namespace minemon

#endif // MINEMON_MODE_STORAGE_CONFIG_H
