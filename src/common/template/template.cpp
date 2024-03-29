// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "template.h"

#include "json/json_spirit_reader_template.h"
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include "dexmatch.h"
#include "dexorder.h"
#include "exchange.h"
#include "mintpledge.h"
#include "mintredeem.h"
#include "multisig.h"
#include "proof.h"
#include "rpc/auto_protocol.h"
#include "stream/datastream.h"
#include "template.h"
#include "templateid.h"
#include "transaction.h"
#include "weighted.h"

using namespace std;
using namespace xengine;
using namespace minemon::crypto;
using namespace minemon::rpc;

//////////////////////////////
// CTypeInfo

struct CTypeInfo
{
    uint16 nType;
    CTemplate* ptr;
    std::string strName;
};

using CTypeInfoSet = boost::multi_index_container<
    CTypeInfo,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CTypeInfo, uint16, &CTypeInfo::nType>>,
        boost::multi_index::ordered_unique<boost::multi_index::member<CTypeInfo, std::string, &CTypeInfo::strName>>>>;

static const CTypeInfoSet setTypeInfo = {
    { TEMPLATE_WEIGHTED, new CTemplateWeighted, "weighted" },
    { TEMPLATE_MULTISIG, new CTemplateMultiSig, "multisig" },
    { TEMPLATE_PROOF, new CTemplateProof, "mint" },
    { TEMPLATE_EXCHANGE, new CTemplateExchange, "exchange" },
    { TEMPLATE_DEXORDER, new CTemplateDexOrder, "dexorder" },
    { TEMPLATE_DEXMATCH, new CTemplateDexMatch, "dexmatch" },
    { TEMPLATE_MINTPLEDGE, new CTemplateMintPledge, "mintpledge" },
    { TEMPLATE_MINTREDEEM, new CTemplateMintRedeem, "mintredeem" },
};

static const CTypeInfo* GetTypeInfoByType(uint16 nTypeIn)
{
    const auto& idxType = setTypeInfo.get<0>();
    auto it = idxType.find(nTypeIn);
    return (it == idxType.end()) ? nullptr : &(*it);
}

const CTypeInfo* GetTypeInfoByName(std::string strNameIn)
{
    const auto& idxName = setTypeInfo.get<1>();
    auto it = idxName.find(strNameIn);
    return (it == idxName.end()) ? nullptr : &(*it);
}

//////////////////////////////
// CTemplate

const CTemplatePtr CTemplate::CreateTemplatePtr(CTemplate* ptr)
{
    if (ptr)
    {
        if (!ptr->ValidateParam())
        {
            delete ptr;
            return nullptr;
        }
        ptr->BuildTemplateData();
        ptr->BuildTemplateId();
    }
    return CTemplatePtr(ptr);
}

const CTemplatePtr CTemplate::CreateTemplatePtr(const CDestination& destIn, const vector<uint8>& vchDataIn)
{
    return CreateTemplatePtr(destIn.GetTemplateId(), vchDataIn);
}

const CTemplatePtr CTemplate::CreateTemplatePtr(const CTemplateId& nIdIn, const vector<uint8>& vchDataIn)
{
    return CreateTemplatePtr(nIdIn.GetType(), vchDataIn);
}

const CTemplatePtr CTemplate::CreateTemplatePtr(uint16 nTypeIn, const vector<uint8>& vchDataIn)
{
    const CTypeInfo* pTypeInfo = GetTypeInfoByType(nTypeIn);
    if (!pTypeInfo)
    {
        return nullptr;
    }

    CTemplate* ptr = pTypeInfo->ptr->clone();
    if (ptr)
    {
        if (!ptr->SetTemplateData(vchDataIn) || !ptr->ValidateParam())
        {
            delete ptr;
            return nullptr;
        }
        ptr->BuildTemplateData();
        ptr->BuildTemplateId();
    }
    return CTemplatePtr(ptr);
}

const CTemplatePtr CTemplate::CreateTemplatePtr(const CTemplateRequest& obj, CDestination&& destInstance)
{
    const CTypeInfo* pTypeInfo = GetTypeInfoByName(obj.strType);
    if (!pTypeInfo)
    {
        return nullptr;
    }

    CTemplate* ptr = pTypeInfo->ptr->clone();
    if (ptr)
    {
        if (!ptr->SetTemplateData(obj, move(destInstance)) || !ptr->ValidateParam())
        {
            delete ptr;
            return nullptr;
        }
        ptr->BuildTemplateData();
        ptr->BuildTemplateId();
    }
    return CTemplatePtr(ptr);
}

const CTemplatePtr CTemplate::Import(const vector<uint8>& vchTemplateIn)
{
    if (vchTemplateIn.size() < 2)
    {
        return nullptr;
    }

    uint16 nTemplateTypeIn = vchTemplateIn[0] | (((uint16)vchTemplateIn[1]) << 8);
    vector<uint8> vchDataIn(vchTemplateIn.begin() + 2, vchTemplateIn.end());

    return CreateTemplatePtr(nTemplateTypeIn, vchDataIn);
}

