/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/database/database_util.h"

#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace ads {
namespace database {

bool DropTable(
    DBTransaction* transaction,
    const std::string& table_name) {
  DCHECK(transaction);
  DCHECK(!table_name.empty());

  const std::string query = base::StringPrintf(
      "PRAGMA foreign_keys = off;"
      "DROP TABLE IF EXISTS %s;"
      "PRAGMA foreign_keys = on;",
      table_name.c_str());

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  return true;
}

bool DeleteTable(
    DBTransaction* transaction,
    const std::string& table_name) {
  DCHECK(transaction);
  DCHECK(!table_name.empty());

  const std::string query = base::StringPrintf(
      "DELETE FROM %s;",
      table_name.c_str());

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  return true;
}

std::string GenerateDBInsertQuery(
    const std::string& from,
    const std::string& to,
    const std::map<std::string, std::string>& columns,
    const std::string& group_by) {
  DCHECK(!columns.empty());

  std::vector<std::string> from_columns;
  std::vector<std::string> to_columns;

  for (const auto& column : columns) {
    from_columns.push_back(column.first);
    to_columns.push_back(column.second);
  }

  const auto comma_separated_from_columns =
      base::JoinString(from_columns, ", ");
  const auto comma_separated_to_columns =
      base::JoinString(to_columns, ", ");

  return base::StringPrintf(
      "INSERT INTO %s (%s) SELECT %s FROM %s %s;", to.c_str(),
          comma_separated_to_columns.c_str(),
              comma_separated_from_columns.c_str(), from.c_str(),
                  group_by.c_str());
}

bool MigrateDBTable(
    DBTransaction* transaction,
    const std::string& from,
    const std::string& to,
    const std::map<std::string, std::string>& columns,
    const bool should_drop,
    const std::string& group_by) {
  DCHECK(transaction);
  DCHECK_NE(from, to);
  DCHECK(!from.empty());
  DCHECK(!to.empty());

  std::string query = "PRAGMA foreign_keys = off;";

  if (!columns.empty()) {
    const auto insert = GenerateDBInsertQuery(from, to, columns, group_by);
    query.append(insert);
  }

  if (should_drop) {
    query.append(base::StringPrintf("DROP TABLE %s;", from.c_str()));
  }

  query.append("PRAGMA foreign_keys = on;");

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  return true;
}

bool MigrateDBTable(
    DBTransaction* transaction,
    const std::string& from,
    const std::string& to,
    const std::vector<std::string>& columns,
    const bool should_drop,
    const std::string& group_by) {
  DCHECK(transaction);
  DCHECK_NE(from, to);
  DCHECK(!from.empty());
  DCHECK(!to.empty());

  std::map<std::string, std::string> new_columns;
  for (const auto& column : columns) {
    new_columns[column] = column;
  }

  return MigrateDBTable(transaction, from, to, new_columns, should_drop,
      group_by);
}

bool RenameDBTable(
    DBTransaction* transaction,
    const std::string& from,
    const std::string& to) {
  DCHECK(transaction);
  DCHECK_NE(from, to);
  DCHECK(!from.empty());
  DCHECK(!to.empty());

  const auto query = base::StringPrintf("ALTER TABLE %s RENAME TO %s;",
      from.c_str(), to.c_str());

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  return true;
}

bool CreateIndex(
    DBTransaction* transaction,
    const std::string& table_name,
    const std::string& key) {
  DCHECK(transaction);
  DCHECK(!table_name.empty());
  DCHECK(!key.empty());

  const std::string query = base::StringPrintf("CREATE INDEX %s_%s_index ON %s "
      "(%s)", table_name.c_str(), key.c_str(), table_name.c_str(), key.c_str());

  auto command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));

  return true;
}

std::string CreateBindingParameterPlaceholder(
    const size_t parameters_count) {
  DCHECK_NE(0UL, parameters_count);

  const std::vector<std::string> placeholders(parameters_count, "?");

  return base::StringPrintf("(%s)",
      base::JoinString(placeholders, ", ").c_str());
}

