// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_search.h"

#include "base/bind.h"
#include "net/base/network_change_notifier.h"

namespace drivefs {

namespace {

constexpr base::TimeDelta kQueryCacheTtl = base::TimeDelta::FromMinutes(5);

bool IsCloudSharedWithMeQuery(const drivefs::mojom::QueryParametersPtr& query) {
  return query->query_source ==
             drivefs::mojom::QueryParameters::QuerySource::kCloudOnly &&
         query->shared_with_me && !query->text_content && !query->title;
}

}  // namespace

DriveFsSearch::DriveFsSearch(mojom::DriveFs* drivefs, const base::Clock* clock)
    : drivefs_(drivefs), clock_(clock), weak_ptr_factory_(this) {}

DriveFsSearch::~DriveFsSearch() = default;

mojom::QueryParameters::QuerySource DriveFsSearch::PerformSearch(
    mojom::QueryParametersPtr query,
    mojom::SearchQuery::GetNextPageCallback callback) {
  // The only cacheable query is 'shared with me'.
  if (IsCloudSharedWithMeQuery(query)) {
    // Check if we should have the response cached.
    auto delta = clock_->Now() - last_shared_with_me_response_;
    if (delta <= kQueryCacheTtl) {
      query->query_source =
          drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
    }
  }

  drivefs::mojom::SearchQueryPtr search;
  drivefs::mojom::QueryParameters::QuerySource source = query->query_source;
  if (net::NetworkChangeNotifier::IsOffline() &&
      source != drivefs::mojom::QueryParameters::QuerySource::kLocalOnly) {
    // No point trying cloud query if we know we are offline.
    source = drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
    OnSearchDriveFs(std::move(search), std::move(query), std::move(callback),
                    drive::FILE_ERROR_NO_CONNECTION, {});
  } else {
    drivefs_->StartSearchQuery(mojo::MakeRequest(&search), query.Clone());
    auto* raw_search = search.get();
    raw_search->GetNextPage(base::BindOnce(
        &DriveFsSearch::OnSearchDriveFs, weak_ptr_factory_.GetWeakPtr(),
        std::move(search), std::move(query), std::move(callback)));
  }
  return source;
}

void DriveFsSearch::OnSearchDriveFs(
    drivefs::mojom::SearchQueryPtr search,
    drivefs::mojom::QueryParametersPtr query,
    mojom::SearchQuery::GetNextPageCallback callback,
    drive::FileError error,
    base::Optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (error == drive::FILE_ERROR_NO_CONNECTION &&
      query->query_source !=
          drivefs::mojom::QueryParameters::QuerySource::kLocalOnly) {
    // Retry with offline query.
    query->query_source =
        drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
    if (query->text_content) {
      // Full-text searches not supported offline.
      std::swap(query->text_content, query->title);
      query->text_content.reset();
    }
    PerformSearch(std::move(query), std::move(callback));
    return;
  }

  if (error == drive::FILE_ERROR_OK && IsCloudSharedWithMeQuery(query)) {
    // Mark that DriveFS should have cached the required info.
    last_shared_with_me_response_ = clock_->Now();
  }

  std::move(callback).Run(error, std::move(items));
}

}  // namespace drivefs
