/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVELEDGER_COMMON_RESPONSE_WALLET_H_
#define BRAVELEDGER_COMMON_RESPONSE_WALLET_H_

#include <string>

#include "bat/ledger/mojom_structs.h"

namespace braveledger_response_util {

ledger::BalancePtr ParseWalletFetchBalanceResponse(
    const ledger::UrlResponse& response);

ledger::Result ParseWalletRecoverKeyResponse(
    const ledger::UrlResponse& response,
    std::string* payment_id);

ledger::Result ParseRecoverWalletResponse(
    const ledger::UrlResponse& response,
    std::string* card_id,
    double* balance);

ledger::Result ParseWalletRegisterPersonaResponse(
    const ledger::UrlResponse& response,
    std::string* payment_id,
    std::string* card_id);

ledger::Result ParseWalletRequestCredentialsResponse(
    const ledger::UrlResponse& response);

ledger::Result ParseTransferAnonToExternalWalletResponse(
    const ledger::UrlResponse& response);

}  // namespace braveledger_response_util

#endif  // BRAVELEDGER_COMMON_RESPONSE_WALLET_H_
