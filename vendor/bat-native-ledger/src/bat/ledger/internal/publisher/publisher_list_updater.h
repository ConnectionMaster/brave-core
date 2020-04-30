/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVELEDGER_PUBLISHER_PUBLISHER_LIST_UPDATER_H_
#define BRAVELEDGER_PUBLISHER_PUBLISHER_LIST_UPDATER_H_

#include <functional>
#include <map>
#include <string>

#include "base/timer/timer.h"
#include "bat/ledger/ledger.h"

namespace bat_ledger {
class LedgerImpl;
}

namespace braveledger_publisher {

// Automatically updates the publisher prefix list store on regular
// intervals.
class PublisherListUpdater {
 public:
  explicit PublisherListUpdater(bat_ledger::LedgerImpl* ledger);

  PublisherListUpdater(const PublisherListUpdater&) = delete;
  PublisherListUpdater& operator=(const PublisherListUpdater&) = delete;

  ~PublisherListUpdater();

  // Starts the auto updater
  void StartAutoUpdate();

  // Cancels the auto updater
  void StopAutoUpdate();

  using OnPublisherListUpdatedCallback = std::function<void()>;

  // Sets a callback that will be executed after the publisher
  // list has been updated
  void SetOnPublisherListUpdatedCallback(
      OnPublisherListUpdatedCallback callback);

 private:
  void StartFetchTimer(
      const base::Location& posted_from,
      base::TimeDelta delay);

  void OnFetchTimerElapsed();
  void OnFetchCompleted(const ledger::UrlResponse& response);
  void OnPublisherListInserted(ledger::Result result);

  base::TimeDelta GetAutoUpdateDelay();
  base::TimeDelta GetRetryAfterFailureDelay();

  bat_ledger::LedgerImpl* ledger_;  // NOT OWNED
  base::OneShotTimer timer_;
  bool auto_update_ = false;
  int retry_count_ = 0;
  OnPublisherListUpdatedCallback on_updated_callback_;
};

}  // namespace braveledger_publisher

#endif  // BRAVELEDGER_PUBLISHER_PUBLISHER_LIST_UPDATER_H_
