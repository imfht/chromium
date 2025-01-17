// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/element_area.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;

namespace autofill_assistant {

namespace {

MATCHER_P4(MatchingRectF,
           left,
           top,
           right,
           bottom,
           base::StringPrintf("MatchingRectF(%2.2f, %2.2f, %2.2f, %2.2f)",
                              left,
                              top,
                              right,
                              bottom)) {
  return abs(left - arg.left) < 0.01 && abs(top - arg.top) < 0.01 &&
         abs(right - arg.right) < 0.01 && abs(bottom - arg.bottom) < 0.01;
}

ACTION(DoNothing) {}

class ElementAreaTest : public testing::Test, public ScriptExecutorDelegate {
 protected:
  ElementAreaTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        element_area_(this) {
    ON_CALL(mock_web_controller_, OnGetElementPosition(_, _))
        .WillByDefault(RunOnceCallback<1>(false, RectF()));
    element_area_.SetOnUpdate(base::BindRepeating(&ElementAreaTest::OnUpdate,
                                                  base::Unretained(this)));
  }

  // Overrides ScriptTrackerDelegate
  Service* GetService() override { return nullptr; }

  UiController* GetUiController() override { return nullptr; }

  WebController* GetWebController() override { return &mock_web_controller_; }

  ClientMemory* GetClientMemory() override { return nullptr; }

  void EnterState(AutofillAssistantState state) override {}

  const std::map<std::string, std::string>& GetParameters() override {
    return parameters_;
  }

  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return nullptr;
  }

  content::WebContents* GetWebContents() override { return nullptr; }

  void SetTouchableElementArea(const ElementAreaProto& element_area) override {}

  void SetStatusMessage(const std::string& status_message) override {}
  std::string GetStatusMessage() const override { return std::string(); }

  void SetDetails(const Details& details) override {}

  void ClearDetails() override {}

  void SetElement(const std::string& selector) {
    ElementAreaProto area;
    area.add_rectangles()->add_elements()->add_selectors(selector);
    element_area_.SetFromProto(area);
  }

  void OnUpdate(bool success, const std::vector<RectF>& area) {
    highlighted_area_ = area;
  }

  // scoped_task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  MockWebController mock_web_controller_;
  std::map<std::string, std::string> parameters_;
  ElementArea element_area_;
  std::vector<RectF> highlighted_area_;
};

TEST_F(ElementAreaTest, Empty) {
  EXPECT_TRUE(element_area_.IsEmpty());
  EXPECT_THAT(highlighted_area_, IsEmpty());
}

TEST_F(ElementAreaTest, ElementNotFound) {
  SetElement("#not_found");
  EXPECT_TRUE(element_area_.IsEmpty());
  EXPECT_THAT(highlighted_area_, IsEmpty());
}

TEST_F(ElementAreaTest, CoverViewport) {
  element_area_.CoverViewport();
  EXPECT_TRUE(element_area_.HasElements());
  EXPECT_FALSE(element_area_.IsEmpty());
  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.0f, 0.0f, 1.0f, 1.0f)));
}

TEST_F(ElementAreaTest, OneRectangle) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#found"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.25f, 0.25f, 0.75f, 0.75f)));

  SetElement("#found");
  EXPECT_FALSE(element_area_.IsEmpty());
  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.25f, 0.25f, 0.75f, 0.75f)));
}

TEST_F(ElementAreaTest, TwoRectangles) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#top_left"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.0f, 0.25f, 0.25f)));
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#bottom_right"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.25f, 0.25f, 1.0f, 1.0f)));

  ElementAreaProto area_proto;
  area_proto.add_rectangles()->add_elements()->add_selectors("#top_left");
  area_proto.add_rectangles()->add_elements()->add_selectors("#bottom_right");
  element_area_.SetFromProto(area_proto);

  EXPECT_FALSE(element_area_.IsEmpty());
  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.0f, 0.0f, 0.25f, 0.25f),
                          MatchingRectF(0.25f, 0.25f, 1.0f, 1.0f)));
}

TEST_F(ElementAreaTest, OneRectangleTwoElements) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element1"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.1f, 0.3f, 0.2f, 0.4f)));
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element2"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.5f, 0.2f, 0.6f, 0.5f)));

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_rectangles();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  element_area_.SetFromProto(area_proto);

  EXPECT_FALSE(element_area_.IsEmpty());
  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.1f, 0.2f, 0.6f, 0.5f)));
}

TEST_F(ElementAreaTest, DoNotReportIncompleteRectangles) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element1"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.1f, 0.3f, 0.2f, 0.4f)));

  // Getting the position of #element2 neither succeeds nor fails, simulating an
  // intermediate state which shouldn't be reported to the callback.
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element2"})), _))
      .WillOnce(DoNothing());  // overrides default action

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_rectangles();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  element_area_.SetFromProto(area_proto);

  EXPECT_TRUE(element_area_.HasElements());
  EXPECT_FALSE(element_area_.IsEmpty());
  EXPECT_THAT(highlighted_area_, IsEmpty());
}

TEST_F(ElementAreaTest, OneRectangleFourElements) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element1"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.0f, 0.1f, 0.1f)));
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element2"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.9f, 0.9f, 1.0f, 1.0f)));
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element3"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.9f, 0.1f, 1.0f)));
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element4"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.9f, 0.0f, 1.0f, 0.1f)));

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_rectangles();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  rectangle_proto->add_elements()->add_selectors("#element3");
  rectangle_proto->add_elements()->add_selectors("#element4");
  element_area_.SetFromProto(area_proto);

  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.0f, 0.0f, 1.0f, 1.0f)));
}

TEST_F(ElementAreaTest, OneRectangleMissingElements) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element1"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.1f, 0.1f, 0.2f, 0.2f)));
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element2"})), _))
      .WillOnce(RunOnceCallback<1>(false, RectF()));

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_rectangles();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  element_area_.SetFromProto(area_proto);

  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.1f, 0.1f, 0.2f, 0.2f)));
}

TEST_F(ElementAreaTest, FullWidthRectangle) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element1"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.1f, 0.3f, 0.2f, 0.4f)));
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element2"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.5f, 0.7f, 0.6f, 0.8f)));

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_rectangles();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  rectangle_proto->set_full_width(true);
  element_area_.SetFromProto(area_proto);

  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.0f, 0.3f, 1.0f, 0.8f)));
}

TEST_F(ElementAreaTest, ElementMovesAfterUpdate) {
  testing::InSequence seq;
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.25f, 1.0f, 0.5f)))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.5f, 1.0f, 0.75f)));

  SetElement("#element");

  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.0f, 0.25f, 1.0f, 0.5f)));

  element_area_.UpdatePositions();

  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.0f, 0.5f, 1.0f, 0.75f)));
}

TEST_F(ElementAreaTest, ElementMovesWithTime) {
  testing::InSequence seq;
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#element"})), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.25f, 1.0f, 0.5f)))
      .WillOnce(RunOnceCallback<1>(true, RectF(0.0f, 0.5f, 1.0f, 0.75f)));

  SetElement("#element");

  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.0f, 0.25f, 1.0f, 0.5f)));

  scoped_task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(100));

  EXPECT_THAT(highlighted_area_,
              ElementsAre(MatchingRectF(0.0f, 0.5f, 1.0f, 0.75f)));
}
}  // namespace
}  // namespace autofill_assistant
