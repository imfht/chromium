// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_SERVER_OBJECT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_SERVER_OBJECT_H_

#include <memory>

#include "base/macros.h"

#include <wayland-server-core.h>

struct wl_client;
struct wl_resource;

namespace wl {

// Base class for managing the life cycle of server objects.
class ServerObject {
 public:
  explicit ServerObject(wl_resource* resource);
  virtual ~ServerObject();

  wl_resource* resource() const { return resource_; }

  static void OnResourceDestroyed(wl_resource* resource);

 private:
  wl_resource* resource_;

  DISALLOW_COPY_AND_ASSIGN(ServerObject);
};

template <class T>
T* GetUserDataAs(wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

template <class T>
std::unique_ptr<T> TakeUserDataAs(wl_resource* resource) {
  std::unique_ptr<T> user_data(GetUserDataAs<T>(resource));
  // Make sure ServerObject doesn't try to destroy the resource twice.
  ServerObject::OnResourceDestroyed(resource);
  wl_resource_set_user_data(resource, nullptr);
  return user_data;
}

template <class T>
void DestroyUserData(wl_resource* resource) {
  TakeUserDataAs<T>(resource);
}

template <class T>
void SetImplementation(wl_resource* resource,
                       const void* implementation,
                       std::unique_ptr<T> user_data) {
  wl_resource_set_implementation(resource, implementation, user_data.release(),
                                 DestroyUserData<T>);
}

// Does not transfer ownership of the user_data.  Use with caution.  The only
// legitimate purpose is setting more than one implementation to the same user
// data.
template <class T>
void SetImplementationUnretained(wl_resource* resource,
                                 const void* implementation,
                                 T* user_data) {
  wl_resource_set_implementation(resource, implementation, user_data,
                                 &ServerObject::OnResourceDestroyed);
}

bool ResourceHasImplementation(wl_resource* resource,
                               const wl_interface* interface,
                               const void* impl);

void DestroyResource(wl_client* client, wl_resource* resource);

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_SERVER_OBJECT_H_
