/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/database/database_migration.h"

#include <utility>

#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/database/database_util.h"
#include "bat/ads/internal/database/database_version.h"
#include "bat/ads/internal/database/tables/ad_conversions_database_table.h"
#include "bat/ads/internal/database/tables/categories_database_table.h"
#include "bat/ads/internal/database/tables/creative_ad_notifications_database_table.h"
#include "bat/ads/internal/database/tables/geo_targets_database_table.h"
#include "bat/ads/internal/logging.h"

using std::placeholders::_1;

namespace ads {
namespace database {

DatabaseMigration::DatabaseMigration(
    AdsImpl* ads)
    : ads_(ads) {
  DCHECK(ads_);
}

DatabaseMigration::~DatabaseMigration() = default;

void DatabaseMigration::MigrateFromVersion(
    const int from_version,
    ResultCallback callback) {
  const int to_version = version();
  if (to_version == from_version) {
    callback(Result::SUCCESS);
    return;
  }

  int migrated_to_version = from_version;

  auto transaction = DBTransaction::New();

  for (int i = from_version + 1; i <= to_version; i++) {
    if (!MigrateToVersion(transaction.get(), i)) {
      BLOG(0, "Error migrating database from version " << i - 1 << " to " << i);
      break;
    }

    migrated_to_version = i;
  }

  BLOG(1, "Migrated database from version " << from_version << " to version "
      << migrated_to_version);

  auto command = DBCommand::New();
  command->type = DBCommand::Type::MIGRATE;

  transaction->version = migrated_to_version;
  transaction->compatible_version = compatible_version();
  transaction->commands.push_back(std::move(command));

  ads_->get_ads_client()->RunDBTransaction(std::move(transaction),
      std::bind(&OnResultCallback, _1, callback));
}

bool DatabaseMigration::MigrateToVersion(
    DBTransaction* transaction,
    const int to_version) {
  DCHECK(transaction);

  AdConversionsDatabaseTable ad_conversions_database_table(ads_);
  if (!ad_conversions_database_table.Migrate(transaction, to_version)) {
    return false;
  }

  CategoriesDatabaseTable categories_database_table(ads_);
  if (!categories_database_table.Migrate(transaction, to_version)) {
    return false;
  }

  CreativeAdNotificationsDatabaseTable
      creative_ad_notifications_database_table(ads_);
  if (!creative_ad_notifications_database_table.Migrate(transaction,
      to_version)) {
    return false;
  }

  GeoTargetsDatabaseTable geo_targets_database_table(ads_);
  if (!geo_targets_database_table.Migrate(transaction, to_version)) {
    return false;
  }

  return true;
}

}  // namespace database
}  // namespace ads
