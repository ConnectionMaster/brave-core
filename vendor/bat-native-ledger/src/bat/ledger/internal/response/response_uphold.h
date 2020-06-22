/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVELEDGER_COMMON_RESPONSE_UPHOLD_H_
#define BRAVELEDGER_COMMON_RESPONSE_UPHOLD_H_

#include <map>
#include <string>

#include "bat/ledger/internal/uphold/uphold_user.h"
#include "bat/ledger/mojom_structs.h"

namespace braveledger_response_util {

ledger::Result ParseFetchUpholdBalanceResponse(
    const ledger::UrlResponse& response,
    double* available);

ledger::Result ParseUpholdAuthorizationResponse(
    const ledger::UrlResponse& response,
    std::string* token);

ledger::Result ParseUpholdGetUserResponse(
    const ledger::UrlResponse& response,
    braveledger_uphold::User* user);

ledger::Result ParseUpholdGetCardAddressesResponse(
    const ledger::UrlResponse& response,
    std::map<std::string, std::string>* addresses);

ledger::Result ParseUpholdCreateAnonAddressResponse(
    const ledger::UrlResponse& response,
    std::string* id);

ledger::Result ParseUpholdCreateIfNecessaryResponse(
    const ledger::UrlResponse& response,
    const std::string& card_name,
    std::string* id);

ledger::Result ParseUpholdAnonAddressResponse(
    const ledger::UrlResponse& response,
    std::string* id);

ledger::Result ParseUpholdCreateTransactionResponse(
    const ledger::UrlResponse& response,
    std::string* id);

ledger::Result ParseUpholdCommitTransactionResponse(
    const ledger::UrlResponse& response);

}  // namespace braveledger_response_util

#endif  // BRAVELEDGER_COMMON_RESPONSE_UPHOLD_H_
