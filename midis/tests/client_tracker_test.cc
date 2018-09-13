// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <base/run_loop.h>
#include <brillo/daemons/daemon.h>
#include <brillo/test_helpers.h>
#include <gtest/gtest.h>
#include <mojo/public/cpp/bindings/interface_request.h>

#include "midis/client_tracker.h"
#include "midis/tests/test_helper.h"

namespace midis {

class ClientTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override { message_loop_.SetAsCurrent(); }

 private:
  brillo::BaseMessageLoop message_loop_;
};

// Check whether we can connect add and remove a client from ClientTracker.
TEST_F(ClientTrackerTest, AddClientPositive) {
  DeviceTracker device_tracker;
  ClientTracker cli_tracker;
  cli_tracker.SetDeviceTracker(&device_tracker);
  cli_tracker.InitClientTracker();

  arc::mojom::MidisServerPtr server;
  mojo::InterfaceRequest<arc::mojom::MidisServer> request =
      mojo::MakeRequest(&server);

  auto client_pointer = std::make_unique<arc::mojom::MidisClientPtr>();
  cli_tracker.MakeMojoClient(std::move(request), std::move(*client_pointer));
  EXPECT_EQ(1, cli_tracker.GetNumClientsForTesting());

  // Try removing a non-existent client.
  cli_tracker.RemoveClient(-1);
  EXPECT_EQ(1, cli_tracker.GetNumClientsForTesting());

  // Get the client ID so that we can issue the delete command.
  int client_id = cli_tracker.clients_.begin()->first;
  cli_tracker.RemoveClient(client_id);
  EXPECT_EQ(0, cli_tracker.GetNumClientsForTesting());
}

}  // namespace midis
