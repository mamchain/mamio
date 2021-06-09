// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_PLEDGEDB_H
#define STORAGE_PLEDGEDB_H

#include <map>

#include "destination.h"
#include "timeseries.h"
#include "uint256.h"
#include "xengine.h"

namespace minemon
{
namespace storage
{

class CPledgeContext
{
    friend class xengine::CStream;

public:
    CPledgeContext() {}

    bool IsFull() const
    {
        return (hashRef == 0);
    }
    void Clear()
    {
        hashRef = 0;
        mapBlockPledge.clear();
        mapPledge.clear();
    }

public:
    uint256 hashRef;
    std::map<CDestination, std::pair<CDestination, int64>> mapBlockPledge; // pledge address, pow address
    std::map<CDestination, std::map<CDestination, int64>> mapPledge;       // pow address, pledge address

public:
    friend bool operator==(const CPledgeContext& a, const CPledgeContext& b)
    {
        return (a.hashRef == b.hashRef && a.mapBlockPledge == b.mapBlockPledge && a.mapPledge == b.mapPledge);
    }
    friend bool operator!=(const CPledgeContext& a, const CPledgeContext& b)
    {
        return !(a == b);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(hashRef, opt);
        s.Serialize(mapBlockPledge, opt);
        s.Serialize(mapPledge, opt);
    }
};

class CPledgeDB : public xengine::CKVDB
{
public:
    CPledgeDB(const bool fCacheIn = true)
      : fCache(fCacheIn) {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNew(const uint256& hashBlock, const CPledgeContext& ctxtPledge);
    bool Remove(const uint256& hashBlock);
    bool Retrieve(const uint256& hashBlock, CPledgeContext& ctxtPledge);
    bool AddBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::pair<CDestination, int64>>& mapBlockPledgeIn);
    bool GetBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::pair<CDestination, int64>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge);
    bool RetrieveBlockPledge(const uint256& hashBlock, CPledgeContext& ctxtPledge);
    void Clear();

protected:
    bool IsFullPledge(const uint256& hashBlock);
    bool GetFullBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::pair<CDestination, int64>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge);
    bool GetIncrementBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::pair<CDestination, int64>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge);

    bool AddPledge(const uint256& hashBlock, const CPledgeContext& ctxtPledge);
    bool RemovePledge(const uint256& hashBlock);
    bool GetPledge(const uint256& hashBlock, CPledgeContext& ctxtPledge);
    bool GetPledge(const uint256& hashBlock, std::map<CDestination, std::map<CDestination, int64>>& mapPledgeOut);

    void AddCache(const uint256& hashBlock, const CPledgeContext& ctxtPledge);

protected:
    enum
    {
        MAX_FULL_CACHE_COUNT = 16,
        MAX_INC_CACHE_COUNT = 1440 * 2,
    };
    bool fCache;
    xengine::CRWAccess rwData;
    std::map<uint256, CPledgeContext> mapCacheFullPledge;
    std::map<uint256, CPledgeContext> mapCacheIncPledge;
};

} // namespace storage
} // namespace minemon

#endif // STORAGE_PLEDGEDB_H
