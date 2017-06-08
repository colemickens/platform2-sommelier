// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/scoped_file.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/threading/thread.h>
#include <base/time/time.h>
#include <brillo/daemons/daemon.h>
#include <brillo/test_helpers.h>
#include <gtest/gtest.h>

#include "midis/client_tracker.h"
#include "midis/clientlib.h"
#include "midis/tests/test_helper.h"

namespace midis {

namespace {

const char kClientThreadName[] = "client_thread";
}

class ClientTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    CreateNewTempDirectory(base::FilePath::StringType(), &temp_fp_);
    message_loop_.SetAsCurrent();
  }

  void TearDown() override { base::DeleteFile(temp_fp_, true); }

  base::FilePath temp_fp_;

 private:
  brillo::BaseMessageLoop message_loop_;
};

void ConnectToClient(base::FilePath socketDir) {
  // Try connecting to the client
  std::string socket_path = socketDir.Append("midis_socket").value();
  base::ScopedFD server_fd =
      base::ScopedFD(MidisConnectToServer(socket_path.c_str()));
  ASSERT_TRUE(server_fd.is_valid());

  ASSERT_EQ(0, MidisListDevices(server_fd.get()));
  uint32_t payload_size;
  uint32_t type;
  ASSERT_EQ(0, MidisProcessMsgHeader(server_fd.get(), &payload_size, &type));
  EXPECT_EQ(LIST_DEVICES_RESPONSE, type);

  std::vector<uint8_t> buffer(payload_size);
  ASSERT_EQ(payload_size,
            MidisProcessListDevices(server_fd.get(), buffer.data(),
                                    buffer.size(), payload_size));
  EXPECT_EQ(buffer[0], 0);
}

void ServerCheckClientsCallback(ClientTracker* cli_tracker,
                                base::Closure quit) {
  EXPECT_EQ(cli_tracker->GetNumClientsForTesting(), 1);
  quit.Run();
}

// Check whether we can connect successfully to the ClientTracker.
TEST_F(ClientTrackerTest, AddClientPositive) {
  ASSERT_FALSE(temp_fp_.empty());
  base::FilePath socket_dir = CreateFakeTempSubDir(temp_fp_, "run/midis");
  ASSERT_NE(socket_dir.value(), "");

  DeviceTracker device_tracker;
  ClientTracker cli_tracker;
  cli_tracker.SetBaseDirForTesting(temp_fp_);
  ASSERT_TRUE(cli_tracker.InitClientTracker(&device_tracker));

  base::Thread client_thread(kClientThreadName);
  ASSERT_TRUE(client_thread.Start());

  base::RunLoop run_loop;
  client_thread.task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&ConnectToClient, socket_dir),
      base::Bind(&ServerCheckClientsCallback, &cli_tracker,
                 run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace midis
