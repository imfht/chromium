// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "chrome/browser/offline_pages/android/offline_page_bridge.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/test/url_loader_interceptor.h"
#include "jni/OfflineTestUtil_jni.h"

// Below is the native implementation of OfflineTestUtil.java.

namespace offline_pages {
namespace {
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;
using ::offline_pages::android::OfflinePageBridge;

Profile* GetProfile() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);
  return profile;
}
RequestCoordinator* GetRequestCoordinator() {
  return RequestCoordinatorFactory::GetForBrowserContext(GetProfile());
}
OfflinePageModel* GetOfflinePageModel() {
  return OfflinePageModelFactory::GetForBrowserContext(GetProfile());
}

void OnGetAllRequestsDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      j_callback_obj,
      offline_pages::android::OfflinePageBridge::CreateJavaSavePageRequests(
          env, std::move(all_requests)));
}

void OnGetAllPagesDone(
    const ScopedJavaGlobalRef<jobject>& j_result_obj,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageModel::MultipleOfflinePageItemResult& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  OfflinePageBridge::AddOfflinePageItemsToJavaList(env, j_result_obj, result);
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
}

void OnDeletePageDone(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                      OfflinePageModel::DeletePageResult result) {
  base::android::RunIntCallbackAndroid(j_callback_obj,
                                       static_cast<int>(result));
}

std::string RequestListToString(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  std::stringstream ss;
  ss << "[\n";
  for (std::unique_ptr<SavePageRequest>& request : requests) {
    ss << " " << request->ToString() << "\n";
  }
  ss << "\n]";
  return ss.str();
}

void DumpRequestCoordinatorState(
    base::OnceCallback<void(std::string)> callback) {
  auto convert_and_return =
      [](base::OnceCallback<void(std::string)> callback,
         std::vector<std::unique_ptr<SavePageRequest>> requests) {
        std::move(callback).Run(RequestListToString(std::move(requests)));
      };
  GetRequestCoordinator()->GetAllRequests(
      base::BindOnce(convert_and_return, std::move(callback)));
}

class Interceptor {
 public:
  void InterceptWithOfflineError(const GURL& url, base::OnceClosure callback) {
    interceptors_.push_back(
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            url, net::Error::ERR_INTERNET_DISCONNECTED, std::move(callback)));
  }

 private:
  std::vector<std::unique_ptr<content::URLLoaderInterceptor>> interceptors_;
};

// This is a raw pointer because global destructors are disallowed.
Interceptor* g_interceptor = nullptr;

}  // namespace

void JNI_OfflineTestUtil_GetRequestsInQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  RequestCoordinator* coordinator = GetRequestCoordinator();

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    const JavaParamRef<jobject> empty_result(nullptr);
    base::android::RunObjectCallbackAndroid(j_callback_obj, empty_result);
    return;
  }

  coordinator->GetAllRequests(
      base::BindOnce(&OnGetAllRequestsDone, std::move(j_callback_ref)));
}

void JNI_OfflineTestUtil_GetAllPages(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_result_obj);
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_result_ref(env, j_result_obj);
  ScopedJavaGlobalRef<jobject> j_callback_ref(env, j_callback_obj);
  GetOfflinePageModel()->GetAllPages(base::BindOnce(
      &OnGetAllPagesDone, std::move(j_result_ref), std::move(j_callback_ref)));
}

void JNI_OfflineTestUtil_DeletePagesByOfflineId(
    JNIEnv* env,
    const JavaParamRef<jlongArray>& j_offline_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(env, j_callback_obj);
  std::vector<int64_t> offline_ids;
  base::android::JavaLongArrayToInt64Vector(env, j_offline_ids_array,
                                            &offline_ids);
  GetOfflinePageModel()->DeletePagesByOfflineId(
      offline_ids,
      base::BindOnce(&OnDeletePageDone, std::move(j_callback_ref)));
}

JNI_EXPORT void JNI_OfflineTestUtil_StartRequestCoordinatorProcessing(
    JNIEnv* env) {
  GetRequestCoordinator()->StartImmediateProcessing(base::DoNothing());
}

void JNI_OfflineTestUtil_InterceptWithOfflineError(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jobject>& j_ready_callback) {
  if (!g_interceptor)
    g_interceptor = new Interceptor;
  const std::string url = base::android::ConvertJavaStringToUTF8(env, j_url);
  g_interceptor->InterceptWithOfflineError(
      GURL(url), base::BindOnce(base::android::RunRunnableAndroid,
                                base::android::ScopedJavaGlobalRef<jobject>(
                                    env, j_ready_callback)));
}

void JNI_OfflineTestUtil_ClearIntercepts(JNIEnv* env) {
  delete g_interceptor;
  g_interceptor = nullptr;
}

void JNI_OfflineTestUtil_DumpRequestCoordinatorState(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_callback) {
  auto wrap_callback = base::BindOnce(
      [](base::android::ScopedJavaGlobalRef<jobject> j_callback,
         std::string dump) {
        JNIEnv* env = base::android::AttachCurrentThread();
        base::android::RunObjectCallbackAndroid(
            j_callback, base::android::ConvertUTF8ToJavaString(env, dump));
      },
      base::android::ScopedJavaGlobalRef<jobject>(env, j_callback));
  DumpRequestCoordinatorState(std::move(wrap_callback));
}

}  // namespace offline_pages
