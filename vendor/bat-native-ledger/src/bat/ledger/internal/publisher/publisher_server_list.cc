/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "bat/ledger/internal/common/time_util.h"
#include "bat/ledger/internal/ledger_impl.h"
#include "bat/ledger/internal/publisher/publisher_server_list.h"
#include "bat/ledger/internal/request/request_publisher.h"
#include "bat/ledger/internal/request/request_util.h"
#include "bat/ledger/internal/response/response_publisher.h"
#include "bat/ledger/internal/state/state_keys.h"
#include "bat/ledger/internal/static_values.h"
#include "bat/ledger/option_keys.h"
#include "brave_base/random.h"
#include "net/http/http_status_code.h"

#if defined(OS_IOS)
#include <dispatch/dispatch.h>
#endif

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace {

const int kHardLimit = 100;

}  // namespace

namespace braveledger_publisher {

PublisherServerList::PublisherServerList(bat_ledger::LedgerImpl* ledger) :
    ledger_(ledger),
    server_list_timer_id_(0ull) {
}

PublisherServerList::~PublisherServerList() {
}

void PublisherServerList::OnTimer(uint32_t timer_id) {
  if (timer_id == server_list_timer_id_) {
    server_list_timer_id_ = 0;
    Start([](const ledger::Result _){});
  }
}

void PublisherServerList::Start(ledger::ResultCallback callback) {
  if (in_progress_) {
    BLOG(1, "Publisher list in progress");
    callback(ledger::Result::LEDGER_OK);
    return;
  }

  in_progress_ = true;
  current_page_ = 1;

  Download(callback);
}

void PublisherServerList::Download(ledger::ResultCallback callback) {
  std::vector<std::string> headers;
  headers.push_back("Accept-Encoding: gzip");

  const std::string url =
      braveledger_request_util::GetPublisherListUrl(current_page_);

  const ledger::LoadURLCallback download_callback = std::bind(
      &PublisherServerList::OnDownload,
      this,
      _1,
      callback);

  ledger_->LoadURL(
      url,
      headers,
      "",
      "",
      ledger::UrlMethod::GET,
      download_callback);
}

void PublisherServerList::OnDownload(
    const ledger::UrlResponse& response,
    ledger::ResultCallback callback) {
  BLOG(7, ledger::UrlResponseToString(__func__, response));

  // we iterated through all pages
  if (response.status_code == net::HTTP_NO_CONTENT) {
    in_progress_ = false;
    OnParsePublisherList(ledger::Result::LEDGER_OK, callback);
    return;
  }

  if (response.status_code == net::HTTP_OK && !response.body.empty()) {
    const auto parse_callback =
      std::bind(&PublisherServerList::OnParsePublisherList, this, _1, callback);
#if defined(OS_IOS)
    // Make sure the data is copied into block
    dispatch_queue_global_t global_queue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_async(global_queue, ^{
      this->ParsePublisherList(response, parse_callback);
    });
#else
    ParsePublisherList(response, parse_callback);
#endif
    return;
  }

  BLOG(0, "Can't fetch publisher list");
  SetTimer(true);
  callback(ledger::Result::LEDGER_ERROR);
}

void PublisherServerList::OnParsePublisherList(
    const ledger::Result result,
    ledger::ResultCallback callback) {
  if (result == ledger::Result::CONTINUE && current_page_ < kHardLimit) {
    current_page_++;
    Download(callback);
    return;
  }

  uint64_t new_time = 0ull;
  if (result != ledger::Result::LEDGER_ERROR) {
    ledger_->ContributeUnverifiedPublishers();
    new_time = braveledger_time_util::GetCurrentTimeStamp();
  }

  ledger_->SetUint64State(ledger::kStateServerPublisherListStamp, new_time);

  in_progress_ = false;
  bool retry_after_error = result != ledger::Result::LEDGER_OK;
  SetTimer(retry_after_error);

  callback(result);
}

void PublisherServerList::SetTimer(bool retry_after_error) {
  auto start_timer_in = 0ull;

  if (server_list_timer_id_ != 0) {
    // timer in progress
    return;
  }

  uint64_t last_download =
      ledger_->GetUint64State(ledger::kStateServerPublisherListStamp);
  start_timer_in = GetTimerTime(retry_after_error, last_download);

  // Start downloading right away
  if (start_timer_in == 0ull) {
    OnTimer(server_list_timer_id_);
    return;
  }

  // start timer
  ledger_->SetTimer(start_timer_in, &server_list_timer_id_);
}

uint64_t PublisherServerList::GetTimerTime(
    bool retry_after_error,
    const uint64_t last_download) {
  auto start_timer_in = 0ull;
  if (retry_after_error) {
    start_timer_in = brave_base::random::Geometric(150);

    BLOG(1, "Failed to refresh server list, will try again in "
        << start_timer_in << " seconds.");

    return start_timer_in;
  }

  uint64_t now_seconds = braveledger_time_util::GetCurrentTimeStamp();

  // check if last_download doesn't exist or have erroneous value.
  // (start_timer_in == 0) is expected to call callback function immediately.

  // time since last successful download
  uint64_t  time_since_last_download =
      (last_download == 0ull || last_download > now_seconds)
      ? 0ull
      : now_seconds - last_download;

  const uint64_t interval =
      ledger_->GetUint64Option(ledger::kOptionPublisherListRefreshInterval);

  if (now_seconds == last_download) {
    start_timer_in = interval;
  } else if (time_since_last_download > 0 &&
             time_since_last_download < interval) {
    start_timer_in = interval - time_since_last_download;
  } else {
    start_timer_in = 0ull;
  }

  return start_timer_in;
}

void PublisherServerList::ParsePublisherList(
    const ledger::UrlResponse& response,
    ledger::ResultCallback callback) {
  auto list_publisher =
      std::make_shared<std::vector<ledger::ServerPublisherPartial>>();
  auto list_banner = std::make_shared<std::vector<ledger::PublisherBanner>>();

  ledger::Result result = braveledger_response_util::ParsePublisherListResponse(
      response,
      list_publisher,
      list_banner);
  if (result != ledger::Result::LEDGER_OK) {
    BLOG(0, "Data is not correct");
    callback(ledger::Result::LEDGER_ERROR);
    return;
  }

  if (list_publisher->empty()) {
    BLOG(0, "Publisher list is empty");
    callback(ledger::Result::LEDGER_ERROR);
    return;
  }

  // we need to clear table when we process first page, but only once
  if (current_page_ == 1) {
    auto clear_callback = std::bind(&PublisherServerList::SaveParsedData,
      this,
      _1,
      list_publisher,
      list_banner,
      callback);

    ledger_->ClearServerPublisherList(clear_callback);
    return;
  }

  SaveParsedData(
      ledger::Result::LEDGER_OK,
      list_publisher,
      list_banner,
      callback);
}

void PublisherServerList::SaveParsedData(
    const ledger::Result result,
    const SharedServerPublisherPartial& list_publisher,
    const SharedPublisherBanner& list_banner,
    ledger::ResultCallback callback) {
  if (result != ledger::Result::LEDGER_OK) {
    BLOG(0, "DB was not cleared");
    callback(result);
    return;
  }

  if (!list_publisher || list_publisher->empty()) {
    BLOG(0, "Publisher list is null");
    callback(ledger::Result::LEDGER_ERROR);
    return;
  }

  auto save_callback = std::bind(&PublisherServerList::SaveBanners,
      this,
      _1,
      list_banner,
      callback);

  ledger_->InsertServerPublisherList(*list_publisher, save_callback);
}

void PublisherServerList::SaveBanners(
    const ledger::Result result,
    const SharedPublisherBanner& list_banner,
    ledger::ResultCallback callback) {
  if (!list_banner || result != ledger::Result::LEDGER_OK) {
    BLOG(0, "Publisher list was not saved");
    callback(ledger::Result::LEDGER_ERROR);
    return;
  }

  if (list_banner->empty()) {
    callback(ledger::Result::CONTINUE);
    return;
  }

  auto save_callback = std::bind(&PublisherServerList::BannerSaved,
      this,
      _1,
      callback);

  ledger_->InsertPublisherBannerList(*list_banner, save_callback);
}

void PublisherServerList::BannerSaved(
    const ledger::Result result,
    ledger::ResultCallback callback) {
  if (result == ledger::Result::LEDGER_OK) {
    callback(ledger::Result::CONTINUE);
    return;
  }


  BLOG(0, "Banners were not saved");
  callback(result);
}

void PublisherServerList::ClearTimer() {
  server_list_timer_id_ = 0;
}

}  // namespace braveledger_publisher
