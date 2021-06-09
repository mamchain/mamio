// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_MINTREDEEM_H
#define COMMON_TEMPLATE_MINTREDEEM_H

#include "template.h"

class CTemplateMintRedeem : virtual public CTemplate, virtual public CSendToRecordedTemplate
{
public:
    CTemplateMintRedeem(const CDestination& destOwnerIn = CDestination(), int64 nNonceIn = 0);
    virtual CTemplateMintRedeem* clone() const;
    virtual bool GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;
    virtual void GetTemplateData(minemon::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const minemon::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();
    virtual bool VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const;

public:
    CDestination destOwner;
    int64 nNonce;
};

#endif // COMMON_TEMPLATE_MINTREDEEM_H
