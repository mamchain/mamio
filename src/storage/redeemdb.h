// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_REDEEMDB_H
#define STORAGE_REDEEMDB_H

#include <map>

#include "destination.h"
#include "timeseries.h"
#include "transaction.h"
#include "uint256.h"
#include "xengine.h"

namespace minemon
{
namespace storage
{

class CRedeemDB : public xengine::CKVDB
{
public:
    CRedeemDB(const bool fCacheIn = true)
      : fCache(fCacheIn), cacheRedeem(MAX_CACHE_COUNT) {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNew(const uint256& hashBlock, const CRedeemContext& redeemContext);
    bool Remove(const uint256& hashBlock);
    bool Retrieve(const uint256& hashBlock, CRedeemContext& redeemContext);
    void Clear();

protected:
    enum
    {
        MAX_CACHE_COUNT = 1024
    };
    bool fCache;
    xengine::CCache<uint256, CRedeemContext> cacheRedeem;
    xengine::CRWAccess rwData;
};

} // namespace storage
} // namespace minemon

#endif // STORAGE_REDEEMDB_H
