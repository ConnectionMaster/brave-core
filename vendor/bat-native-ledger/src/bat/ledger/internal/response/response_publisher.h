/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVELEDGER_COMMON_RESPONSE_PUBLISHER_H_
#define BRAVELEDGER_COMMON_RESPONSE_PUBLISHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "bat/ledger/mojom_structs.h"

namespace braveledger_response_util {

ledger::Result ParsePublisherListResponse(
    const ledger::UrlResponse& response,
    std::shared_ptr<std::vector<ledger::ServerPublisherPartial>> list_publisher,
    std::shared_ptr<std::vector<ledger::PublisherBanner>> list_banner);

}  // namespace braveledger_response_util

#endif  // BRAVELEDGER_COMMON_RESPONSE_PUBLISHER_H_
