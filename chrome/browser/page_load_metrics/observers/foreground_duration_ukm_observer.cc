// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/foreground_duration_ukm_observer.h"

#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

ForegroundDurationUKMObserver::ForegroundDurationUKMObserver() {}

ForegroundDurationUKMObserver::~ForegroundDurationUKMObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  currently_in_foreground_ = started_in_foreground;
  if (currently_in_foreground_) {
    last_time_shown_ = navigation_handle->NavigationStart();
  }
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  source_id_ = source_id;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordUkmIfInForeground();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordUkmIfInForeground();
  return CONTINUE_OBSERVING;
}
page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnShown() {
  if (!currently_in_foreground_) {
    last_time_shown_ = base::TimeTicks::Now();
    currently_in_foreground_ = true;
  }
  return CONTINUE_OBSERVING;
}

void ForegroundDurationUKMObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordUkmIfInForeground();
}

void ForegroundDurationUKMObserver::RecordUkmIfInForeground() {
  if (!currently_in_foreground_)
    return;
  base::TimeDelta foreground_duration =
      base::TimeTicks::Now() - last_time_shown_;
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::PageForegroundSession(source_id_)
      .SetForegroundDuration(foreground_duration.InMilliseconds())
      .Record(ukm_recorder);
  currently_in_foreground_ = false;
}
