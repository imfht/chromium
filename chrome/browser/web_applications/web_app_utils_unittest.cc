// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // OS_CHROMEOS

namespace web_app {

class WebAppUtilsTest : public WebAppTest {};

TEST_F(WebAppUtilsTest, AreWebAppsEnabled) {
  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile->GetOffTheRecordProfile()));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile));
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile->GetOffTheRecordProfile()));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsEnabled(system_profile));
  EXPECT_FALSE(AreWebAppsEnabled(system_profile->GetOffTheRecordProfile()));

#if defined(OS_CHROMEOS)
  Profile* signin_profile =
      profile_manager.CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsEnabled(signin_profile));
  EXPECT_FALSE(AreWebAppsEnabled(signin_profile->GetOffTheRecordProfile()));

  Profile* lock_screen_profile = profile_manager.CreateTestingProfile(
      chromeos::ProfileHelper::GetLockScreenAppProfileName());
  EXPECT_FALSE(AreWebAppsEnabled(lock_screen_profile));
  EXPECT_FALSE(
      AreWebAppsEnabled(lock_screen_profile->GetOffTheRecordProfile()));
#endif
}

TEST_F(WebAppUtilsTest, AreWebAppsUserInstallable) {
  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsUserInstallable(regular_profile));
  EXPECT_TRUE(
      AreWebAppsUserInstallable(regular_profile->GetOffTheRecordProfile()));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(guest_profile));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(guest_profile->GetOffTheRecordProfile()));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(system_profile));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(system_profile->GetOffTheRecordProfile()));

#if defined(OS_CHROMEOS)
  Profile* signin_profile =
      profile_manager.CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsUserInstallable(signin_profile));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(signin_profile->GetOffTheRecordProfile()));

  Profile* lock_screen_profile = profile_manager.CreateTestingProfile(
      chromeos::ProfileHelper::GetLockScreenAppProfileName());
  EXPECT_FALSE(AreWebAppsUserInstallable(lock_screen_profile));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(lock_screen_profile->GetOffTheRecordProfile()));
#endif
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebApps) {
  Profile* regular_profile = profile();

  EXPECT_EQ(regular_profile, GetBrowserContextForWebApps(regular_profile));
  EXPECT_EQ(regular_profile, GetBrowserContextForWebApps(
                                 regular_profile->GetOffTheRecordProfile()));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_EQ(guest_profile, GetBrowserContextForWebApps(guest_profile));
  EXPECT_EQ(guest_profile, GetBrowserContextForWebApps(
                               guest_profile->GetOffTheRecordProfile()));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(system_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(
                         system_profile->GetOffTheRecordProfile()));
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebAppMetrics) {
  Profile* regular_profile = profile();

  EXPECT_EQ(regular_profile,
            GetBrowserContextForWebAppMetrics(regular_profile));
  EXPECT_EQ(regular_profile, GetBrowserContextForWebAppMetrics(
                                 regular_profile->GetOffTheRecordProfile()));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(guest_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(
                         guest_profile->GetOffTheRecordProfile()));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(system_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(
                         system_profile->GetOffTheRecordProfile()));
}

}  // namespace web_app
