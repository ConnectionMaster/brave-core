/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_DATABASE_CATEGORIES_DATABASE_TABLE_H_
#define BAT_ADS_INTERNAL_DATABASE_CATEGORIES_DATABASE_TABLE_H_

#include <string>

#include "bat/ads/internal/creative_ad_notification_info.h"
#include "bat/ads/internal/database/database_table.h"

namespace ads {

class AdsImpl;

namespace database {

class CategoriesDatabaseTable : public DatabaseTable {
 public:
  explicit CategoriesDatabaseTable(
      AdsImpl* ads);

  ~CategoriesDatabaseTable() override;

  void AppendInsertOrUpdateQuery(
      DBTransaction* transaction,
      const CreativeAdNotificationList& creative_ad_notifications);

  bool Migrate(
      DBTransaction* transaction,
      const int to_version) override;

 private:
  int BindParameters(
      DBCommand* command,
      const CreativeAdNotificationList& creative_ad_notifications);

  std::string CreateInsertOrUpdateQuery(
      DBCommand* command,
      const CreativeAdNotificationList& creative_ad_notifications);

  bool CreateTableV1(
      DBTransaction* transaction);
  bool CreateIndexV1(
      DBTransaction* transaction);
  bool MigrateToV1(
      DBTransaction* transaction);

  AdsImpl* ads_;  // NOT OWNED
};

}  // namespace database
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_DATABASE_CATEGORIES_DATABASE_TABLE_H_
