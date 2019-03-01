// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/run_loop.h>
#include <brillo/message_loops/base_message_loop.h>
#include <gtest/gtest.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/edk/embedder/test_embedder.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <mojo/public/cpp/bindings/interface_request.h>

#include "midis/client.h"
#include "midis/device_tracker.h"
#include "mojo/midis.mojom.h"

// Local implementation of the mojo MidisClient interface.
class ClientImpl : public arc::mojom::MidisClient {
 public:
  ~ClientImpl() override{};
  void OnDeviceAdded(arc::mojom::MidisDeviceInfoPtr device) override {}

  void OnDeviceRemoved(arc::mojom::MidisDeviceInfoPtr device) override {}

  void BindClientPtr(arc::mojom::MidisClientPtr* ptr) {
    mojo::InterfaceRequest<arc::mojom::MidisClient> request(ptr);
    binding_ = base::MakeUnique<mojo::Binding<arc::mojom::MidisClient>>(
        this, std::move(request));
  }

 private:
  std::unique_ptr<mojo::Binding<arc::mojom::MidisClient>> binding_;
};

namespace midis {

class ClientTest : public ::testing::Test {
 public:
  ClientTest() = default;
  ~ClientTest() override = default;
 protected:
  void SetUp() override {
    message_loop_.SetAsCurrent();
    mojo::edk::Init();
  }

  void TearDown() override {
    ASSERT_TRUE(mojo::edk::test::Shutdown());
  }
 private:
  brillo::BaseMessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ClientTest);
};

// Check that the MidisServer implementation sends back the correct
// number of devices.
TEST_F(ClientTest, ListDevices) {
  DeviceTracker tracker;
  arc::mojom::MidisServerPtr server;

  ClientImpl client;
  arc::mojom::MidisClientPtr ptr;
  client.BindClientPtr(&ptr);

  Client client_class(&tracker, 0, base::Bind([](uint32_t client_id) {}),
                      mojo::MakeRequest(&server), std::move(ptr));

  // Check that initially there are no devices listed.
  int64_t num_devices = -1;
  server->ListDevices(base::Bind(
      [](int64_t* num_devices,
         std::vector<arc::mojom::MidisDeviceInfoPtr> devices) {
        *num_devices = devices.size();
      },
      &num_devices));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(num_devices, 0);

  // TODO(b/122623049): Add a device, then check that ListDevices works as
  // expected.
}

}  // namespace midis
