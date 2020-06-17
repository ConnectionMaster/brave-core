/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_DATABASE_CREATIVE_AD_NOTIFICATIONS_DATABASE_TABLE_H_
#define BAT_ADS_INTERNAL_DATABASE_CREATIVE_AD_NOTIFICATIONS_DATABASE_TABLE_H_

#include <string>
#include <vector>

#include "bat/ads/internal/classification/page_classifier/page_classifier.h"
#include "bat/ads/internal/creative_ad_notification_info.h"
#include "bat/ads/internal/database/database_table.h"

namespace ads {

using GetCreativeAdNotificationsCallback = std::function<void(const Result,
    const std::vector<std::string>&, const CreativeAdNotificationList&)>;

class AdsImpl;

namespace database {

class CreativeAdNotificationsDatabaseTable : public DatabaseTable {
 public:
  explicit CreativeAdNotificationsDatabaseTable(
      AdsImpl* ads);

  ~CreativeAdNotificationsDatabaseTable() override;

  void Save(
      const CreativeAdNotificationList& creative_ad_notifications,
      ResultCallback callback);

  void AppendInsertOrUpdateQuery(
      DBTransaction* transaction,
      const CreativeAdNotificationList& creative_ad_notifications);

  void GetCreativeAdNotifications(
      const std::vector<std::string>& categories,
      GetCreativeAdNotificationsCallback callback);

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

  void OnGetCreativeAdNotifications(
      DBCommandResponsePtr response,
      const classification::CategoryList& categories,
      GetCreativeAdNotificationsCallback callback);

  bool CreateTableV1(
      DBTransaction* transaction);
  bool MigrateToV1(
      DBTransaction* transaction);

  AdsImpl* ads_;  // NOT OWNED
};

}  // namespace database
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_DATABASE_CREATIVE_AD_NOTIFICATIONS_DATABASE_TABLE_H_
