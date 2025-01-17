// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_pairing_state_tracker_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace {
const char kMessagesPairStateCookieName[] = "pair_state_cookie";
const char kPairedCookieValue[] = "true";
}  // namespace

namespace chromeos {

namespace android_sms {

AndroidSmsPairingStateTrackerImpl::AndroidSmsPairingStateTrackerImpl(
    Profile* profile,
    AndroidSmsAppManager* android_sms_app_manager)
    : profile_(profile),
      android_sms_app_manager_(android_sms_app_manager),
      cookie_listener_binding_(this),
      weak_ptr_factory_(this) {
  android_sms_app_manager_->AddObserver(this);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AndroidSmsPairingStateTrackerImpl::AddCookieChangeListener,
          weak_ptr_factory_.GetWeakPtr()));
}

AndroidSmsPairingStateTrackerImpl::~AndroidSmsPairingStateTrackerImpl() {
  android_sms_app_manager_->RemoveObserver(this);
}

bool AndroidSmsPairingStateTrackerImpl::IsAndroidSmsPairingComplete() {
  return was_paired_on_last_update_;
}

void AndroidSmsPairingStateTrackerImpl::AttemptFetchMessagesPairingState() {
  GetCookieManager()->GetCookieList(
      GetPairingUrl(), net::CookieOptions(),
      base::BindOnce(&AndroidSmsPairingStateTrackerImpl::OnCookiesRetrieved,
                     base::Unretained(this)));
}

void AndroidSmsPairingStateTrackerImpl::OnCookiesRetrieved(
    const std::vector<net::CanonicalCookie>& cookies) {
  bool was_previously_paired = was_paired_on_last_update_;
  for (const auto& cookie : cookies) {
    if (cookie.Name() == kMessagesPairStateCookieName) {
      PA_LOG(VERBOSE) << "Cookie says Messages paired: " << cookie.Value();
      was_paired_on_last_update_ = cookie.Value() == kPairedCookieValue;
      if (was_previously_paired != was_paired_on_last_update_)
        NotifyPairingStateChanged();
      return;
    }
  }

  was_paired_on_last_update_ = false;
  if (was_previously_paired != was_paired_on_last_update_)
    NotifyPairingStateChanged();
}

void AndroidSmsPairingStateTrackerImpl::OnCookieChange(
    const net::CanonicalCookie& cookie,
    network::mojom::CookieChangeCause cause) {
  DCHECK_EQ(kMessagesPairStateCookieName, cookie.Name());
  DCHECK(cookie.IsDomainMatch(GetPairingUrl().host()));

  // NOTE: cookie.Value() cannot be trusted in this callback. The cookie may
  // have expired or been removed and the Value() does not get updated. It's
  // cleanest to just re-fetch it.
  AttemptFetchMessagesPairingState();
}

void AndroidSmsPairingStateTrackerImpl::OnInstalledAppUrlChanged() {
  // If the app URL changed, stop any ongoing cookie monitoring and attempt to
  // add a new change listener.
  cookie_listener_binding_.Close();
  AddCookieChangeListener();
}

GURL AndroidSmsPairingStateTrackerImpl::GetPairingUrl() {
  base::Optional<GURL> app_url = android_sms_app_manager_->GetInstalledAppUrl();
  if (app_url)
    return *app_url;

  // If no app is installed, default to the normal messages URL.
  return GetAndroidMessagesURL();
}

network::mojom::CookieManager*
AndroidSmsPairingStateTrackerImpl::GetCookieManager() {
  return content::BrowserContext::GetStoragePartitionForSite(profile_,
                                                             GetPairingUrl())
      ->GetCookieManagerForBrowserProcess();
}

void AndroidSmsPairingStateTrackerImpl::AddCookieChangeListener() {
  // Trigger the first fetch of the sms cookie and start listening for changes.
  AttemptFetchMessagesPairingState();
  network::mojom::CookieChangeListenerPtr listener_ptr;
  cookie_listener_binding_.Bind(mojo::MakeRequest(&listener_ptr));

  GetCookieManager()->AddCookieChangeListener(
      GetPairingUrl(), kMessagesPairStateCookieName, std::move(listener_ptr));
}

}  // namespace android_sms

}  // namespace chromeos
