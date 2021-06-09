// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_TEMPLATEDATADB_H
#define STORAGE_TEMPLATEDATADB_H

#include <map>

#include "destination.h"
#include "timeseries.h"
#include "uint256.h"
#include "xengine.h"

namespace minemon
{
namespace storage
{

class CTemplateDataDBWalker
{
public:
    virtual bool Walk(const CDestination& dest, const std::vector<uint8>& vTemplateData) = 0;
};

class CTemplateDataDB : public xengine::CKVDB
{
public:
    CTemplateDataDB(const bool fCacheIn = true)
      : fCache(fCacheIn), cacheTemplate(MAX_CACHE_COUNT) {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNew(const CDestination& dest, const std::vector<uint8>& vTemplateData);
    bool Remove(const CDestination& dest);
    bool Retrieve(const CDestination& dest, std::vector<uint8>& vTemplateData);
    bool WalkThroughTemplateData(CTemplateDataDBWalker& walker);
    void Clear();

protected:
    bool LoadTemplateDataWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue, CTemplateDataDBWalker& walker);

protected:
    enum
    {
        MAX_CACHE_COUNT = 0x10000
    };
    bool fCache;
    xengine::CCache<CDestination, std::vector<uint8>> cacheTemplate;
    xengine::CRWAccess rwData;
};

} // namespace storage
} // namespace minemon

#endif // STORAGE_TEMPLATEDATADB_H
