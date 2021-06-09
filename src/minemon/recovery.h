// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_RECOVERY_H
#define MINEMON_RECOVERY_H

#include "base.h"

namespace minemon
{

class CRecovery : public IRecovery
{
public:
    CRecovery();
    ~CRecovery();

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;

protected:
    IDispatcher* pDispatcher;
};

} // namespace minemon
#endif // MINEMON_RECOVERY_H
