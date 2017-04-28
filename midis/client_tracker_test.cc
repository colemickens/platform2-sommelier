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
#include "midis/test_helper.h"

using ::testing::_;

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
  base::AtExitManager at_exit_;
  brillo::BaseMessageLoop message_loop_;
};

void ConnectToClient(base::FilePath socketDir) {
  uint8_t buf[1024];
  // Try connecting to the client
  struct sockaddr_un addr;
  base::ScopedFD server_fd = base::ScopedFD(socket(AF_UNIX, SOCK_SEQPACKET, 0));

  ASSERT_TRUE(server_fd.is_valid());

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  std::string socket_path = socketDir.Append("midis_socket").value();
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path.c_str());

  int ret = connect(server_fd.get(), reinterpret_cast<sockaddr*>(&addr),
                    sizeof(addr));
  ASSERT_EQ(ret, 0);

  struct MidisMessageHeader header;
  header.type = REQUEST_LIST_DEVICES;
  header.payload_size = 0;
  int bytes =
      write(server_fd.get(), &header, sizeof(struct MidisMessageHeader));
  ASSERT_EQ(bytes, sizeof(struct MidisMessageHeader));

  bytes = read(server_fd.get(), &header, sizeof(struct MidisMessageHeader));
  ASSERT_EQ(bytes, sizeof(struct MidisMessageHeader));

  EXPECT_EQ(header.type, LIST_DEVICES_RESPONSE);

  bytes = read(server_fd.get(), buf, header.payload_size);
  EXPECT_EQ(bytes, 0);
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

  ClientTracker cli_tracker;
  cli_tracker.SetBaseDirForTesting(temp_fp_);
  ASSERT_TRUE(cli_tracker.InitClientTracker(nullptr));

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

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
