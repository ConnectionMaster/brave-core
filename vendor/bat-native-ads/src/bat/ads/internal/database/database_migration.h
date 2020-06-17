/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_DATABASE_DATABASE_MIGRATION_H_
#define BAT_ADS_INTERNAL_DATABASE_DATABASE_MIGRATION_H_

#include "bat/ads/ads.h"

namespace ads {

class AdsImpl;

namespace database {

class DatabaseMigration {
 public:
  explicit DatabaseMigration(
      AdsImpl* ads);

  ~DatabaseMigration();

  void MigrateFromVersion(
      const int from_version,
      ResultCallback callback);

 private:
  bool MigrateToVersion(
      DBTransaction* transaction,
      const int to_version);

  AdsImpl* ads_;  // NOT OWNED
};

}  // namespace database
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_DATABASE_DATABASE_MIGRATION_H_
