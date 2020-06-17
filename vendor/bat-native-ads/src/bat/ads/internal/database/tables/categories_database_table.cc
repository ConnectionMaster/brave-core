/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/database/tables/categories_database_table.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/creative_ad_notification_info.h"
#include "bat/ads/internal/database/database_util.h"
#include "bat/ads/internal/logging.h"

using std::placeholders::_1;

namespace ads {
namespace database {

namespace {
const char kTableName[] = "categories";
}  // namespace

CategoriesDatabaseTable::CategoriesDatabaseTable(
    AdsImpl* ads)
    : ads_(ads) {
  DCHECK(ads_);
}

CategoriesDatabaseTable::~CategoriesDatabaseTable() = default;

void CategoriesDatabaseTable::AppendInsertOrUpdateQuery(
    DBTransaction* transaction,
    const CreativeAdNotificationList& creative_ad_notifications) {
  DCHECK(transaction);

  DeleteTable(transaction, kTableName);

  if (creative_ad_notifications.empty()) {
    return;
  }

  auto command = DBCommand::New();
  command->type = DBCommand::Type::RUN;
  command->command =
      CreateInsertOrUpdateQuery(command.get(), creative_ad_notifications);

  transaction->commands.push_back(std::move(command));
}

bool CategoriesDatabaseTable::Migrate(
    DBTransaction* transaction,
    const int to_version) {
  DCHECK(transaction);

  switch (to_version) {
    case 1: {
      return MigrateToV1(transaction);
    }

    default: {
      return true;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////

int CategoriesDatabaseTable::BindParameters(
    DBCommand* command,
    const CreativeAdNotificationList& creative_ad_notifications) {
  DCHECK(command);

  int count = 0;

  int index = 0;
  for (const auto& creative_ad_notification : creative_ad_notifications) {
    BindString(command, index++, creative_ad_notification.creative_instance_id);
    BindString(command, index++, creative_ad_notification.category);

    count++;
  }

  return count;
}

std::string CategoriesDatabaseTable::CreateInsertOrUpdateQuery(
    DBCommand* command,
    const CreativeAdNotificationList& creative_ad_notifications) {
  const int count = BindParameters(command, creative_ad_notifications);

  return base::StringPrintf(
      "INSERT OR REPLACE INTO %s "
          "(creative_instance_id, "
          "category) VALUES %s",
      kTableName,
      CreateBindingParameterPlaceholders(2, count).c_str());
}

bool CategoriesDatabaseTable::CreateTableV1(
    DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s "
          "(creative_instance_id TEXT NOT NULL, "
          "category TEXT NOT NULL, "
          "UNIQUE(creative_instance_id, category) ON CONFLICT REPLACE, "
          "CONSTRAINT fk_creative_instance_id "
              "FOREIGN KEY (creative_instance_id) "
              "REFERENCES creative_ad_notifications (creative_instance_id) "
              "ON DELETE CASCADE)",
      kTableName);

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  return true;
}

bool CategoriesDatabaseTable::CreateIndexV1(
    DBTransaction* transaction) {
  DCHECK(transaction);

  return CreateIndex(transaction, kTableName, "category");
}

bool CategoriesDatabaseTable::MigrateToV1(
    DBTransaction* transaction) {
  DCHECK(transaction);

  if (!DropTable(transaction, kTableName)) {
    BLOG(0, "Failed to drop table");
    return false;
  }

  if (!CreateTableV1(transaction)) {
    BLOG(0, "Failed to create table");
    return false;
  }

  if (!CreateIndexV1(transaction)) {
    BLOG(0, "Failed to create index");
    return false;
  }

  return true;
}

}  // namespace database
}  // namespace ads
