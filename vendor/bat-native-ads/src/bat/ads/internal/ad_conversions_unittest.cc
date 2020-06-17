/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/ad_conversions.h"

#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "brave/components/l10n/browser/locale_helper_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "bat/ads/internal/ads_client_mock.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/database/tables/ad_conversions_database_table.h"
#include "bat/ads/internal/unittest_utils.h"

// npm run test -- brave_unit_tests --filter=BatAds*

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace ads {

class BatAdsAdConversionsTest : public ::testing::Test {
 protected:
  BatAdsAdConversionsTest()
      : ads_client_mock_(std::make_unique<NiceMock<AdsClientMock>>()),
        ads_(std::make_unique<AdsImpl>(ads_client_mock_.get())),
        locale_helper_mock_(std::make_unique<NiceMock<
            brave_l10n::LocaleHelperMock>>()),
        database_table_(std::make_unique<
            database::AdConversionsDatabaseTable>(ads_.get())) {
    // You can do set-up work for each test here

    brave_l10n::LocaleHelper::GetInstance()->set_for_testing(
        locale_helper_mock_.get());
  }

  ~BatAdsAdConversionsTest() override {
    // You can do clean-up work that doesn't throw exceptions here
  }

  // If the constructor and destructor are not enough for setting up and
  // cleaning up each test, you can use the following methods

  void SetUp() override {
    // Code here will be called immediately after the constructor (right before
    // each test)

    ON_CALL(*ads_client_mock_, IsEnabled())
        .WillByDefault(Return(true));

    ON_CALL(*ads_client_mock_, ShouldAllowAdConversionTracking())
        .WillByDefault(Return(true));

    ON_CALL(*locale_helper_mock_, GetLocale())
        .WillByDefault(Return("en-US"));

    MockLoad(ads_client_mock_);
    MockLoadUserModelForLanguage(ads_client_mock_);
    MockLoadJsonSchema(ads_client_mock_);
    MockSave(ads_client_mock_);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    database_ = std::make_unique<AdsDatabase>(
        temp_dir_.GetPath().AppendASCII("database"));

    MockRunDBTransaction(ads_client_mock_, database_);

    Initialize(ads_);
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right before the
    // destructor)
  }

  // Objects declared here can be used by all tests in the test case

  std::deque<uint64_t> GetAdConversionHistoryForCreativeSet(
      const std::string& creative_set_id) {
    std::deque<uint64_t> creative_set_history;

    const std::map<std::string, std::deque<uint64_t>> history =
        ads_->get_client()->GetAdConversionHistory();

    const auto iter = history.find(creative_set_id);
    if (iter == history.end()) {
      return creative_set_history;
    }

    creative_set_history = iter->second;
    return creative_set_history;
  }

  void SaveAdConversions(
      const AdConversionList& ad_conversions) {
    database_table_->Save(ad_conversions, [](const Result result) {
        ASSERT_EQ(Result::SUCCESS, result);
    });
  }

  void TriggerAdEvent(
      const std::string& creative_set_id,
      const ConfirmationType confirmation_type,
      const base::Time time = base::Time::Now()) {
    AdHistory history;

    history.ad_content.creative_instance_id =
        "7a3b6d9f-d0b7-4da6-8988-8d5b8938c94f";
    history.ad_content.creative_set_id = creative_set_id;
    history.ad_content.ad_action = confirmation_type;
    history.timestamp_in_seconds = time.ToDoubleT();

    ads_->get_client()->AppendAdHistoryToAdsHistory(history);
  }

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;

  std::unique_ptr<AdsClientMock> ads_client_mock_;
  std::unique_ptr<AdsImpl> ads_;
  std::unique_ptr<brave_l10n::LocaleHelperMock> locale_helper_mock_;
  std::unique_ptr<database::AdConversionsDatabaseTable> database_table_;
  std::unique_ptr<AdsDatabase> database_;
};

TEST_F(BatAdsAdConversionsTest,
    ShouldNotAllowAdConversionTracking) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  ON_CALL(*ads_client_mock_, ShouldAllowAdConversionTracking())
      .WillByDefault(Return(false));

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/signup");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_TRUE(creative_set_history.empty());
}

TEST_F(BatAdsAdConversionsTest,
    ConvertViewedAd) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  unsigned int foo = 1;
  ad_conversions.emplace_back(creative_set_id, creative_set_id,
      creative_set_id, foo, DistantFuture());
  ad_conversions.emplace_back(creative_set_id, "postview",
      "https://www.brave.com/*", 3, DistantFuture());
  SaveAdConversions(ad_conversions);

  TriggerAdEvent(creative_set_id, ConfirmationType::kViewed);

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/signup");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_EQ(1UL, creative_set_history.size());
}

