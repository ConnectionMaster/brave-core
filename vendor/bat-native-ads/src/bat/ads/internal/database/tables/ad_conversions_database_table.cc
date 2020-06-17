/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/database/tables/ad_conversions_database_table.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "bat/ads/internal/ad_conversion_info.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/database/database_util.h"
#include "bat/ads/internal/logging.h"

using std::placeholders::_1;

namespace ads {
namespace database {

namespace {
const char kTableName[] = "ad_conversions";
}  // namespace

AdConversionsDatabaseTable::AdConversionsDatabaseTable(
    AdsImpl* ads)
    : ads_(ads) {
  DCHECK(ads_);
}

AdConversionsDatabaseTable::~AdConversionsDatabaseTable() = default;

void AdConversionsDatabaseTable::Save(
    const AdConversionList& ad_conversions,
    ResultCallback callback) {
  auto transaction = DBTransaction::New();

  AppendInsertOrUpdateQuery(transaction.get(), ad_conversions);

  ads_->get_ads_client()->RunDBTransaction(std::move(transaction),
      std::bind(&OnResultCallback, _1, callback));
}

void AdConversionsDatabaseTable::AppendInsertOrUpdateQuery(
    DBTransaction* transaction,
    const AdConversionList& ad_conversions) {
  DCHECK(transaction);

  if (ad_conversions.empty()) {
    return;
  }

  auto command = DBCommand::New();
  command->type = DBCommand::Type::RUN;
  command->command = CreateInsertOrUpdateQuery(command.get(), ad_conversions);

  transaction->commands.push_back(std::move(command));
}

void AdConversionsDatabaseTable::GetAdConversions(
    GetAdConversionsCallback callback) {
  const std::string query = base::StringPrintf(
      "SELECT "
          "ac.creative_set_id, "
          "ac.type, "
          "ac.url_pattern, "
          "ac.observation_window, "
          "ac.expiry_timestamp "
      "FROM %s AS ac "
      "WHERE strftime('%%s','now') < expiry_timestamp",
      kTableName);

  auto command = DBCommand::New();
  command->type = DBCommand::Type::READ;
  command->command = query;

  command->record_bindings = {
    DBCommand::RecordBindingType::STRING_TYPE,  // creative_set_id
    DBCommand::RecordBindingType::STRING_TYPE,  // type
    DBCommand::RecordBindingType::STRING_TYPE,  // url_pattern
    DBCommand::RecordBindingType::INT_TYPE,     // observation_window
    DBCommand::RecordBindingType::INT64_TYPE    // expiry_timestamp
  };

  auto transaction = DBTransaction::New();
  transaction->commands.push_back(std::move(command));

  ads_->get_ads_client()->RunDBTransaction(std::move(transaction),
      std::bind(&AdConversionsDatabaseTable::OnGetAdConversions, this, _1,
          callback));
}

void AdConversionsDatabaseTable::PurgeExpiredAdConversions(
    ResultCallback callback) {
  auto transaction = DBTransaction::New();

  const std::string query = base::StringPrintf(
      "DELETE FROM %s "
      "WHERE strftime('%%s','now') >= expiry_timestamp",
      kTableName);

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  ads_->get_ads_client()->RunDBTransaction(std::move(transaction),
      std::bind(&OnResultCallback, _1, callback));
}

bool AdConversionsDatabaseTable::Migrate(
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

int AdConversionsDatabaseTable::BindParameters(
    DBCommand* command,
    const AdConversionList& ad_conversions) {
  DCHECK(command);

  int count = 0;

  int index = 0;
  for (const auto& ad_conversion : ad_conversions) {
    BindString(command, index++, ad_conversion.creative_set_id);
    BindString(command, index++, ad_conversion.type);
    BindString(command, index++, ad_conversion.url_pattern);
    BindInt(command, index++, ad_conversion.observation_window);
    BindInt64(command, index++, ad_conversion.expiry_timestamp);

    count++;
  }

  return count;
}

std::string AdConversionsDatabaseTable::CreateInsertOrUpdateQuery(
    DBCommand* command,
    const AdConversionList& ad_conversions) {
  const int count = BindParameters(command, ad_conversions);

  return base::StringPrintf(
      "INSERT OR REPLACE INTO %s "
          "(creative_set_id, "
          "type, "
          "url_pattern, "
          "observation_window, "
          "expiry_timestamp) VALUES %s",
    kTableName,
    CreateBindingParameterPlaceholders(5, count).c_str());
}

void AdConversionsDatabaseTable::OnGetAdConversions(
    DBCommandResponsePtr response,
    GetAdConversionsCallback callback) {
  if (!response || response->status != DBCommandResponse::Status::RESPONSE_OK) {
    BLOG(0, "Failed to get creative ad conversions");
    callback(Result::FAILED, {});
    return;
  }

  AdConversionList ad_conversions;

  for (auto const& record : response->result->get_records()) {
    AdConversionInfo info;

    info.creative_set_id = ColumnString(record.get(), 0);
    info.type = ColumnString(record.get(), 1);
    info.url_pattern = ColumnString(record.get(), 2);
    info.observation_window = ColumnInt(record.get(), 3);
    info.expiry_timestamp = ColumnInt64(record.get(), 4);

    ad_conversions.emplace_back(info);
  }

  callback(Result::SUCCESS, ad_conversions);
}

bool AdConversionsDatabaseTable::CreateTableV1(
    DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s "
          "(creative_set_id TEXT NOT NULL, "
          "type TEXT NOT NULL, "
          "url_pattern TEXT NOT NULL, "
          "observation_window INTEGER NOT NULL, "
          "expiry_timestamp TIMESTAMP NOT NULL, "
          "UNIQUE(creative_set_id, type, url_pattern) ON CONFLICT REPLACE, "
          "PRIMARY KEY(creative_set_id, type, url_pattern))",
      kTableName);

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  return true;
}

bool AdConversionsDatabaseTable::MigrateToV1(
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
