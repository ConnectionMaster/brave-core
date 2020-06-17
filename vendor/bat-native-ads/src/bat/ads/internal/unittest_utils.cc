/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/unittest_utils.h"

#include <limits>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "bat/ads/internal/ads_client_mock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ads {

base::FilePath GetDataPath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("brave");
  path = path.AppendASCII("vendor");
  path = path.AppendASCII("bat-native-ads");
  path = path.AppendASCII("data");
  return path;
}

base::FilePath GetTestPath() {
  base::FilePath path = GetDataPath();
  path = path.AppendASCII("test");
  return path;
}

base::FilePath GetResourcesPath() {
  base::FilePath path = GetDataPath();
  path = path.AppendASCII("resources");
  return path;
}

std::string GetPathForRequest(
    const std::string& url) {
  return GURL(url).PathForRequest();
}

void MockLoad(
    const std::unique_ptr<AdsClientMock>& mock) {
  ON_CALL(*mock, Load(_, _))
      .WillByDefault(Invoke([](
          const std::string& name,
          LoadCallback callback) {
        base::FilePath path = GetTestPath();
        path = path.AppendASCII(name);

        std::string value;
        if (!base::ReadFileToString(path, &value)) {
          callback(FAILED, value);
          return;
        }

        callback(SUCCESS, value);
      }));
}

void MockSave(
    const std::unique_ptr<AdsClientMock>& mock) {
  ON_CALL(*mock, Save(_, _, _))
      .WillByDefault(Invoke([](
          const std::string& name,
          const std::string& value,
          ResultCallback callback) {
        callback(SUCCESS);
      }));
}

void MockLoadUserModelForLanguage(
    const std::unique_ptr<AdsClientMock>& mock) {
  const std::vector<std::string> user_model_languages = { "en", "de", "fr" };
  ON_CALL(*mock, GetUserModelLanguages())
      .WillByDefault(Return(user_model_languages));

  ON_CALL(*mock, LoadUserModelForLanguage(_, _))
      .WillByDefault(Invoke([](
          const std::string& language,
          LoadCallback callback) {
        base::FilePath path = GetResourcesPath();
        path = path.AppendASCII("user_models");
        path = path.AppendASCII("languages");
        path = path.AppendASCII(language);
        path = path.AppendASCII("user_model.json");

        std::string value;
        if (!base::ReadFileToString(path, &value)) {
          callback(FAILED, value);
          return;
        }

        callback(SUCCESS, value);
      }));
}

void MockLoadJsonSchema(
    const std::unique_ptr<AdsClientMock>& mock) {
  ON_CALL(*mock, LoadJsonSchema(_))
      .WillByDefault(Invoke([](
          const std::string& name) -> std::string {
        base::FilePath path = GetTestPath();
        path = path.AppendASCII(name);

        std::string value;
        base::ReadFileToString(path, &value);

        return value;
      }));
}

void MockRunDBTransaction(
    const std::unique_ptr<AdsClientMock>& mock,
    const std::unique_ptr<AdsDatabase>& database) {
  ON_CALL(*mock, RunDBTransaction(_, _))
      .WillByDefault(Invoke([&database](
          DBTransactionPtr transaction,
          RunDBTransactionCallback callback) {
        auto response = DBCommandResponse::New();

        if (!database) {
          response->status = DBCommandResponse::Status::RESPONSE_ERROR;
        } else {
          database->RunTransaction(std::move(transaction), response.get());
        }

        callback(std::move(response));
      }));
}

uint64_t DistantPast() {
  return std::numeric_limits<int64_t>::min();
}

uint64_t DistantFuture() {
  return std::numeric_limits<int64_t>::max();
}

}  // namespace ads