TEST_F(BatAdsAdConversionsTest,
    ConvertClickedAd) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  ad_conversions.emplace_back(creative_set_id, "postclick",
      "https://www.brave.com/*", 3, DistantFuture());
  SaveAdConversions(ad_conversions);

  TriggerAdEvent(creative_set_id, ConfirmationType::kClicked);

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/signup");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_EQ(1UL, creative_set_history.size());
}

TEST_F(BatAdsAdConversionsTest,
    DoNotConvertNonViewedOrClickedAds) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  ad_conversions.emplace_back(creative_set_id, "postclick",
      "https://www.brave.com/*", 3, DistantFuture());
  SaveAdConversions(ad_conversions);

  TriggerAdEvent(creative_set_id, ConfirmationType::kNone);
  TriggerAdEvent(creative_set_id, ConfirmationType::kDismissed);
  TriggerAdEvent(creative_set_id, ConfirmationType::kLanded);
  TriggerAdEvent(creative_set_id, ConfirmationType::kFlagged);
  TriggerAdEvent(creative_set_id, ConfirmationType::kUpvoted);
  TriggerAdEvent(creative_set_id, ConfirmationType::kDownvoted);
  TriggerAdEvent(creative_set_id, ConfirmationType::kConversion);

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/signup");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_TRUE(creative_set_history.empty());
}

TEST_F(BatAdsAdConversionsTest,
    DoNotConvertAdWhenThereIsAdConversionHistoryForTheSameCreativeSet) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  ad_conversions.emplace_back(creative_set_id, "postview",
      "https://www.brave.com/*", 3, DistantFuture());
  SaveAdConversions(ad_conversions);

  TriggerAdEvent(creative_set_id, ConfirmationType::kViewed);

  ads_->get_ad_conversions()->Check("https://www.brave.com/signup");

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/signup");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_EQ(1UL, creative_set_history.size());
}

TEST_F(BatAdsAdConversionsTest,
    DoNotConvertAdWhenUrlDoesNotMatchAdConversionPattern) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  ad_conversions.emplace_back(creative_set_id, "postview",
      "https://www.brave.com/signup/*", 3, DistantFuture());
  SaveAdConversions(ad_conversions);

  TriggerAdEvent(creative_set_id, ConfirmationType::kViewed);

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/welcome");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_TRUE(creative_set_history.empty());
}

TEST_F(BatAdsAdConversionsTest,
    ConvertAdWhenTheAdConversionObservationWindowHasNotExpired) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  ad_conversions.emplace_back(creative_set_id, "postview",
      "https://www.brave.com/*", 3, DistantFuture());
  SaveAdConversions(ad_conversions);

  const base::Time time = base::Time::Now() - base::TimeDelta::FromHours(71);
  TriggerAdEvent(creative_set_id, ConfirmationType::kViewed, time);

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_EQ(1UL, creative_set_history.size());
}

TEST_F(BatAdsAdConversionsTest,
    DoNotConvertAdWhenTheAdConversionObservationWindowHasExpired) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  ad_conversions.emplace_back(creative_set_id, "postview",
      "https://www.brave.com/*", 3, DistantFuture());
  SaveAdConversions(ad_conversions);

  const base::Time time = base::Time::Now() - base::TimeDelta::FromHours(73);
  TriggerAdEvent(creative_set_id, ConfirmationType::kViewed, time);

  // Act
  ads_->get_ad_conversions()->Check("https://www.foobar.com/");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_TRUE(creative_set_history.empty());
}

TEST_F(BatAdsAdConversionsTest,
    ConvertAdWhenTheAdConversionIsOnTheCuspOfExpiring) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  const base::Time time = base::Time::Now() + base::TimeDelta::FromHours(1);
  ad_conversions.emplace_back(creative_set_id, "postview",
      "https://www.brave.com/*", 3, time.ToDoubleT());
  SaveAdConversions(ad_conversions);

  TriggerAdEvent(creative_set_id, ConfirmationType::kViewed);

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_EQ(1UL, creative_set_history.size());
}

TEST_F(BatAdsAdConversionsTest,
    DoNotConvertAdWhenTheAdConversionHasExpired) {
  // Arrange
  const std::string creative_set_id = "3519f52c-46a4-4c48-9c2b-c264c0067f04";

  AdConversionList ad_conversions;
  ad_conversions.emplace_back(creative_set_id, "postview",
      "https://www.brave.com/*", 3, DistantPast());
  SaveAdConversions(ad_conversions);

  TriggerAdEvent(creative_set_id, ConfirmationType::kViewed);

  // Act
  ads_->get_ad_conversions()->Check("https://www.brave.com/");

  // Assert
  const std::deque<uint64_t> creative_set_history =
      GetAdConversionHistoryForCreativeSet(creative_set_id);

  EXPECT_TRUE(creative_set_history.empty());
}

}  // namespace ads
