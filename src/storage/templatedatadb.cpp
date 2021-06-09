// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "templatedatadb.h"

#include <boost/range/adaptor/reversed.hpp>

#include "leveldbeng.h"

using namespace std;
using namespace xengine;

namespace minemon
{
namespace storage
{

//////////////////////////////
// CTemplateDataDB

bool CTemplateDataDB::Initialize(const boost::filesystem::path& pathData)
{
    CLevelDBArguments args;
    args.path = (pathData / "template").string();
    args.syncwrite = false;
    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }
    return true;
}

void CTemplateDataDB::Deinitialize()
{
    cacheTemplate.Clear();
    Close();
}

bool CTemplateDataDB::AddNew(const CDestination& dest, const vector<uint8>& vTemplateData)
{
    xengine::CWriteLock wlock(rwData);
    if (!Write(dest, vTemplateData))
    {
        return false;
    }
    if (fCache)
    {
        cacheTemplate.AddNew(dest, vTemplateData);
    }
    return true;
}

bool CTemplateDataDB::Remove(const CDestination& dest)
{
    xengine::CWriteLock wlock(rwData);
    if (fCache)
    {
        cacheTemplate.Remove(dest);
    }
    return Erase(dest);
}

bool CTemplateDataDB::Retrieve(const CDestination& dest, vector<uint8>& vTemplateData)
{
    xengine::CReadLock rlock(rwData);
    if (fCache)
    {
        if (cacheTemplate.Retrieve(dest, vTemplateData))
        {
            return true;
        }
        if (!Read(dest, vTemplateData))
        {
            return false;
        }
        cacheTemplate.AddNew(dest, vTemplateData);
    }
    else
    {
        if (!Read(dest, vTemplateData))
        {
            return false;
        }
    }
    return true;
}

bool CTemplateDataDB::WalkThroughTemplateData(CTemplateDataDBWalker& walker)
{
    return WalkThrough(boost::bind(&CTemplateDataDB::LoadTemplateDataWalker, this, _1, _2, boost::ref(walker)));
}

void CTemplateDataDB::Clear()
{
    cacheTemplate.Clear();
    RemoveAll();
}

bool CTemplateDataDB::LoadTemplateDataWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue, CTemplateDataDBWalker& walker)
{
    CDestination dest;
    vector<uint8> vTemplateData;
    ssKey >> dest;
    ssValue >> vTemplateData;
    return walker.Walk(dest, vTemplateData);
}

} // namespace storage
} // namespace minemon