std::string CreateBindingParameterPlaceholders(
    const size_t parameters_count,
    const size_t values_count) {
  DCHECK_NE(0UL, values_count);

  const std::string value = CreateBindingParameterPlaceholder(parameters_count);
  if (values_count == 1) {
    return value;
  }

  const std::vector<std::string> values(values_count, value);

  return base::JoinString(values, ", ");
}

void BindNull(
    DBCommand* command,
    const int_fast16_t index) {
  DCHECK(command);

  auto binding = DBCommandBinding::New();
  binding->index = index;
  binding->value = DBValue::New();
  binding->value->set_null_value(0);

  command->bindings.push_back(std::move(binding));
}

void BindInt(
    DBCommand* command,
    const int index,
    const int32_t value) {
  DCHECK(command);

  auto binding = DBCommandBinding::New();
  binding->index = index;
  binding->value = DBValue::New();
  binding->value->set_int_value(value);

  command->bindings.push_back(std::move(binding));
}

void BindInt64(
    DBCommand* command,
    const int index,
    const int64_t value) {
  DCHECK(command);

  auto binding = DBCommandBinding::New();
  binding->index = index;
  binding->value = DBValue::New();
  binding->value->set_int64_value(value);

  command->bindings.push_back(std::move(binding));
}

void BindDouble(
    DBCommand* command,
    const int index,
    const double value) {
  DCHECK(command);

  auto binding = DBCommandBinding::New();
  binding->index = index;
  binding->value = DBValue::New();
  binding->value->set_double_value(value);

  command->bindings.push_back(std::move(binding));
}

void BindBool(
    DBCommand* command,
    const int index,
    const bool value) {
  DCHECK(command);

  auto binding = DBCommandBinding::New();
  binding->index = index;
  binding->value = DBValue::New();
  binding->value->set_bool_value(value);

  command->bindings.push_back(std::move(binding));
}

void BindString(
    DBCommand* command,
    const int index,
    const std::string& value) {
  DCHECK(command);

  auto binding = DBCommandBinding::New();
  binding->index = index;
  binding->value = DBValue::New();
  binding->value->set_string_value(value);

  command->bindings.push_back(std::move(binding));
}

void OnResultCallback(
    DBCommandResponsePtr response,
    ResultCallback callback) {
  DCHECK(response);

  if (response->status != DBCommandResponse::Status::RESPONSE_OK) {
    callback(Result::FAILED);
    return;
  }

  callback(Result::SUCCESS);
}

int ColumnInt(
    DBRecord* record,
    const size_t index) {
  DCHECK(record);
  DCHECK_LT(index, record->fields.size());
  DCHECK_EQ(DBValue::Tag::INT_VALUE, record->fields.at(index)->which());

  return record->fields.at(index)->get_int_value();
}

int64_t ColumnInt64(
    DBRecord* record,
    const size_t index) {
  DCHECK(record);
  DCHECK_LT(index, record->fields.size());
  DCHECK_EQ(DBValue::Tag::INT64_VALUE, record->fields.at(index)->which());

  return record->fields.at(index)->get_int64_value();
}

double ColumnDouble(
    DBRecord* record,
    const size_t index) {
  DCHECK(record);
  DCHECK_LT(index, record->fields.size());
  DCHECK_EQ(DBValue::Tag::DOUBLE_VALUE, record->fields.at(index)->which());

  return record->fields.at(index)->get_double_value();
}

bool ColumnBool(
    DBRecord* record,
    const size_t index) {
  DCHECK(record);
  DCHECK_LT(index, record->fields.size());
  DCHECK_EQ(DBValue::Tag::BOOL_VALUE, record->fields.at(index)->which());

  return record->fields.at(index)->get_bool_value();
}

std::string ColumnString(
    DBRecord* record,
    const size_t index) {
  DCHECK(record);
  DCHECK_LT(index, record->fields.size());
  DCHECK_EQ(DBValue::Tag::STRING_VALUE, record->fields.at(index)->which());

  return record->fields.at(index)->get_string_value();
}

}  // namespace database
}  // namespace ads
