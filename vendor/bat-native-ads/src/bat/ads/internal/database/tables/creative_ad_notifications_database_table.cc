/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/database/tables/creative_ad_notifications_database_table.h"

#include <limits>
#include <utility>

#include "base/strings/stringprintf.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/creative_ad_notification_info.h"
#include "bat/ads/internal/database/database_util.h"
#include "bat/ads/internal/database/tables/categories_database_table.h"
#include "bat/ads/internal/database/tables/geo_targets_database_table.h"
#include "bat/ads/internal/logging.h"

using std::placeholders::_1;

namespace ads {
namespace database {

namespace {
const char kTableName[] = "creative_ad_notifications";
}  // namespace

CreativeAdNotificationsDatabaseTable::CreativeAdNotificationsDatabaseTable(
    AdsImpl* ads)
    : ads_(ads) {
  DCHECK(ads_);
}

CreativeAdNotificationsDatabaseTable::
~CreativeAdNotificationsDatabaseTable() = default;

void CreativeAdNotificationsDatabaseTable::Save(
    const CreativeAdNotificationList& creative_ad_notifications,
    ResultCallback callback) {
  auto transaction = DBTransaction::New();

  std::vector<CreativeAdNotificationList> chunks =
      ChunkVector(creative_ad_notifications, 50);

  for (const auto& chunk : chunks) {
    AppendInsertOrUpdateQuery(transaction.get(), chunk);

    CategoriesDatabaseTable categories_database_table(ads_);
    categories_database_table.AppendInsertOrUpdateQuery(transaction.get(),
        chunk);

    GeoTargetsDatabaseTable geo_targets_database_table(ads_);
    geo_targets_database_table.AppendInsertOrUpdateQuery(transaction.get(),
        chunk);
  }

  ads_->get_ads_client()->RunDBTransaction(std::move(transaction),
      std::bind(&OnResultCallback, _1, callback));
}

void CreativeAdNotificationsDatabaseTable::AppendInsertOrUpdateQuery(
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

void CreativeAdNotificationsDatabaseTable::GetCreativeAdNotifications(
    const std::vector<std::string>& categories,
    GetCreativeAdNotificationsCallback callback) {
  if (categories.empty()) {
    callback(Result::SUCCESS, categories, {});
    return;
  }

  const std::string query = base::StringPrintf(
      "SELECT "
          "can.creative_instance_id, "
          "can.creative_set_id, "
          "can.campaign_id, "
          "can.start_at_timestamp, "
          "can.end_at_timestamp, "
          "can.daily_cap, "
          "can.advertiser_id, "
          "can.priority, "
          "can.conversion, "
          "can.per_day, "
          "can.total_max, "
          "c.category, "
          "gt.geo_target, "
          "can.target_url, "
          "can.title, "
          "can.body "
      "FROM %s AS can "
          "INNER JOIN categories AS c "
              "ON c.creative_instance_id = can.creative_instance_id "
          "INNER JOIN geo_targets AS gt "
              "ON gt.creative_instance_id = can.creative_instance_id "
      "WHERE c.category IN %s "
          "AND strftime('%%s', 'now') "
              "BETWEEN can.start_timestamp AND can.end_timestamp",
      kTableName,
      CreateBindingParameterPlaceholder(categories.size()).c_str());

  auto command = DBCommand::New();
  command->type = DBCommand::Type::READ;
  command->command = query;

  int index = 0;
  for (const auto& category : categories) {
    BindString(command.get(), index, category.c_str());
    index++;
  }

  command->record_bindings = {
    DBCommand::RecordBindingType::STRING_TYPE,  // creative_instance_id
    DBCommand::RecordBindingType::STRING_TYPE,  // creative_set_id
    DBCommand::RecordBindingType::STRING_TYPE,  // campaign_id
    DBCommand::RecordBindingType::INT64_TYPE,   // start_at_timestamp
    DBCommand::RecordBindingType::INT64_TYPE,   // end_at_timestamp
    DBCommand::RecordBindingType::INT_TYPE,     // daily_cap
    DBCommand::RecordBindingType::STRING_TYPE,  // advertiser_id
    DBCommand::RecordBindingType::INT_TYPE,     // priority
    DBCommand::RecordBindingType::BOOL_TYPE,    // conversion
    DBCommand::RecordBindingType::INT_TYPE,     // per_day
    DBCommand::RecordBindingType::INT_TYPE,     // total_max
    DBCommand::RecordBindingType::STRING_TYPE,  // category
    DBCommand::RecordBindingType::STRING_TYPE,  // geo_target
    DBCommand::RecordBindingType::STRING_TYPE,  // target_url
    DBCommand::RecordBindingType::STRING_TYPE,  // title
    DBCommand::RecordBindingType::STRING_TYPE   // body
  };

  auto transaction = DBTransaction::New();
  transaction->commands.push_back(std::move(command));

  ads_->get_ads_client()->RunDBTransaction(std::move(transaction),
      std::bind(&CreativeAdNotificationsDatabaseTable::
          OnGetCreativeAdNotifications, this, _1, categories, callback));
}

bool CreativeAdNotificationsDatabaseTable::Migrate(
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

int CreativeAdNotificationsDatabaseTable::BindParameters(
    DBCommand* command,
    const CreativeAdNotificationList& creative_ad_notifications) {
  DCHECK(command);

  int count = 0;

  int index = 0;
  for (const auto& creative_ad_notification : creative_ad_notifications) {
    BindString(command, index++, creative_ad_notification.creative_instance_id);
    BindString(command, index++, creative_ad_notification.creative_set_id);
    BindString(command, index++, creative_ad_notification.campaign_id);

    const std::string start_at_timestamp =
        creative_ad_notification.start_at_timestamp;
    base::Time start_at_time;
    if (base::Time::FromUTCString(start_at_timestamp.c_str(), &start_at_time)) {
      BindInt64(command, index++, start_at_time.ToDoubleT());
    } else {
      BindInt64(command, index++, std::numeric_limits<uint64_t>::min());
    }

    const std::string end_at_timestamp =
        creative_ad_notification.end_at_timestamp;
    base::Time end_at_time;
    if (base::Time::FromUTCString(end_at_timestamp.c_str(), &end_at_time)) {
      BindInt64(command, index++, end_at_time.ToDoubleT());
    } else {
      BindInt64(command, index++, std::numeric_limits<uint64_t>::max());
    }

    BindInt64(command, index++, creative_ad_notification.daily_cap);
    BindString(command, index++, creative_ad_notification.advertiser_id);
    BindInt64(command, index++, creative_ad_notification.priority);
    BindBool(command, index++, creative_ad_notification.conversion);
    BindInt64(command, index++, creative_ad_notification.per_day);
    BindInt64(command, index++, creative_ad_notification.total_max);
    BindString(command, index++, creative_ad_notification.target_url);
    BindString(command, index++, creative_ad_notification.title);
    BindString(command, index++, creative_ad_notification.body);

    count++;
  }

  return count;
}

std::string CreativeAdNotificationsDatabaseTable::CreateInsertOrUpdateQuery(
    DBCommand* command,
    const CreativeAdNotificationList& creative_ad_notifications) {
  const int count = BindParameters(command, creative_ad_notifications);

  return base::StringPrintf(
      "INSERT OR REPLACE INTO %s "
          "(creative_instance_id, "
          "creative_set_id, "
          "campaign_id, "
          "start_at_timestamp, "
          "end_at_timestamp, "
          "daily_cap, "
          "advertiser_id, "
          "priority, "
          "conversion, "
          "per_day, "
          "total_max, "
          "target_url, "
          "title, "
          "body) VALUES %s;",
      kTableName,
      CreateBindingParameterPlaceholders(14, count).c_str());
}

void CreativeAdNotificationsDatabaseTable::OnGetCreativeAdNotifications(
    DBCommandResponsePtr response,
    const classification::CategoryList& categories,
    GetCreativeAdNotificationsCallback callback) {
  if (!response || response->status != DBCommandResponse::Status::RESPONSE_OK) {
    BLOG(0, "Failed to get creative ad notifications");
    callback(Result::FAILED, categories, {});
    return;
  }

  CreativeAdNotificationList creative_ad_notifications;

  for (auto const& record : response->result->get_records()) {
    CreativeAdNotificationInfo info;

    info.creative_instance_id = ColumnString(record.get(), 0);
    info.creative_set_id = ColumnString(record.get(), 1);
    info.campaign_id = ColumnString(record.get(), 2);
    info.start_at_timestamp = ColumnInt64(record.get(), 3);
    info.end_at_timestamp = ColumnInt64(record.get(), 4);
    info.daily_cap = ColumnInt(record.get(), 5);
    info.advertiser_id = ColumnString(record.get(), 6);
    info.priority = ColumnInt(record.get(), 7);
    info.conversion = ColumnBool(record.get(), 8);
    info.per_day = ColumnInt(record.get(), 9);
    info.total_max = ColumnInt(record.get(), 10);
    info.category = ColumnString(record.get(), 11);
    info.geo_targets.push_back(ColumnString(record.get(), 12));
    info.target_url = ColumnString(record.get(), 13);
    info.title = ColumnString(record.get(), 14);
    info.body = ColumnString(record.get(), 15);

    creative_ad_notifications.emplace_back(info);
  }

  callback(Result::SUCCESS, categories, creative_ad_notifications);
}

bool CreativeAdNotificationsDatabaseTable::CreateTableV1(
    DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s "
          "(creative_instance_id TEXT NOT NULL, "
          "creative_set_id TEXT NOT NULL, "
          "campaign_id TEXT NOT NULL, "
          "start_at_timestamp TIMESTAMP NOT NULL, "
          "end_at_timestamp TIMESTAMP NOT NULL, "
          "daily_cap INTEGER DEFAULT 0 NOT NULL, "
          "advertiser_id LONGVARCHAR, "
          "priority INTEGER NOT NULL DEFAULT 0, "
          "conversion INTEGER NOT NULL DEFAULT 0, "
          "per_day INTEGER NOT NULL DEFAULT 0, "
          "total_max INTEGER NOT NULL DEFAULT 0, "
          "target_url TEXT NOT NULL, "
          "title TEXT NOT NULL, "
          "body TEXT NOT NULL, "
          "PRIMARY KEY(creative_instance_id))",
      kTableName);

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  return true;
}

bool CreativeAdNotificationsDatabaseTable::MigrateToV1(
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

  return true;
}

}  // namespace database
}  // namespace ads
