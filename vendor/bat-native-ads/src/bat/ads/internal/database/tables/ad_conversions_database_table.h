/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_DATABASE_AD_CONVERSIONS_DATABASE_TABLE_H_
#define BAT_ADS_INTERNAL_DATABASE_AD_CONVERSIONS_DATABASE_TABLE_H_

#include <string>

#include "bat/ads/internal/ad_conversion_info.h"
#include "bat/ads/internal/database/database_table.h"

namespace ads {

using GetAdConversionsCallback = std::function<void(const Result,
    const AdConversionList&)>;

class AdsImpl;

namespace database {

class AdConversionsDatabaseTable : public DatabaseTable {
 public:
  explicit AdConversionsDatabaseTable(
      AdsImpl* ads);

  ~AdConversionsDatabaseTable() override;

  void Save(
      const AdConversionList& ad_conversions,
      ResultCallback callback);

  void AppendInsertOrUpdateQuery(
      DBTransaction* transaction,
      const AdConversionList& ad_conversion);

  void GetAdConversions(
      GetAdConversionsCallback callback);

  void PurgeExpiredAdConversions(
      ResultCallback callback);

  bool Migrate(
      DBTransaction* transaction,
      const int to_version) override;

 private:
  int BindParameters(
      DBCommand* command,
      const AdConversionList& ad_conversion);

  std::string CreateInsertOrUpdateQuery(
      DBCommand* command,
      const AdConversionList& ad_conversions);

  void OnGetAdConversions(
      DBCommandResponsePtr response,
      GetAdConversionsCallback callback);

  bool CreateTableV1(
      DBTransaction* transaction);
  bool MigrateToV1(
      DBTransaction* transaction);

  AdsImpl* ads_;  // NOT OWNED
};

}  // namespace database
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_DATABASE_AD_CONVERSIONS_DATABASE_TABLE_H_
