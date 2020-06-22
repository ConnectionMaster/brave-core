/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/response/response_publisher.h"

#include <utility>

#include "base/json/json_reader.h"
#include "bat/ledger/internal/logging.h"

namespace {

void ParsePublisherBanner(
    ledger::PublisherBanner* banner,
    base::Value* dictionary) {
  DCHECK(dictionary && banner);
  if (!dictionary->is_dict()) {
    return;
  }

  const auto* title = dictionary->FindStringKey("title");
  if (title) {
    banner->title = *title;
  }

  const auto* description = dictionary->FindStringKey("description");
  if (description) {
    banner->description = *description;
  }

  const auto* background = dictionary->FindStringKey("backgroundUrl");
  if (background && !background->empty()) {
    banner->background = "chrome://rewards-image/" + *background;
  }

  const auto* logo = dictionary->FindStringKey("logoUrl");
  if (logo && !logo->empty()) {
    banner->logo = "chrome://rewards-image/" + *logo;
  }

  const auto* amounts = dictionary->FindListKey("donationAmounts");
  if (amounts) {
    for (const auto& it : amounts->GetList()) {
      if (it.is_int()) {
        banner->amounts.push_back(it.GetInt());
      }
    }
  }

  const auto* links = dictionary->FindDictKey("socialLinks");
  if (links) {
    for (const auto& it : links->DictItems()) {
      if (it.second.is_string()) {
        banner->links.insert(std::make_pair(it.first, it.second.GetString()));
      }
    }
  }
}

ledger::PublisherStatus ParsePublisherStatus(const std::string& status) {
  if (status == "publisher_verified") {
    return ledger::PublisherStatus::CONNECTED;
  }

  if (status == "wallet_connected") {
    return ledger::PublisherStatus::VERIFIED;
  }

  return ledger::PublisherStatus::NOT_VERIFIED;
}

}  // namespace

namespace braveledger_response_util {

// Request Url: GET /api/v3/public/channels?page={page}
//
// Response Format:
// [
//   [
//     "laurenwags.github.io",
//     "wallet_connected",
//     false,
//     "abf1ff79-a239-42af-abff-20eb121edd1c",
//     {
//       "title": "Staging Banner Test",
//       "description": "Lorem ipsum dolor sit amet",
//       "backgroundUrl": "https://rewards-stg.bravesoftware.com/xrEJASVGN9nQ5zJUnmoCxjEE",
//       "logoUrl": "https://rewards-stg.bravesoftware.com/8eT9LXcpK3D795YHxvDdhrmg",
//       "donationAmounts": [
//         5,
//         10,
//         20
//       ],
//       "socialLinks": {
//         "youtube": "https://www.youtube.com/channel/UCCs7AQEDwrHEc86r0NNXE_A/videos",
//         "twitter": "https://twitter.com/bravelaurenwags",
//         "twitch": "https://www.twitch.tv/laurenwags"
//       }
//     }
//   ],
//   [
//     "bravesoftware.com",
//     "wallet_connected",
//     false,
//     "04c8dcae-9943-44a2-aa66-95203dca8b6c",
//     {}
//   ]
// ]

ledger::Result ParsePublisherListResponse(
    const ledger::UrlResponse& response,
    std::shared_ptr<std::vector<ledger::ServerPublisherPartial>> list_publisher,
    std::shared_ptr<std::vector<ledger::PublisherBanner>> list_banner) {
  DCHECK(list_publisher && list_banner);

  base::Optional<base::Value> value = base::JSONReader::Read(response.body);
  if (!value || !value->is_list()) {
    BLOG(0, "Invalid JSON");
    return ledger::Result::LEDGER_ERROR;
  }

  list_publisher->reserve(value->GetList().size());
  list_banner->reserve(value->GetList().size());

  for (auto& item : value->GetList()) {
    if (!item.is_list()) {
      continue;
    }

    const auto& list = item.GetList();

    if (list.size() != 5) {
      continue;
    }

    if (!list[0].is_string() || list[0].GetString().empty()  // Publisher key
        || !list[1].is_string()                              // Status
        || !list[2].is_bool()                                // Excluded
        || !list[3].is_string()) {                           // Address
      continue;
    }

    list_publisher->emplace_back(
        list[0].GetString(),
        ParsePublisherStatus(list[1].GetString()),
        list[2].GetBool(),
        list[3].GetString());

    // Banner
    if (!list[4].is_dict() || list[4].DictEmpty()) {
      continue;
    }

    list_banner->push_back(ledger::PublisherBanner());
    auto& banner = list_banner->back();
    ParsePublisherBanner(&banner, &list[4]);
    banner.publisher_key = list[0].GetString();
  }

  return ledger::Result::LEDGER_OK;
}

}  // namespace braveledger_response_util
