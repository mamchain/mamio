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
      : fCache(fCacheIn) {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNew(const uint256& hashBlock, const CRedeemContext& ctxtRedeem);
    bool Remove(const uint256& hashBlock);
    bool Retrieve(const uint256& hashBlock, CRedeemContext& ctxtRedeem);
    bool AddBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn);
    bool GetBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn, CRedeemContext& ctxtRedeem);
    bool RetrieveBlockRedeem(const uint256& hashBlock, CRedeemContext& ctxtRedeem);
    void Clear();

protected:
    bool IsFullRedeem(const uint256& hashBlock);
    bool GetFullBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn, CRedeemContext& ctxtRedeem);
    bool GetIncrementBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn, CRedeemContext& ctxtRedeem);

    bool AddRedeem(const uint256& hashBlock, const CRedeemContext& ctxtRedeem);
    bool RemoveRedeem(const uint256& hashBlock);
    bool GetRedeem(const uint256& hashBlock, CRedeemContext& ctxtRedeem);
    bool GetRedeem(const uint256& hashBlock, std::map<CDestination, CDestRedeem>& mapRedeemOut);

    void AddCache(const uint256& hashBlock, const CRedeemContext& ctxtRedeem);

protected:
    enum
    {
        MAX_FULL_CACHE_COUNT = 16,
        MAX_INC_CACHE_COUNT = 1440 * 2,
    };
    bool fCache;
    xengine::CRWAccess rwData;
    std::map<uint256, CRedeemContext> mapCacheFullRedeem;
    std::map<uint256, CRedeemContext> mapCacheIncRedeem;
};

} // namespace storage
} // namespace minemon

#endif // STORAGE_REDEEMDB_H
