/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/response/response_promotion.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "bat/ledger/internal/logging.h"
#include "bat/ledger/internal/promotion/promotion_util.h"
#include "net/http/http_status_code.h"

namespace braveledger_response_util {

// Request Url: POST /v1/promotions/{promotion_id}
//
// Response Format:
// {
//   "claimId": "53714048-9675-419e-baa3-369d85a2facb"
// }

ledger::Result ParseClaimCredsResponse(
    const ledger::UrlResponse& response,
    std::string* claim_id) {
  DCHECK(claim_id);

  if (response.status_code != net::HTTP_OK) {
    return ledger::Result::LEDGER_ERROR;
  }

  base::Optional<base::Value> value = base::JSONReader::Read(response.body);
  if (!value || !value->is_dict()) {
    BLOG(0, "Invalid JSON");
    return ledger::Result::LEDGER_ERROR;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    BLOG(0, "Invalid JSON");
    return ledger::Result::LEDGER_ERROR;
  }

  auto* id = dictionary->FindStringKey("claimId");
  if (!id || id->empty()) {
    BLOG(0, "Claim id is missing");
    return ledger::Result::LEDGER_ERROR;
  }

  *claim_id = *id;

  return ledger::Result::LEDGER_OK;
}

// Request Url: GET
// /v1/promotions?migrate=true&paymentId={payment_id}&platform={platform} // NOLINT
//
// Response Format:
// {
//   "promotions": [
//     {
//       "id": "83b3b77b-e7c3-455b-adda-e476fa0656d2",
//       "createdAt": "2020-06-08T15:04:45.352584Z",
//       "expiresAt": "2020-10-08T15:04:45.352584Z",
//       "version": 5,
//       "suggestionsPerGrant": 120,
//       "approximateValue": "30",
//       "type": "ugp",
//       "available": true,
//       "platform": "desktop",
//       "publicKeys": [
//         "dvpysTSiJdZUPihius7pvGOfngRWfDiIbrowykgMi1I="
//       ],
//       "legacyClaimed": false
//     }
//   ]
// }

ledger::Result ParseFetchPromotionsResponse(
    const ledger::UrlResponse& response,
    ledger::PromotionList* list,
    std::vector<std::string>* corrupted_promotions) {
  DCHECK(corrupted_promotions);

  if (!list) {
    return ledger::Result::LEDGER_ERROR;
  }

  base::Optional<base::Value> value = base::JSONReader::Read(response.body);
  if (!value || !value->is_dict()) {
    BLOG(0, "Invalid JSON");
    return ledger::Result::LEDGER_ERROR;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    BLOG(0, "Invalid JSON");
    return ledger::Result::LEDGER_ERROR;
  }

  auto* promotions = dictionary->FindListKey("promotions");
  if (promotions) {
    const auto promotion_size = promotions->GetList().size();
    for (auto& item : promotions->GetList()) {
      ledger::PromotionPtr promotion = ledger::Promotion::New();

      const auto* id = item.FindStringKey("id");
      if (!id) {
        continue;
      }
      promotion->id = *id;

      const auto version = item.FindIntKey("version");
      if (!version) {
        corrupted_promotions->push_back(promotion->id);
        continue;
      }
      promotion->version = *version;

      const auto* type = item.FindStringKey("type");
      if (!type) {
        corrupted_promotions->push_back(promotion->id);
        continue;
      }
      promotion->type =
          braveledger_promotion::ConvertStringToPromotionType(*type);

      const auto suggestions = item.FindIntKey("suggestionsPerGrant");
      if (!suggestions) {
        corrupted_promotions->push_back(promotion->id);
        continue;
      }
      promotion->suggestions = *suggestions;

      const auto* approximate_value = item.FindStringKey("approximateValue");
      if (!approximate_value) {
        corrupted_promotions->push_back(promotion->id);
        continue;
      }
      promotion->approximate_value = std::stod(*approximate_value);

      const auto available = item.FindBoolKey("available");
      if (!available) {
        corrupted_promotions->push_back(promotion->id);
        continue;
      }

      if (*available) {
        promotion->status = ledger::PromotionStatus::ACTIVE;
      } else {
        promotion->status = ledger::PromotionStatus::OVER;
      }

      auto* expires_at = item.FindStringKey("expiresAt");
      if (!expires_at) {
        corrupted_promotions->push_back(promotion->id);
        continue;
      }

      base::Time time;
      bool success =  base::Time::FromUTCString((*expires_at).c_str(), &time);
      if (success) {
        promotion->expires_at = time.ToDoubleT();
      }

      auto* public_keys = item.FindListKey("publicKeys");
      if (!public_keys || public_keys->GetList().empty()) {
        corrupted_promotions->push_back(promotion->id);
        continue;
      }

      std::string keys_json;
      base::JSONWriter::Write(*public_keys, &keys_json);
      promotion->public_keys = keys_json;

      auto legacy_claimed = item.FindBoolKey("legacyClaimed");
      promotion->legacy_claimed = legacy_claimed.value_or(false);

      list->push_back(std::move(promotion));
    }

    if (promotion_size != list->size()) {
      return ledger::Result::CORRUPTED_DATA;
    }
  }

  return ledger::Result::LEDGER_OK;
}

// Request Url: POST /v1/promotions/reportclobberedclaims
//
// Response Format:
// {Empty body}

ledger::Result ParseCorruptedPromotionsResponse(
    const ledger::UrlResponse& response) {
  if (response.status_code != net::HTTP_OK) {
    return ledger::Result::LEDGER_ERROR;
  }

  return ledger::Result::LEDGER_OK;
}

// Request Url: POST /v1/suggestions
//
// Response Format:
// {Empty body}

ledger::Result ParseRedeemTokensResponse(const ledger::UrlResponse& response) {
  if (response.status_code != net::HTTP_OK) {
    return ledger::Result::LEDGER_ERROR;
  }

  return ledger::Result::LEDGER_OK;
}

}  // namespace braveledger_response_util
