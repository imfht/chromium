// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/fcm_connection_establisher.h"

#include <utility>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace chromeos {

namespace android_sms {

class FcmConnectionEstablisherTest : public testing::Test {
 protected:
  FcmConnectionEstablisherTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~FcmConnectionEstablisherTest() override = default;

  void VerifyTransferrableMessage(
      const char* expected,
      const content::FakeServiceWorkerContext::
          StartServiceWorkerAndDispatchMessageArgs& call_args) {
    base::string16 message_string;
    blink::DecodeStringMessage(
        std::get<blink::TransferableMessage>(call_args).owned_encoded_message,
        &message_string);
    EXPECT_EQ(base::UTF8ToUTF16(expected), message_string);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(FcmConnectionEstablisherTest);
};

TEST_F(FcmConnectionEstablisherTest, TestEstablishConnection) {
  auto mock_retry_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* mock_retry_timer_ptr = mock_retry_timer.get();

  content::FakeServiceWorkerContext fake_service_worker_context;
  FcmConnectionEstablisher fcm_connection_establisher(
      std::move(mock_retry_timer));
  auto& message_dispatch_calls =
      fake_service_worker_context
          .start_service_worker_and_dispatch_message_calls();

  // Verify that message is dispatch to service worker.
  fcm_connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), std::get<GURL>(message_dispatch_calls[0]));
  VerifyTransferrableMessage(FcmConnectionEstablisher::kStartFcmMessage,
                             message_dispatch_calls[0]);

  // Return success to result callback and verify that no retries are attempted
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[0]))
      .Run(true /* status */);
  ASSERT_EQ(1u, message_dispatch_calls.size());
  EXPECT_FALSE(mock_retry_timer_ptr->IsRunning());

  // Verify that when multiple requests are sent only the first one is
  // dispatched while the others are queued.
  fcm_connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  fcm_connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kResumeExistingConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, message_dispatch_calls.size());
  VerifyTransferrableMessage(FcmConnectionEstablisher::kStartFcmMessage,
                             message_dispatch_calls[1]);

  // Verify that if the first request fails then it's retried
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[1]))
      .Run(false /* status */);
  ASSERT_EQ(2u, message_dispatch_calls.size());
  EXPECT_TRUE(mock_retry_timer_ptr->IsRunning());
  mock_retry_timer_ptr->Fire();
  ASSERT_EQ(3u, message_dispatch_calls.size());
  VerifyTransferrableMessage(FcmConnectionEstablisher::kStartFcmMessage,
                             message_dispatch_calls[2]);

  // Verify that if the first request succeeds then the next message is
  // dispatched
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[2]))
      .Run(true /* status */);
  ASSERT_EQ(4u, message_dispatch_calls.size());
  EXPECT_FALSE(mock_retry_timer_ptr->IsRunning());
  VerifyTransferrableMessage(FcmConnectionEstablisher::kResumeFcmMessage,
                             message_dispatch_calls[3]);

  // Complete second request and verify that no more retries are scheduled.
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[3]))
      .Run(true /* status */);
  EXPECT_FALSE(mock_retry_timer_ptr->IsRunning());

  // Verify that max retries are attempted before abandoning request
  fcm_connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();

  int retry_count = 0;
  while (true) {
    ASSERT_EQ(5u + retry_count, message_dispatch_calls.size());
    std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                  message_dispatch_calls[4 + retry_count]))
        .Run(false /* status */);
    if (mock_retry_timer_ptr->IsRunning()) {
      mock_retry_timer_ptr->Fire();
      retry_count++;
    } else {
      break;
    }
  }

  EXPECT_EQ(FcmConnectionEstablisher::kMaxRetryCount, retry_count);
}

TEST_F(FcmConnectionEstablisherTest, TestTearDownConnection) {
  content::FakeServiceWorkerContext fake_service_worker_context;
  FcmConnectionEstablisher fcm_connection_establisher(
      std::make_unique<base::MockOneShotTimer>());
  auto& message_dispatch_calls =
      fake_service_worker_context
          .start_service_worker_and_dispatch_message_calls();

  // Verify that message is dispatch to service worker.
  fcm_connection_establisher.TearDownConnection(GetAndroidMessagesURL(),
                                                &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), std::get<GURL>(message_dispatch_calls[0]));
  VerifyTransferrableMessage(FcmConnectionEstablisher::kStopFcmMessage,
                             message_dispatch_calls[0]);
}

}  // namespace android_sms

}  // namespace chromeos
