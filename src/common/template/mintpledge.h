// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_MINTPLEDGE_H
#define COMMON_TEMPLATE_MINTPLEDGE_H

#include "template.h"

class CTemplateMintPledge : virtual public CTemplate, virtual public CSendToRecordedTemplate
{
public:
    CTemplateMintPledge(const CDestination& destOwnerIn = CDestination(), const CDestination& destPowMintIn = CDestination(), int nRewardModeIn = 1);
    virtual CTemplateMintPledge* clone() const;
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
    CDestination destPowMint;
    int nRewardMode;
};

#endif // COMMON_TEMPLATE_MINTPLEDGE_H
