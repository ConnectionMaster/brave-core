/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/publisher/server_publisher_fetcher.h"

#include <utility>

#include "base/big_endian.h"
#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "bat/ledger/internal/common/brotli_helpers.h"
#include "bat/ledger/internal/ledger_impl.h"
#include "bat/ledger/internal/publisher/channel_response.pb.h"
#include "bat/ledger/internal/publisher/prefix_util.h"
#include "bat/ledger/internal/request/request_publisher.h"
#include "bat/ledger/option_keys.h"
#include "brave_base/random.h"
#include "net/http/http_status_code.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace {

constexpr size_t kQueryPrefixBytes = 2;

int64_t GetCacheExpiryInSeconds(bat_ledger::LedgerImpl* ledger) {
  DCHECK(ledger);
  // NOTE: We are reusing the publisher prefix list refresh interval for
  // determining the cache lifetime of publisher details. At a later
  // time we may want to introduce an additional option for this value.
  return ledger->GetUint64Option(ledger::kOptionPublisherListRefreshInterval);
}

ledger::PublisherStatus PublisherStatusFromMessage(
    const publishers_pb::ChannelResponse& response) {
  switch (response.wallet_connected_state()) {
    case publishers_pb::UPHOLD_ACCOUNT_KYC:
      return ledger::PublisherStatus::VERIFIED;
    case publishers_pb::UPHOLD_ACCOUNT_NO_KYC:
      return ledger::PublisherStatus::CONNECTED;
    default:
      return ledger::PublisherStatus::NOT_VERIFIED;
  }
}

ledger::PublisherBannerPtr PublisherBannerFromMessage(
    const publishers_pb::SiteBannerDetails& banner_details) {
  auto banner = ledger::PublisherBanner::New();

  banner->title = banner_details.title();
  banner->description = banner_details.description();

  if (!banner_details.background_url().empty()) {
    banner->background =
        "chrome://rewards-image/" + banner_details.background_url();
  }

  if (!banner_details.logo_url().empty()) {
    banner->logo = "chrome://rewards-image/" + banner_details.logo_url();
  }

  for (auto& amount : banner_details.donation_amounts()) {
    banner->amounts.push_back(amount);
  }

  if (banner_details.has_social_links()) {
    auto& links = banner_details.social_links();
    if (!links.youtube().empty()) {
      banner->links.insert(std::make_pair("youtube", links.youtube()));
    }
    if (!links.twitter().empty()) {
      banner->links.insert(std::make_pair("twitter", links.twitter()));
    }
    if (!links.twitch().empty()) {
      banner->links.insert(std::make_pair("twitch", links.twitch()));
    }
  }

  return banner;
}

ledger::ServerPublisherInfoPtr ServerPublisherInfoFromMessage(
    const publishers_pb::ChannelResponseList& message,
    const std::string& expected_key) {
  for (const auto& entry : message.channel_responses()) {
    if (entry.channel_identifier() != expected_key) {
      continue;
    }

    auto server_info = ledger::ServerPublisherInfo::New();
    server_info->publisher_key = entry.channel_identifier();
    server_info->status = PublisherStatusFromMessage(entry);
    server_info->address = entry.wallet_address();
    server_info->updated_at =
        static_cast<uint64_t>(base::Time::Now().ToDoubleT());

    if (entry.has_site_banner_details()) {
      server_info->banner =
          PublisherBannerFromMessage(entry.site_banner_details());
    }

    return server_info;
  }

  return nullptr;
}

bool RemovePadding(base::StringPiece* padded_string) {
  if (!padded_string) {
    return false;
  }

  // Read payload length from the header.
  if (padded_string->size() < sizeof(uint32_t)) {
    return false;
  }
  uint32_t data_length;
  base::ReadBigEndian(padded_string->data(), &data_length);

  // Remove length header.
  padded_string->remove_prefix(sizeof(uint32_t));
  if (padded_string->size() < data_length) {
    return false;
  }

  // Remove padding.
  padded_string->remove_suffix(padded_string->size() - data_length);
  return true;
}

bool DecompressMessage(base::StringPiece payload, std::string* output) {
  constexpr size_t buffer_size = 32 * 1024;
  return braveledger_helpers::DecodeBrotliStringWithBuffer(
      payload,
      buffer_size,
      output);
}

}  // namespace

