// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/vr/orientation/orientation_device.h"
#include "device/vr/orientation/orientation_device_provider.h"
#include "device/vr/test/fake_orientation_provider.h"
#include "device/vr/test/fake_sensor_provider.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/quaternion.h"

namespace device {

class VROrientationDeviceProviderTest : public testing::Test {
 protected:
  VROrientationDeviceProviderTest() = default;
  ~VROrientationDeviceProviderTest() override = default;
  void SetUp() override {
    fake_sensor_provider_ = std::make_unique<FakeSensorProvider>();

    fake_sensor_ = std::make_unique<FakeOrientationSensor>(
        mojo::MakeRequest(&sensor_ptr_));
    shared_buffer_handle_ = mojo::SharedBufferHandle::Create(
        sizeof(SensorReadingSharedBuffer) *
        (static_cast<uint64_t>(mojom::SensorType::kMaxValue) + 1));

    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
    connector_->OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName(mojom::kServiceName),
        mojom::SensorProvider::Name_,
        base::BindRepeating(&FakeSensorProvider::Bind,
                            base::Unretained(fake_sensor_provider_.get())));

    provider_ = std::make_unique<VROrientationDeviceProvider>(connector_.get());

    scoped_task_environment_.RunUntilIdle();
  }

  void TearDown() override {}

  void InitializeDevice(mojom::SensorInitParamsPtr params) {
    // Be sure GetSensor goes through so the callback is set.
    scoped_task_environment_.RunUntilIdle();

    fake_sensor_provider_->CallCallback(std::move(params));

    // Allow the callback call to go through.
    scoped_task_environment_.RunUntilIdle();
  }

  mojom::SensorInitParamsPtr FakeInitParams() {
    auto init_params = mojom::SensorInitParams::New();
    init_params->sensor = std::move(sensor_ptr_);
    init_params->default_configuration = PlatformSensorConfiguration(
        SensorTraits<kOrientationSensorType>::kDefaultFrequency);

    init_params->client_request = mojo::MakeRequest(&sensor_client_ptr_);

    init_params->memory = shared_buffer_handle_->Clone(
        mojo::SharedBufferHandle::AccessMode::READ_ONLY);

    init_params->buffer_offset =
        SensorReadingSharedBuffer::GetOffset(kOrientationSensorType);

    return init_params;
  }

  base::RepeatingCallback<void(device::mojom::XRDeviceId,
                               mojom::VRDisplayInfoPtr,
                               mojom::XRRuntimePtr device)>
  DeviceAndIdCallbackFailIfCalled() {
    return base::BindRepeating([](device::mojom::XRDeviceId id,
                                  mojom::VRDisplayInfoPtr,
                                  mojom::XRRuntimePtr device) { FAIL(); });
  };

  base::RepeatingCallback<void(device::mojom::XRDeviceId)>
  DeviceIdCallbackFailIfCalled() {
    return base::BindRepeating([](device::mojom::XRDeviceId id) { FAIL(); });
  };

  base::RepeatingCallback<void(device::mojom::XRDeviceId,
                               mojom::VRDisplayInfoPtr,
                               mojom::XRRuntimePtr device)>
  DeviceAndIdCallbackMustBeCalled(base::RunLoop* loop) {
    return base::BindRepeating(
        [](base::OnceClosure quit_closure, device::mojom::XRDeviceId id,
           mojom::VRDisplayInfoPtr info, mojom::XRRuntimePtr device) {
          ASSERT_TRUE(device);
          ASSERT_TRUE(info);
          std::move(quit_closure).Run();
        },
        loop->QuitClosure());
  };

  base::RepeatingCallback<void(device::mojom::XRDeviceId)>
  DeviceIdCallbackMustBeCalled(base::RunLoop* loop) {
    return base::BindRepeating(
        [](base::OnceClosure quit_closure, device::mojom::XRDeviceId id) {
          std::move(quit_closure).Run();
        },
        loop->QuitClosure());
  };

  base::OnceClosure ClosureFailIfCalled() {
    return base::BindOnce([]() { FAIL(); });
  };

  base::OnceClosure ClosureMustBeCalled(base::RunLoop* loop) {
    return base::BindOnce(
        [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
        loop->QuitClosure());
  };

  // Needed for MakeRequest to work.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<VROrientationDeviceProvider> provider_;

  std::unique_ptr<FakeSensorProvider> fake_sensor_provider_;
  mojom::SensorProviderPtr sensor_provider_ptr_;

  // Fake Sensor Init params objects
  std::unique_ptr<FakeOrientationSensor> fake_sensor_;
  mojom::SensorPtrInfo sensor_ptr_;
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  mojom::SensorClientPtr sensor_client_ptr_;

  std::unique_ptr<service_manager::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(VROrientationDeviceProviderTest);
};

TEST_F(VROrientationDeviceProviderTest, InitializationTest) {
  // Check that without running anything, the provider will not be initialized.
  EXPECT_FALSE(provider_->Initialized());
}

TEST_F(VROrientationDeviceProviderTest, InitializationCallbackSuccessTest) {
  base::RunLoop wait_for_device;
  base::RunLoop wait_for_init;

  provider_->Initialize(DeviceAndIdCallbackMustBeCalled(&wait_for_device),
                        DeviceIdCallbackFailIfCalled(),
                        ClosureMustBeCalled(&wait_for_init));

  InitializeDevice(FakeInitParams());

  wait_for_init.Run();
  wait_for_device.Run();

  EXPECT_TRUE(provider_->Initialized());
}

TEST_F(VROrientationDeviceProviderTest, InitializationCallbackFailureTest) {
  base::RunLoop wait_for_init;

  provider_->Initialize(DeviceAndIdCallbackFailIfCalled(),
                        DeviceIdCallbackFailIfCalled(),
                        ClosureMustBeCalled(&wait_for_init));

  InitializeDevice(nullptr);

  // Wait for the initialization to finish.
  wait_for_init.Run();
  EXPECT_TRUE(provider_->Initialized());
}

}  // namespace device
