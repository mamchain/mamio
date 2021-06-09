// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mintredeem.h"

#include "destination.h"
#include "rpc/auto_protocol.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;

//////////////////////////////
// CTemplateMintRedeem

CTemplateMintRedeem::CTemplateMintRedeem(const CDestination& destOwnerIn, int64 nNonceIn)
  : CTemplate(TEMPLATE_MINTREDEEM),
    destOwner(destOwnerIn),
    nNonce(nNonceIn)
{
}

CTemplateMintRedeem* CTemplateMintRedeem::clone() const
{
    return new CTemplateMintRedeem(*this);
}

bool CTemplateMintRedeem::GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const vector<uint8>& vchSig,
                                             set<CDestination>& setSubDest, vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, hashFork, nHeight, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }
    setSubDest.clear();
    setSubDest.insert(destOwner);
    return true;
}

void CTemplateMintRedeem::GetTemplateData(minemon::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.mintredeem.strOwner = (destInstance = destOwner).ToString();
    obj.mintredeem.nNonce = nNonce;
}

bool CTemplateMintRedeem::ValidateParam() const
{
    if (!IsTxSpendable(destOwner))
    {
        return false;
    }
    return true;
}

bool CTemplateMintRedeem::SetTemplateData(const std::vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> destOwner >> nNonce;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CTemplateMintRedeem::SetTemplateData(const minemon::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_MINTREDEEM))
    {
        return false;
    }

    if (!destInstance.ParseString(obj.mintredeem.strOwner))
    {
        return false;
    }
    destOwner = destInstance;

    nNonce = obj.mintredeem.nNonce;
    return true;
}

void CTemplateMintRedeem::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destOwner << nNonce;
}

bool CTemplateMintRedeem::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                            const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    return destOwner.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
