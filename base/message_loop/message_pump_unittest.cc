// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;

namespace base {
namespace {

class MockMessagePumpDelegate : public MessagePump::Delegate {
 public:
  MockMessagePumpDelegate() = default;

  // MessagePump::Delegate:
  MOCK_METHOD0(DoWork, bool());
  MOCK_METHOD1(DoDelayedWork, bool(TimeTicks*));
  MOCK_METHOD0(DoIdleWork, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMessagePumpDelegate);
};

class MessagePumpTest : public ::testing::TestWithParam<MessageLoopBase::Type> {
 public:
  MessagePumpTest()
      : message_pump_(MessageLoop::CreateMessagePumpForType(GetParam())) {}

 protected:
  std::unique_ptr<MessagePump> message_pump_;
};

TEST_P(MessagePumpTest, QuitStopsWork) {
  testing::StrictMock<MockMessagePumpDelegate> delegate;

  // Not expecting any calls to DoDelayedWork or DoIdleWork after quitting.
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return false;
  }));
  EXPECT_CALL(delegate, DoDelayedWork(_)).Times(0);
  EXPECT_CALL(delegate, DoIdleWork()).Times(0);

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, QuitStopsWorkWithNestedRunLoop) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate;
  testing::StrictMock<MockMessagePumpDelegate> nested_delegate;

  // We first schedule a call to DoWork, which runs a nested run loop. After the
  // nested loop exits, we schedule another DoWork which quits the outer
  // (original) run loop. The test verifies that there are no extra calls to
  // DoWork after the outer loop quits.
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([&] {
    message_pump_->ScheduleWork();
    message_pump_->Run(&nested_delegate);
    message_pump_->ScheduleWork();
    return false;
  }));
  EXPECT_CALL(nested_delegate, DoWork).WillOnce(Invoke([&] {
    // Quit the nested run loop.
    message_pump_->Quit();
    return false;
  }));
  EXPECT_CALL(delegate, DoDelayedWork(_)).WillOnce(Return(false));
  // The outer pump may or may not trigger idle work at this point.
  EXPECT_CALL(delegate, DoIdleWork()).Times(AnyNumber());
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([&] {
    // Quit the original run loop.
    message_pump_->Quit();
    return false;
  }));

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

class TimerSlackTestDelegate : public MessagePump::Delegate {
 public:
  TimerSlackTestDelegate(MessagePump* message_pump)
      : message_pump_(message_pump) {
    // We first schedule a delayed task far in the future with maximum timer
    // slack.
    message_pump_->SetTimerSlack(TIMER_SLACK_MAXIMUM);
    message_pump_->ScheduleDelayedWork(TimeTicks::Now() +
                                       TimeDelta::FromHours(1));

    // Since we have no other work pending, the pump will initially be idle.
    action_.store(NONE);
  }

  bool DoWork() override {
    switch (action_.load()) {
      case NONE:
        break;
      case SCHEDULE_DELAYED_WORK:
        // After being woken up by the other thread, we schedule work after a
        // short delay. If is workaround was successful, the pump will wake up
        // shortly, finishing the test.
        action_.store(QUIT);
        message_pump_->ScheduleDelayedWork(TimeTicks::Now() +
                                           TimeDelta::FromMilliseconds(50));
        break;
      case QUIT:
        message_pump_->Quit();
        break;
    }
    return false;
  }
  bool DoDelayedWork(base::TimeTicks*) override { return false; }
  bool DoIdleWork() override { return false; }

  void WakeUpFromOtherThread() {
    action_.store(SCHEDULE_DELAYED_WORK);
    message_pump_->ScheduleWork();
  }

 private:
  enum Action {
    NONE,
    SCHEDULE_DELAYED_WORK,
    QUIT,
  };

  MessagePump* const message_pump_;
  std::atomic<Action> action_;
};

TEST_P(MessagePumpTest, TimerSlackWithLongDelays) {
  // This is a regression test for an issue where the iOS message pump fails to
  // run delayed work when timer slack is enabled. The steps needed to trigger
  // this are:
  //
  //  1. The message pump timer slack is set to maximum.
  //  2. A delayed task is posted for far in the future (e.g., 1h).
  //  3. The system goes idle at least for a few seconds.
  //  4. Another delayed task is posted with a much smaller delay.
  //
  // The following message pump test delegate automatically runs through this
  // sequence.
  TimerSlackTestDelegate delegate(message_pump_.get());

  // We use another thread to wake up the pump after 2 seconds to allow the
  // system to enter an idle state. This delay was determined experimentally on
  // the iPhone 6S simulator.
  Thread thread("Waking thread");
  thread.StartAndWaitForTesting();
  thread.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindLambdaForTesting([&delegate] { delegate.WakeUpFromOtherThread(); }),
      TimeDelta::FromSeconds(2));

  message_pump_->Run(&delegate);
}

INSTANTIATE_TEST_SUITE_P(,
                         MessagePumpTest,
                         ::testing::Values(MessageLoop::TYPE_DEFAULT,
                                           MessageLoop::TYPE_UI,
                                           MessageLoop::TYPE_IO));

}  // namespace
}  // namespace base