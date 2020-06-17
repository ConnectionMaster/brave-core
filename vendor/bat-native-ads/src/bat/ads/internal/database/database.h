/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_DATABASE_DATABASE_H_
#define BAT_ADS_INTERNAL_DATABASE_DATABASE_H_

#include <string>

#include "bat/ads/ads.h"

namespace ads {

class AdsImpl;

namespace database {

class Database {
 public:
  explicit Database(
      AdsImpl* ads);

  ~Database();

  void Initialize(
      ResultCallback callback);

  std::string get_last_message() const;

 private:
  void OnInitialize(
      DBCommandResponsePtr response,
      ResultCallback callback);

  std::string last_message_;

  AdsImpl* ads_;  // NOT OWNED
};

}  // namespace database
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_DATABASE_DATABASE_H_
