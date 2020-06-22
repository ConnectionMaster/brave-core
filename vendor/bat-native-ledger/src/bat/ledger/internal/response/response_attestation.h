/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVELEDGER_COMMON_RESPONSE_ATTESTATION_H_
#define BRAVELEDGER_COMMON_RESPONSE_ATTESTATION_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "bat/ledger/mojom_structs.h"

namespace braveledger_response_util {

ledger::Result ParseStartAttestationResponse(
    const ledger::UrlResponse& response);

ledger::Result ParseCaptchaResponse(
    const ledger::UrlResponse& response,
    base::Value* result);

ledger::Result ParseCaptchaImageResponse(
    const ledger::UrlResponse& response,
    std::string* encoded_image);

ledger::Result ParseConfirmAttestationResponse(
    const ledger::UrlResponse& response);

}  // namespace braveledger_response_util

#endif  // BRAVELEDGER_COMMON_RESPONSE_ATTESTATION_H_
