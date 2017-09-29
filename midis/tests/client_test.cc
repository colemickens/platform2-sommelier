// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/scoped_file.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/threading/thread.h>
#include <base/time/time.h>
#include <brillo/daemons/daemon.h>
#include <brillo/test_helpers.h>
#include <gtest/gtest.h>

#include "midis/client_tracker.h"
#include "midis/clientlib.h"
#include "midis/constants.h"
#include "midis/tests/test_helper.h"

namespace {

const char kClientThreadName[] = "client_thread";

const char kFakeName1[] = "Sample MIDI Device - 1";
const char kFakeManufacturer1[] = "Foo";
const uint32_t kFakeSysNum1 = 2;
const uint32_t kFakeDevNum1 = 0;
const uint32_t kFakeSubdevs1 = 1;
const uint32_t kFakeFlags1 = 7;

const char kFakeMidiData1[] = "0xDEADBEEF";

}  // namespace

namespace midis {

base::ScopedFD RequestFakePort(int server_fd,
                               const struct MidisRequestPort& port_msg) {
  EXPECT_EQ(0, MidisRequestPort(server_fd, &port_msg));

  // Receive the port back.
  uint32_t payload_size;
  uint32_t type;
  EXPECT_EQ(0, MidisProcessMsgHeader(server_fd, &payload_size, &type));
  EXPECT_EQ(REQUEST_PORT_RESPONSE, type);

  struct MidisRequestPort port_msg_ret;
  memset(&port_msg_ret, 0, sizeof(port_msg_ret));

  base::ScopedFD portfd(
      MidisProcessRequestPortResponse(server_fd, &port_msg_ret));
  EXPECT_TRUE(portfd.is_valid());

  // Make sure the returned FD is for the port we requested.
  EXPECT_EQ(port_msg_ret.card, port_msg.card);
  EXPECT_EQ(port_msg_ret.device_num, port_msg.device_num);
  EXPECT_EQ(port_msg_ret.subdevice_num, port_msg.subdevice_num);

  return portfd;
}

void ConnectToClient(base::FilePath socketDir, base::FilePath devNodePath) {
  uint8_t buf[kMaxBufSize];
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
  EXPECT_EQ(buffer[0], 1);

  struct MidisRequestPort port_msg;
  memset(&port_msg, 0, sizeof(port_msg));
  port_msg.card = kFakeSysNum1;
  port_msg.device_num = kFakeDevNum1;
  port_msg.subdevice_num = kFakeSubdevs1 - 1;

  base::ScopedFD portfd = RequestFakePort(server_fd.get(), port_msg);

  // Create another client
  base::ScopedFD server_fd2 =
      base::ScopedFD(MidisConnectToServer(socket_path.c_str()));
  ASSERT_TRUE(server_fd2.is_valid());

  base::ScopedFD portfd2 = RequestFakePort(server_fd2.get(), port_msg);

  // Write data to the dev-node to fake the MIDI h/w generating messages.
  ASSERT_EQ(sizeof(kFakeMidiData1), base::WriteFile(devNodePath, kFakeMidiData1,
                                                    sizeof(kFakeMidiData1)));

  // Read it back and verify.
  memset(buf, 0, kMaxBufSize);
  EXPECT_EQ(sizeof(kFakeMidiData1), read(portfd.get(), buf, kMaxBufSize));
  EXPECT_EQ(0, memcmp(buf, kFakeMidiData1, sizeof(kFakeMidiData1)));
  memset(buf, 0, kMaxBufSize);
  EXPECT_EQ(sizeof(kFakeMidiData1), read(portfd2.get(), buf, kMaxBufSize));
  EXPECT_EQ(0, memcmp(buf, kFakeMidiData1, sizeof(kFakeMidiData1)));
}

void CheckClientCallback(ClientTracker* cli_tracker, base::Closure quit) {
  EXPECT_EQ(cli_tracker->GetNumClientsForTesting(), 2);
  quit.Run();
}

class ClientTest : public ::testing::Test {
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

// Check the following basic sequence.
// - Connect to a client tracker.
// - Request list of devices (DeviceTracker has a device added)
// - Request for a sub-device from the device in question.
// - Confirm that the data received is what was sent by the device.
TEST_F(ClientTest, AddClientAndReceiveMessages) {
  ASSERT_FALSE(temp_fp_.empty());
  base::FilePath socket_dir = CreateFakeTempSubDir(temp_fp_, "run/midis");
  ASSERT_NE(socket_dir.value(), "");

  base::FilePath dev_path = CreateFakeTempSubDir(temp_fp_, "dev/snd");
  ASSERT_NE(dev_path.value(), "");

  base::FilePath dev_node_path =
      CreateDevNodeFileName(dev_path, kFakeSysNum1, kFakeDevNum1);

  // Create a fake devnode and allow a poll on it.
  ASSERT_EQ(0, mkfifo(dev_node_path.value().c_str(),
                      S_IRGRP | S_IWGRP | S_IRUSR | S_IWUSR));

  Device::SetBaseDirForTesting(temp_fp_);

  DeviceTracker device_tracker;
  ClientTracker cli_tracker;
  cli_tracker.SetBaseDirForTesting(temp_fp_);
  ASSERT_TRUE(cli_tracker.InitClientTracker(&device_tracker));

  device_tracker.AddDevice(
      std::make_unique<Device>(kFakeName1, kFakeManufacturer1, kFakeSysNum1,
                               kFakeDevNum1, kFakeSubdevs1, kFakeFlags1));
  ASSERT_EQ(device_tracker.devices_.size(), 1);

  // Start the monitoring for the device, so that the file handlers
  // are created correctly.
  auto dev = device_tracker.devices_.find(
      device_tracker.udev_handler_->GenerateDeviceId(kFakeSysNum1,
                                                     kFakeDevNum1));
  ASSERT_NE(dev, device_tracker.devices_.end());
  dev->second->StartMonitoring();

  base::Thread client_thread(kClientThreadName);
  ASSERT_TRUE(client_thread.Start());

  base::RunLoop run_loop;
  client_thread.task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&ConnectToClient, socket_dir, dev_node_path),
      base::Bind(&CheckClientCallback, &cli_tracker, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace midis
