// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper for tests that want to fill in a NetworkServiceConfig

#include "jingle/glue/network_service_config_test_util.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"

namespace jingle_glue {

NetworkServiceConfigTestUtil::NetworkServiceConfigTestUtil(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : url_request_context_getter_(std::move(url_request_context_getter)),
      weak_ptr_factory_(this) {
  net_runner_ = url_request_context_getter_->GetNetworkTaskRunner();
  mojo_runner_ = base::SequencedTaskRunnerHandle::Get();
  if (net_runner_->BelongsToCurrentThread()) {
    CreateNetworkContextOnNetworkRunner(
        mojo::MakeRequest(&network_context_ptr_), nullptr);
  } else {
    base::ScopedAllowBaseSyncPrimitivesForTesting permission;
    base::WaitableEvent wait_for_create;
    net_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NetworkServiceConfigTestUtil::CreateNetworkContextOnNetworkRunner,
            base::Unretained(this), mojo::MakeRequest(&network_context_ptr_),
            &wait_for_create));
    // Block for creation to avoid needing to worry about
    // CreateNetworkContextOnNetworkRunner
    // potentially happening after ~NetworkServiceConfigTestUtil.
    wait_for_create.Wait();
  }
}

NetworkServiceConfigTestUtil::~NetworkServiceConfigTestUtil() {
  if (!net_runner_->BelongsToCurrentThread()) {
    base::ScopedAllowBaseSyncPrimitivesForTesting permission;
    base::WaitableEvent wait_for_delete;
    net_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NetworkServiceConfigTestUtil::DeleteNetworkContextOnNetworkRunner,
            base::Unretained(this), &wait_for_delete));
    wait_for_delete.Wait();
  }
}

void NetworkServiceConfigTestUtil::FillInNetworkConfig(
    NetworkServiceConfig* config) {
  config->task_runner = net_runner_;
  config->get_proxy_resolving_socket_factory_callback =
      MakeSocketFactoryCallback();
}

GetProxyResolvingSocketFactoryCallback
NetworkServiceConfigTestUtil::MakeSocketFactoryCallback() {
  DCHECK(mojo_runner_->RunsTasksInCurrentSequence());
  return base::BindRepeating(&NetworkServiceConfigTestUtil::RequestSocket,
                             weak_ptr_factory_.GetWeakPtr(), mojo_runner_,
                             net_runner_);
}

void NetworkServiceConfigTestUtil::RequestSocket(
    base::WeakPtr<NetworkServiceConfigTestUtil> instance,
    scoped_refptr<base::SequencedTaskRunner> mojo_runner,
    scoped_refptr<base::SequencedTaskRunner> net_runner,
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  DCHECK(net_runner->RunsTasksInCurrentSequence());
  mojo_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkServiceConfigTestUtil::RequestSocketOnMojoRunner,
                     std::move(instance), std::move(request)));
}

void NetworkServiceConfigTestUtil::RequestSocketOnMojoRunner(
    base::WeakPtr<NetworkServiceConfigTestUtil> instance,
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  if (instance) {
    instance->network_context_ptr_->CreateProxyResolvingSocketFactory(
        std::move(request));
  }
}

void NetworkServiceConfigTestUtil::CreateNetworkContextOnNetworkRunner(
    network::mojom::NetworkContextRequest network_context_request,
    base::WaitableEvent* notify) {
  DCHECK(net_runner_->RunsTasksInCurrentSequence());
  network_context_ = std::make_unique<network::NetworkContext>(
      nullptr, std::move(network_context_request),
      url_request_context_getter_->GetURLRequestContext());
  if (notify)
    notify->Signal();
}

void NetworkServiceConfigTestUtil::DeleteNetworkContextOnNetworkRunner(
    base::WaitableEvent* notify) {
  DCHECK(net_runner_->RunsTasksInCurrentSequence());
  network_context_ = nullptr;
  notify->Signal();
}

}  // namespace jingle_glue