namespace braveledger_publisher {

ServerPublisherFetcher::ServerPublisherFetcher(
    bat_ledger::LedgerImpl* ledger)
    : ledger_(ledger) {
  DCHECK(ledger);
}

ServerPublisherFetcher::~ServerPublisherFetcher() = default;

void ServerPublisherFetcher::Fetch(
    const std::string& publisher_key,
    ledger::GetServerPublisherInfoCallback callback) {
  CallbackVector& callbacks = callback_map_[publisher_key];
  callbacks.push_back(callback);
  if (callbacks.size() > 1) {
    BLOG(1, "Fetch already in progress for publisher " << publisher_key);
    return;
  }

  BLOG(1, "Fetching server publisher info for " << publisher_key);

  std::string hex_prefix = GetHashPrefixInHex(
      publisher_key,
      kQueryPrefixBytes);

  // Due to privacy concerns, the request length must be consistent
  // for all publisher lookups. Do not add URL parameters or headers
  // whose size will vary depending on the publisher key.
  std::string url = braveledger_request_util::GetPublisherInfoUrl(hex_prefix);
  ledger_->LoadURL(
      url, {}, "", "",
      ledger::UrlMethod::GET,
      std::bind(&ServerPublisherFetcher::OnFetchCompleted,
          this, publisher_key, _1));
}

void ServerPublisherFetcher::OnFetchCompleted(
    const std::string& publisher_key,
    const ledger::UrlResponse& response) {
  auto server_info = ParseResponse(
      publisher_key,
      response.status_code,
      response.body);

  if (!server_info) {
    RunCallbacks(publisher_key, nullptr);
    return;
  }

  // Create a shared pointer to a mojo struct so that it can be copied
  // into a callback.
  auto shared_info = std::make_shared<ledger::ServerPublisherInfoPtr>(
      std::move(server_info));

  // Store the result for subsequent lookups.
  ledger_->InsertServerPublisherInfo(**shared_info,
      [this, publisher_key, shared_info](ledger::Result result) {
        if (result != ledger::Result::LEDGER_OK) {
          BLOG(0, "Error saving server publisher info record");
        }
        RunCallbacks(publisher_key, std::move(*shared_info));
      });
}

ledger::ServerPublisherInfoPtr ServerPublisherFetcher::ParseResponse(
    const std::string& publisher_key,
    int response_status_code,
    const std::string& response) {
  if (response_status_code == net::HTTP_NOT_FOUND) {
    return GetServerInfoForEmptyResponse(publisher_key);
  }

  if (response_status_code != net::HTTP_OK || response.empty()) {
    BLOG(0, "Server returned an invalid response from publisher data URL");
    return nullptr;
  }

  base::StringPiece response_payload(response.data(), response.size());
  if (!RemovePadding(&response_payload)) {
    BLOG(0, "Publisher data response has invalid padding");
    return nullptr;
  }

  std::string message_string;
  if (!DecompressMessage(response_payload, &message_string)) {
    BLOG(1, "Error decompressing publisher data response. "
        "Attempting to parse as uncompressed message.");
    message_string.assign(response_payload.data(), response_payload.size());
  }

  publishers_pb::ChannelResponseList message;
  if (!message.ParseFromString(message_string)) {
    BLOG(0, "Error parsing publisher data protobuf message");
    return nullptr;
  }

  auto server_info = ServerPublisherInfoFromMessage(message, publisher_key);
  if (!server_info) {
    return GetServerInfoForEmptyResponse(publisher_key);
  }

  return server_info;
}

bool ServerPublisherFetcher::IsExpired(base::Time last_update_time) {
  base::TimeDelta age = base::Time::Now() - last_update_time;

  if (age.InSeconds() < 0) {
    // A negative age value indicates that either the data is
    // corrupted or that we are incorrectly storing the timestamp.
    // Pessimistically assume that we are incorrectly storing
    // the timestamp in order to avoid a case where we fetch
    // on every tab update.
    BLOG(0, "Server publisher info has a future updated_at time.");
  }

  return age.InSeconds() > GetCacheExpiryInSeconds(ledger_);
}

bool ServerPublisherFetcher::IsExpired(
    ledger::ServerPublisherInfo* server_info) {
  return server_info
      ? IsExpired(base::Time::FromDoubleT(server_info->updated_at))
      : true;
}

void ServerPublisherFetcher::PurgeExpiredRecords() {
  BLOG(1, "Purging expired server publisher info records");
  int64_t max_age = GetCacheExpiryInSeconds(ledger_) * 2;
  ledger_->DeleteExpiredServerPublisherInfo(max_age, [](auto result) {});
}

ledger::ServerPublisherInfoPtr
ServerPublisherFetcher::GetServerInfoForEmptyResponse(
    const std::string& publisher_key) {
  // The server has indicated that a publisher record does not exist
  // for this publisher key, perhaps as a result of a false positive
  // when searching the publisher prefix list. Create a "non-verified"
  // record that can be cached in the database so that we don't repeatedly
  // attempt to fetch from the server for this publisher.
  BLOG(1, "Server did not return an entry for publisher " << publisher_key);
  auto server_info = ledger::ServerPublisherInfo::New();
  server_info->publisher_key = publisher_key;
  server_info->status = ledger::PublisherStatus::NOT_VERIFIED;
  server_info->updated_at =
      static_cast<uint64_t>(base::Time::Now().ToDoubleT());
  return server_info;
}

ServerPublisherFetcher::CallbackVector ServerPublisherFetcher::GetCallbacks(
    const std::string& publisher_key) {
  CallbackVector callbacks;
  auto iter = callback_map_.find(publisher_key);
  if (iter != callback_map_.end()) {
    callbacks = std::move(iter->second);
    callback_map_.erase(iter);
  }
  return callbacks;
}

void ServerPublisherFetcher::RunCallbacks(
    const std::string& publisher_key,
    ledger::ServerPublisherInfoPtr server_info) {
  CallbackVector callbacks = GetCallbacks(publisher_key);
  DCHECK(!callbacks.empty());
  for (auto& callback : callbacks) {
    callback(server_info ? server_info.Clone() : nullptr);
  }
}

}  // namespace braveledger_publisher
