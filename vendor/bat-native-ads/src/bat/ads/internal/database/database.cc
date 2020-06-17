/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include <utility>

#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/database/database.h"
#include "bat/ads/internal/database/database_migration.h"
#include "bat/ads/internal/database/database_util.h"
#include "bat/ads/internal/database/database_version.h"
#include "bat/ads/internal/logging.h"

using std::placeholders::_1;
using std::placeholders::_2;

namespace ads {
namespace database {

Database::Database(
    AdsImpl* ads)
    : ads_(ads) {
  DCHECK(ads_);
}

Database::~Database() = default;

void Database::Initialize(
    ResultCallback callback) {
  auto transaction = DBTransaction::New();
  transaction->version = version();
  transaction->compatible_version = compatible_version();

  auto command = DBCommand::New();
  command->type = DBCommand::Type::INITIALIZE;

  transaction->commands.push_back(std::move(command));

  ads_->get_ads_client()->RunDBTransaction(std::move(transaction),
      std::bind(&Database::OnInitialize, this, _1, callback));
}

std::string Database::get_last_message() const {
  return last_message_;
}

//////////////////////////////////////////////////////////////////////////////

void Database::OnInitialize(
    DBCommandResponsePtr response,
    ResultCallback callback) {
  DCHECK(response);

  if (response->status != DBCommandResponse::Status::RESPONSE_OK) {
    last_message_ = "Invalid response status";
    callback(Result::FAILED);
    return;
  }

  if (!response->result ||
      response->result->get_value()->which() != DBValue::Tag::INT_VALUE) {
    last_message_ = "Invalid type for response result";
    callback(Result::FAILED);
    return;
  }

  const auto version = response->result->get_value()->get_int_value();

  DatabaseMigration migration(ads_);
  migration.MigrateFromVersion(version, callback);
}

}  // namespace database
}  // namespace ads
