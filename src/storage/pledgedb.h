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

    std::map<CDestination, std::map<CDestination, int64>> mapPledge; // pow address, pledge address

public:
    friend bool operator==(const CPledgeContext& a, const CPledgeContext& b)
    {
        return (a.mapPledge == b.mapPledge);
    }
    friend bool operator!=(const CPledgeContext& a, const CPledgeContext& b)
    {
        return !(a == b);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(mapPledge, opt);
    }
};

class CPledgeDB : public xengine::CKVDB
{
public:
    CPledgeDB(const bool fCacheIn = true)
      : fCache(fCacheIn), cachePledge(MAX_CACHE_COUNT) {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNew(const uint256& hashBlock, const CPledgeContext& ctxtPledge);
    bool Remove(const uint256& hashBlock);
    bool Retrieve(const uint256& hashBlock, CPledgeContext& ctxtPledge);
    void Clear();

protected:
    enum
    {
        MAX_CACHE_COUNT = 1024
    };
    bool fCache;
    xengine::CCache<uint256, CPledgeContext> cachePledge;
    xengine::CRWAccess rwData;
};

} // namespace storage
} // namespace minemon

#endif // STORAGE_PLEDGEDB_H
