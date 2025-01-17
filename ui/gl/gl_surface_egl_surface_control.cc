// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_surface_control.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace gl {
namespace {

constexpr char kRootSurfaceName[] = "ChromeNativeWindowSurface";
constexpr char kChildSurfaceName[] = "ChromeChildSurface";

gfx::Size GetBufferSize(const AHardwareBuffer* buffer) {
  AHardwareBuffer_Desc desc;
  base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer, &desc);
  return gfx::Size(desc.width, desc.height);
}

}  // namespace

GLSurfaceEGLSurfaceControl::GLSurfaceEGLSurfaceControl(
    ANativeWindow* window,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : root_surface_(new SurfaceControl::Surface(window, kRootSurfaceName)),
      gpu_task_runner_(std::move(task_runner)),
      weak_factory_(this) {}

GLSurfaceEGLSurfaceControl::~GLSurfaceEGLSurfaceControl() = default;

int GLSurfaceEGLSurfaceControl::GetBufferCount() const {
  // Triple buffering to match framework's BufferQueue.
  return 3;
}

bool GLSurfaceEGLSurfaceControl::Initialize(GLSurfaceFormat format) {
  format_ = format;
  return true;
}

void GLSurfaceEGLSurfaceControl::Destroy() {
  pending_transaction_.reset();
  surface_list_.clear();
  root_surface_.reset();
}

bool GLSurfaceEGLSurfaceControl::Resize(const gfx::Size& size,
                                        float scale_factor,
                                        ColorSpace color_space,
                                        bool has_alpha) {
  // Resizing requires resizing the SurfaceView in the browser.
  return true;
}

bool GLSurfaceEGLSurfaceControl::IsOffscreen() {
  return false;
}

gfx::SwapResult GLSurfaceEGLSurfaceControl::SwapBuffers(
    const PresentationCallback& callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurfaceEGLSurfaceControl::SwapBuffersAsync(
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& presentation_callback) {
  CommitPendingTransaction(completion_callback, presentation_callback);
}

gfx::SwapResult GLSurfaceEGLSurfaceControl::CommitOverlayPlanes(
    const PresentationCallback& callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurfaceEGLSurfaceControl::CommitOverlayPlanesAsync(
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& presentation_callback) {
  CommitPendingTransaction(completion_callback, presentation_callback);
}

void GLSurfaceEGLSurfaceControl::CommitPendingTransaction(
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& present_callback) {
  DCHECK(pending_transaction_);

  // Release resources for the current frame once the next frame is acked.
  ResourceRefs resources_to_release;
  resources_to_release.swap(current_frame_resources_);
  current_frame_resources_.clear();

  // Track resources to be owned by the framework after this transaction.
  current_frame_resources_.swap(pending_frame_resources_);
  pending_frame_resources_.clear();

  SurfaceControl::Transaction::OnCompleteCb callback =
      base::BindOnce(&GLSurfaceEGLSurfaceControl::OnTransactionAckOnGpuThread,
                     weak_factory_.GetWeakPtr(), completion_callback,
                     present_callback, std::move(resources_to_release));
  pending_transaction_->SetOnCompleteCb(std::move(callback), gpu_task_runner_);

  pending_transaction_->Apply();
  pending_transaction_.reset();

  DCHECK_GE(surface_list_.size(), pending_surfaces_count_);
  surface_list_.resize(pending_surfaces_count_);
  pending_surfaces_count_ = 0u;
}

gfx::Size GLSurfaceEGLSurfaceControl::GetSize() {
  return gfx::Size(0, 0);
}

bool GLSurfaceEGLSurfaceControl::OnMakeCurrent(GLContext* context) {
  context_ = context;
  return true;
}

bool GLSurfaceEGLSurfaceControl::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  if (!pending_transaction_)
    pending_transaction_.emplace();

  bool uninitialized = false;
  if (pending_surfaces_count_ == surface_list_.size()) {
    uninitialized = true;
    surface_list_.emplace_back(*root_surface_);
  }
  pending_surfaces_count_++;
  auto& surface_state = surface_list_.at(pending_surfaces_count_ - 1);

  if (uninitialized || surface_state.z_order != z_order) {
    surface_state.z_order = z_order;
    pending_transaction_->SetZOrder(*surface_state.surface, z_order);
  }

  AHardwareBuffer* hardware_buffer = nullptr;
  base::ScopedFD fence_fd;
  auto scoped_hardware_buffer = image->GetAHardwareBuffer();
  if (scoped_hardware_buffer) {
    hardware_buffer = scoped_hardware_buffer->buffer();
    fence_fd = scoped_hardware_buffer->TakeFence();

    auto* a_surface = surface_state.surface->surface();
    DCHECK_EQ(pending_frame_resources_.count(a_surface), 0u);

    auto& resource_ref = pending_frame_resources_[a_surface];
    resource_ref.surface = surface_state.surface;
    resource_ref.scoped_buffer = std::move(scoped_hardware_buffer);
  }

  if (uninitialized || surface_state.hardware_buffer != hardware_buffer) {
    surface_state.hardware_buffer = hardware_buffer;

    if (!fence_fd.is_valid() && gpu_fence && surface_state.hardware_buffer) {
      auto fence_handle =
          gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle());
      DCHECK(!fence_handle.is_null());
      fence_fd = base::ScopedFD(fence_handle.native_fd.fd);
    }

    pending_transaction_->SetBuffer(*surface_state.surface,
                                    surface_state.hardware_buffer,
                                    std::move(fence_fd));
  }

  if (hardware_buffer) {
    gfx::Rect dst = bounds_rect;

    gfx::Size buffer_size = GetBufferSize(hardware_buffer);
    gfx::RectF scaled_rect =
        gfx::RectF(crop_rect.x() * buffer_size.width(),
                   crop_rect.y() * buffer_size.height(),
                   crop_rect.width() * buffer_size.width(),
                   crop_rect.height() * buffer_size.height());
    gfx::Rect src = gfx::ToEnclosedRect(scaled_rect);

    if (uninitialized || surface_state.src != src || surface_state.dst != dst ||
        surface_state.transform != transform) {
      surface_state.src = src;
      surface_state.dst = dst;
      surface_state.transform = transform;
      pending_transaction_->SetGeometry(*surface_state.surface, src, dst,
                                        transform);
    }
  }

  bool opaque = !enable_blend;
  if (uninitialized || surface_state.opaque != opaque) {
    surface_state.opaque = opaque;
    pending_transaction_->SetOpaque(*surface_state.surface, opaque);
  }

  return true;
}

bool GLSurfaceEGLSurfaceControl::IsSurfaceless() const {
  return true;
}

void* GLSurfaceEGLSurfaceControl::GetHandle() {
  return nullptr;
}

bool GLSurfaceEGLSurfaceControl::SupportsAsyncSwap() {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsPlaneGpuFences() const {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsPresentationCallback() {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsSwapBuffersWithBounds() {
  // TODO(khushalsagar): Add support for partial swap.
  return false;
}

bool GLSurfaceEGLSurfaceControl::SupportsCommitOverlayPlanes() {
  return true;
}

void GLSurfaceEGLSurfaceControl::OnTransactionAckOnGpuThread(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    ResourceRefs released_resources,
    SurfaceControl::TransactionStats transaction_stats) {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  context_->MakeCurrent(this);

  // The presentation feedback callback must run after swap completion.
  completion_callback.Run(gfx::SwapResult::SWAP_ACK, nullptr);

  // TODO(khushalsagar): Maintain a queue of present fences so we poll to see if
  // they are signaled every frame, and get a signal timestamp to feed into this
  // feedback.
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::TimeDelta(),
                                     0 /* flags */);
  presentation_callback.Run(feedback);

  for (auto& surface_stat : transaction_stats.surface_stats) {
    auto it = released_resources.find(surface_stat.surface);
    DCHECK(it != released_resources.end());
    if (surface_stat.fence.is_valid())
      it->second.scoped_buffer->SetReadFence(std::move(surface_stat.fence));
  }
  released_resources.clear();
}

GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState(
    const SurfaceControl::Surface& parent)
    : surface(new SurfaceControl::Surface(parent, kChildSurfaceName)) {}

GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState() = default;
GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState(SurfaceState&& other) =
    default;
GLSurfaceEGLSurfaceControl::SurfaceState&
GLSurfaceEGLSurfaceControl::SurfaceState::operator=(SurfaceState&& other) =
    default;

GLSurfaceEGLSurfaceControl::SurfaceState::~SurfaceState() = default;

GLSurfaceEGLSurfaceControl::ResourceRef::ResourceRef() = default;
GLSurfaceEGLSurfaceControl::ResourceRef::~ResourceRef() = default;
GLSurfaceEGLSurfaceControl::ResourceRef::ResourceRef(ResourceRef&& other) =
    default;
GLSurfaceEGLSurfaceControl::ResourceRef&
GLSurfaceEGLSurfaceControl::ResourceRef::operator=(ResourceRef&& other) =
    default;

}  // namespace gl
