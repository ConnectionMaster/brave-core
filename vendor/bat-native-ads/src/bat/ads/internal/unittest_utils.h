/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_UNITTEST_UTILS_H_
#define BAT_ADS_INTERNAL_UNITTEST_UTILS_H_

#include <stdint.h>

#include <deque>
#include <utility>

#include "base/files/file_path.h"
#include "brave/components/l10n/browser/locale_helper_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "bat/ads/ads_database.h"
#include "bat/ads/result.h"

namespace ads {

class AdsClientMock;
class AdsImpl;

template<class T>
void Initialize(
    const T& object) {
  object->Initialize(
      [](const Result result) {
    ASSERT_EQ(Result::SUCCESS, result);
  });
}

base::FilePath GetTestPath();

base::FilePath GetResourcesPath();

void MockLoad(
    const std::unique_ptr<AdsClientMock>& mock);

void MockSave(
    const std::unique_ptr<AdsClientMock>& mock);

void MockLoadUserModelForLanguage(
    const std::unique_ptr<AdsClientMock>& mock);

void MockLoadJsonSchema(
    const std::unique_ptr<AdsClientMock>& mock);

void MockRunDBTransaction(
    const std::unique_ptr<AdsClientMock>& mock,
    const std::unique_ptr<AdsDatabase>& database);

uint64_t DistantPast();

uint64_t DistantFuture();

// Checks that |deq1| and |deq2| contain the same number of elements and each
// element in |deq1| is present in |deq2| and vice-versa (Uses the == operator
// for comparing). Returns true if it is the case. Note that this method will
// return true for (aab, abb)
template <class T>
bool CompareDequeAsSets(
    const std::deque<T>& deq1,
    const std::deque<T>& deq2) {
  if (deq1.size() != deq2.size()) {
    return false;
  }

  for (size_t i = 0; i < deq2.size(); i++) {
    bool found = false;

    for (size_t j = 0; (j < deq2.size()) && !found; j++) {
      found = found || (deq1[i] == deq2[j]);
    }

    if (!found) {
      return false;
    }
  }

  return true;
}

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_UNITTEST_UTILS_H_
