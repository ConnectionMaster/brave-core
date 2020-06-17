/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_DATABASE_DATABASE_UTIL_H_
#define BAT_ADS_INTERNAL_DATABASE_DATABASE_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "bat/ads/ads.h"

namespace ads {
namespace database {

bool DropTable(
    DBTransaction* transaction,
    const std::string& table_name);

bool DeleteTable(
    DBTransaction* transaction,
    const std::string& table_name);

std::string GenerateDBInsertQuery(
    const std::string& from,
    const std::string& to,
    const std::map<std::string, std::string>& columns,
    const std::string& group_by);

bool MigrateDBTable(
    DBTransaction* transaction,
    const std::string& from,
    const std::string& to,
    const std::map<std::string, std::string>& columns,
    const bool should_drop,
    const std::string& group_by = "");

bool MigrateDBTable(
    DBTransaction* transaction,
    const std::string& from,
    const std::string& to,
    const std::vector<std::string>& columns,
    const bool should_drop,
    const std::string& group_by = "");

bool RenameDBTable(
    DBTransaction* transaction,
    const std::string& from,
    const std::string& to);

bool CreateIndex(
    DBTransaction* transaction,
    const std::string& table_name,
    const std::string& key);

std::string CreateBindingParameterPlaceholder(
    const size_t parameters_count);

template<typename T>
std::vector<std::vector<T>> ChunkVector(
    const std::vector<T>& source,
    const int size) {
  std::vector<std::vector<T>> result;
  result.reserve((source.size() + size - 1) / size);

  auto start = source.begin();
  auto end = source.end();

  while (start != end) {
    auto next = std::distance(start, end) >= size ? start + size : end;
    result.emplace_back(start, next);
    start = next;
  }

  return result;
}

std::string CreateBindingParameterPlaceholders(
    const size_t parameters_count,
    const size_t values_count);

void BindNull(
    DBCommand* command,
    const int index);

void BindInt(
    DBCommand* command,
    const int index,
    const int32_t value);

void BindInt64(
    DBCommand* command,
    const int index,
    const int64_t value);

void BindDouble(
    DBCommand* command,
    const int index,
    const double value);

void BindBool(
    DBCommand* command,
    const int index,
    const bool value);

void BindString(
    DBCommand* command,
    const int index,
    const std::string& value);

void OnResultCallback(
    DBCommandResponsePtr response,
    ResultCallback callback);

int ColumnInt(
    DBRecord* record,
    const size_t index);

int64_t ColumnInt64(
    DBRecord* record,
    const size_t index);

double ColumnDouble(
    DBRecord* record,
    const size_t index);

bool ColumnBool(
    DBRecord* record,
    const size_t index);

std::string ColumnString(
    DBRecord* record,
    const size_t index);

}  // namespace database
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_DATABASE_DATABASE_UTIL_H_