bool CTemplate::IsValidType(const uint16 nTypeIn)
{
    return (nTypeIn > TEMPLATE_MIN && nTypeIn < TEMPLATE_MAX);
}

string CTemplate::GetTypeName(uint16 nTypeIn)
{
    const CTypeInfo* pTypeInfo = GetTypeInfoByType(nTypeIn);
    return pTypeInfo ? pTypeInfo->strName : "";
}

bool CTemplate::VerifyTxSignature(const CTemplateId& nIdIn, const uint16 nType, const uint256& hash, const uint256& hashAnchor,
                                  const CDestination& destTo, const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted)
{
    CTemplatePtr ptr = CreateTemplatePtr(nIdIn.GetType(), vchSig);
    if (!ptr)
    {
        return false;
    }

    if (ptr->nId != nIdIn)
    {
        return false;
    }
    vector<uint8> vchSubSig(vchSig.begin() + ptr->vchData.size(), vchSig.end());
    return ptr->VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSubSig, nForkHeight, fCompleted);
}

bool CTemplate::IsTxSpendable(const CDestination& dest)
{
    if (dest.IsPubKey())
    {
        return true;
    }
    else if (dest.IsTemplate())
    {
        uint16 nType = dest.GetTemplateId().GetType();
        const CTypeInfo* pTypeInfo = GetTypeInfoByType(nType);
        if (pTypeInfo)
        {
            return (dynamic_cast<CSpendableTemplate*>(pTypeInfo->ptr) != nullptr);
        }
    }
    return false;
}

bool CTemplate::IsDestInRecorded(const CDestination& dest)
{
    if (dest.IsTemplate())
    {
        uint16 nType = dest.GetTemplateId().GetType();
        const CTypeInfo* pTypeInfo = GetTypeInfoByType(nType);
        if (pTypeInfo)
        {
            return (dynamic_cast<CSendToRecordedTemplate*>(pTypeInfo->ptr) != nullptr);
        }
    }
    return false;
}

bool CTemplate::VerifyDestRecorded(const CTransaction& tx, vector<uint8>& vchSigOut)
{
    if (!tx.vchSig.empty())
    {
        if (tx.sendTo.IsTemplate() && CTemplate::IsDestInRecorded(tx.sendTo))
        {
            CTemplatePtr ptr = CTemplate::CreateTemplatePtr(tx.sendTo.GetTemplateId().GetType(), tx.vchSig);
            if (!ptr)
            {
                return false;
            }
            if (ptr->GetTemplateId() != tx.sendTo.GetTemplateId())
            {
                return false;
            }
            set<CDestination> setSubDest;
            if (!ptr->GetSignDestination(tx, uint256(), 0, tx.vchSig, setSubDest, vchSigOut))
            {
                return false;
            }
        }
        else
        {
            vchSigOut = tx.vchSig;
        }
    }
    return true;
}

const uint16& CTemplate::GetTemplateType() const
{
    return nType;
}

const CTemplateId& CTemplate::GetTemplateId() const
{
    return nId;
}

const vector<uint8>& CTemplate::GetTemplateData() const
{
    return vchData;
}

std::string CTemplate::GetName() const
{
    return GetTypeName(nType);
}

vector<uint8> CTemplate::Export() const
{
    vector<uint8> vchTemplate;
    vchTemplate.reserve(2 + vchData.size());
    vchTemplate.push_back((uint8)(nType & 0xff));
    vchTemplate.push_back((uint8)(nType >> 8));
    vchTemplate.insert(vchTemplate.end(), vchData.begin(), vchData.end());
    return vchTemplate;
}

bool CTemplate::GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const vector<uint8>& vchSig,
                                   set<CDestination>& setSubDest, vector<uint8>& vchSubSig) const
{
    if (!vchSig.empty())
    {
        if (vchSig.size() < vchData.size())
        {
            return false;
        }
        vchSubSig.assign(vchSig.begin() + vchData.size(), vchSig.end());
    }
    return true;
}

bool CTemplate::BuildTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor,
                                 const CDestination& destTo, const int32 nForkHeight,
                                 const vector<uint8>& vchPreSig, vector<uint8>& vchSig,
                                 bool& fCompleted) const
{
    vchSig = vchData;
    if (!VerifyTxSignature(hash, nType, hashAnchor, destTo, vchPreSig, nForkHeight, fCompleted))
    {
        return false;
    }

    return BuildTxSignature(hash, nType, hashAnchor, destTo, vchPreSig, vchSig);
}

CTemplate::CTemplate(const uint16 nTypeIn)
  : nType(nTypeIn)
{
}

bool CTemplate::VerifyTemplateData(const vector<uint8>& vchSig) const
{
    return (vchSig.size() >= vchData.size() && memcmp(&vchData[0], &vchSig[0], vchData.size()) == 0);
}

void CTemplate::BuildTemplateId()
{
    nId = CTemplateId(nType, minemon::crypto::CryptoHash(&vchData[0], vchData.size()));
}

bool CTemplate::BuildTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                 const vector<uint8>& vchPreSig, vector<uint8>& vchSig) const
{
    vchSig.insert(vchSig.end(), vchPreSig.begin(), vchPreSig.end());
    return true;
}
