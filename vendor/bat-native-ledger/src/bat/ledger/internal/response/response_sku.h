/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVELEDGER_COMMON_RESPONSE_SKU_H_
#define BRAVELEDGER_COMMON_RESPONSE_SKU_H_

#include <string>
#include <vector>

#include "bat/ledger/mojom_structs.h"

namespace braveledger_response_util {

ledger::Result ParseSendExternalTransactionResponse(
    const ledger::UrlResponse& response);

ledger::SKUOrderPtr ParseOrderCreateResponse(
    const ledger::UrlResponse& response,
    const std::vector<ledger::SKUOrderItem>& order_items);

ledger::Result ParseClaimSKUCredsResponse(const ledger::UrlResponse& response);

ledger::Result ParseRedeemSKUTokensResponse(
    const ledger::UrlResponse& response);

}  // namespace braveledger_response_util

#endif  // BRAVELEDGER_COMMON_RESPONSE_SKU_H_
