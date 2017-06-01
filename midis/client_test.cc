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

namespace {

const char kClientThreadName[] = "client_thread";

const char kFakeName1[] = "Sample MIDI Device - 1";
const uint32_t kFakeSysNum1 = 2;
const uint32_t kFakeDevNum1 = 0;
const uint32_t kFakeSubdevs1 = 1;
const uint32_t kFakeFlags1 = 7;

const char kFakeMidiData1[] = "0xDEADBEEF";

const int kMaxBufSize = 1024;

}  // namespace

namespace midis {

void ConnectToClient(base::FilePath socketDir, base::FilePath devNodePath) {
  uint8_t buf[kMaxBufSize];
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

  // Get list of devices
  struct MidisMessageHeader header;
  header.type = REQUEST_LIST_DEVICES;
  header.payload_size = 0;
  int bytes = write(server_fd.get(), &header, sizeof(header));
  ASSERT_EQ(bytes, sizeof(header));

  bytes = read(server_fd.get(), &header, sizeof(header));
  ASSERT_EQ(bytes, sizeof(header));
  EXPECT_EQ(header.type, LIST_DEVICES_RESPONSE);

  bytes = read(server_fd.get(), buf, header.payload_size);
  EXPECT_EQ(buf[0], 1);
  EXPECT_EQ(bytes, (sizeof(MidisDeviceInfo) * buf[0]) + 1);

  // Request a port
  memset(&header, 0, sizeof(header));
  header.type = REQUEST_PORT;
  header.payload_size = 0;
  bytes = write(server_fd.get(), &header, sizeof(header));
  ASSERT_EQ(bytes, sizeof(struct MidisMessageHeader));

  struct MidisRequestPort port_msg = {0};
  port_msg.card = kFakeSysNum1;
  port_msg.device_num = kFakeDevNum1;
  port_msg.subdevice_num = kFakeSubdevs1 - 1;

  bytes = write(server_fd.get(), &port_msg, sizeof(port_msg));
  ASSERT_EQ(bytes, sizeof(struct MidisRequestPort));

  // Receive the port back.
  memset(&header, 0, sizeof(header));
  bytes = read(server_fd.get(), &header, sizeof(header));
  EXPECT_EQ(bytes, sizeof(header));
  EXPECT_EQ(header.type, REQUEST_PORT_RESPONSE);

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  struct iovec iov;

  memset(&port_msg, 0, sizeof(struct MidisRequestPort));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  iov.iov_base = &port_msg;
  iov.iov_len = sizeof(struct MidisRequestPort);

  int portfd = -1;
  const unsigned int control_size = CMSG_SPACE(sizeof(portfd));
  std::vector<char> control(control_size);
  msg.msg_control = control.data();
  msg.msg_controllen = control.size();

  int rc = recvmsg(server_fd.get(), &msg, 0);
  EXPECT_NE(rc, 0);

  size_t num_fds = 1;
  struct cmsghdr* cmsg;
  for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      memcpy(&portfd, CMSG_DATA(cmsg), num_fds * sizeof(portfd));
      break;
    }
  }

  // Make sure the returned FD is for the port we requested.
  EXPECT_EQ(port_msg.card, kFakeSysNum1);
  EXPECT_EQ(port_msg.device_num, kFakeDevNum1);
  EXPECT_EQ(port_msg.subdevice_num, kFakeSubdevs1 - 1);

  // Write data to the dev-node
  ASSERT_EQ(sizeof(kFakeMidiData1), base::WriteFile(devNodePath, kFakeMidiData1,
                                                    sizeof(kFakeMidiData1)));

  memset(buf, 0, kMaxBufSize);
  EXPECT_EQ(sizeof(kFakeMidiData1), read(portfd, buf, kMaxBufSize));
  EXPECT_EQ(0, memcmp(buf, kFakeMidiData1, sizeof(kFakeMidiData1)));
}

void ServerCheckClientsCallback(ClientTracker* cli_tracker,
                                base::Closure quit) {
  EXPECT_EQ(cli_tracker->GetNumClientsForTesting(), 1);
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

  device_tracker.AddDevice(base::MakeUnique<Device>(
      kFakeName1, kFakeSysNum1, kFakeDevNum1, kFakeSubdevs1, kFakeFlags1));
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
      base::Bind(&ServerCheckClientsCallback, &cli_tracker,
                 run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace midis
