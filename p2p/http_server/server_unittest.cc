// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/constants.h"
#include "common/server_message.h"
#include "common/struct_serializer.h"
#include "common/testutil.h"
#include "common/util.h"
#include "http_server/connection_delegate.h"
#include "http_server/fake_connection_delegate.h"
#include "http_server/server.h"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <cctype>
#include <cinttypes>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <base/command_line.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <base/threading/simple_thread.h>
#include <gtest/gtest.h>

using std::string;
using std::tuple;
using std::vector;

using base::FilePath;

using p2p::constants::kBytesPerMB;
using p2p::testutil::ExpectCommand;
using p2p::testutil::ExpectFileSize;
using p2p::testutil::SetupTestDir;
using p2p::testutil::TeardownTestDir;
using p2p::testutil::RunGMainLoopMaxIterations;
using p2p::util::P2PServerMessage;
using p2p::util::StructSerializerWatcher;

namespace p2p {

namespace http_server {

static void OnMessageReceivedAppend(
    const P2PServerMessage& msg, void* user_data) {
  vector<string> *messages = reinterpret_cast<vector<string>*>(user_data);
  EXPECT_TRUE(ValidP2PServerMessageMagic(msg));
  messages->push_back(ToString(msg));
}

static int ConnectToLocalPort(uint16_t port) {
  int sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
  if (sock == -1) {
    PLOG(ERROR) << "Creating a client socket()";
    NOTREACHED();
  }

  struct sockaddr_in server_addr;
  memset(reinterpret_cast<char*>(&server_addr), 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (connect(sock, reinterpret_cast<struct sockaddr*>(&server_addr),
              sizeof(server_addr)) == -1) {
    PLOG(ERROR) << "Connecting to localhost:" << port;
    NOTREACHED();
  }
  return sock;
}

TEST(P2PHttpServer, InvalidDirectoryFails) {
  FilePath testdir_path("/path/to/invalid/directory");
  Server server(testdir_path, 0, STDOUT_FILENO,
      FakeConnectionDelegate::Construct);
  EXPECT_FALSE(server.Start());
}

TEST(P2PHttpServer, AlreadyUsedPortFails) {
  FilePath testdir_path = SetupTestDir("reuse-port");

  // Create a server on a port number provided by the kernel.
  Server server1(testdir_path, 0, STDOUT_FILENO,
      FakeConnectionDelegate::Construct);
  EXPECT_TRUE(server1.Start());
  EXPECT_NE(server1.Port(), 0);

  // Attempt to create a server on the same port must fail.
  Server server2(testdir_path, server1.Port(), STDOUT_FILENO,
      FakeConnectionDelegate::Construct);
  EXPECT_FALSE(server2.Start());

  // Stop the first server allows the second server to run. This ensures that
  // we are closing the sockets properly on Stop().
  server1.Stop();
  EXPECT_TRUE(server2.Start());
  server2.Stop();

  TeardownTestDir(testdir_path);
}

TEST(P2PHttpServer, ReportServerMessageTest) {
  FilePath testdir_path = SetupTestDir("basic");

  // Redirect the ServerMessage to a pipe to verify the contents.
  // (requires the GLib main loop).
  vector<string> messages;
  int pipefd[2];
  ASSERT_EQ(0, pipe(pipefd));
  StructSerializerWatcher<P2PServerMessage> watch(
      pipefd[0],
      OnMessageReceivedAppend, reinterpret_cast<void*>(&messages));

  // Bring up the HTTP server.
  Server server(testdir_path, 0, pipefd[1], FakeConnectionDelegate::Construct);
  EXPECT_TRUE(server.Start());

  // Connect and disconnect a client. This doesn't block (at least accepts 1
  // connection).
  int sock = ConnectToLocalPort(server.Port());
  RunGMainLoopMaxIterations(100);
  EXPECT_EQ(0, close(sock));
  RunGMainLoopMaxIterations(100);
  server.Stop();
  RunGMainLoopMaxIterations(100);

  TeardownTestDir(testdir_path);

  // Check the messages reported by the Server.
  ASSERT_EQ(messages.size(), 3);
  EXPECT_EQ(messages[0], "{NumConnections: 1}");
  EXPECT_EQ(messages[1], "{ClientCount: 1}");
  EXPECT_EQ(messages[2], "{NumConnections: 0}");

  EXPECT_EQ(0, close(pipefd[0]));
  EXPECT_EQ(0, close(pipefd[1]));
}

// ------------------------------------------------------------------------

static const int kMultipleTestNumFiles = 5;

class MultipleClientThread : public base::SimpleThread {
 public:
  MultipleClientThread(FilePath testdir_path, uint16_t port, int num)
      : base::SimpleThread("test-multiple", base::SimpleThread::Options()),
        testdir_path_(testdir_path),
        port_(port),
        num_(num) {}

 private:
  virtual void Run() {
    const char* dir = testdir_path_.value().c_str();
    // Use --no-buffer, otherwise the target file will be zero bytes
    // when comparing in the function below
    ExpectCommand(0,
                  "curl --no-buffer -s -o %s/file%d "
                  "http://127.0.0.1:%d/file%d",
                  dir,
                  num_,
                  port_,
                  num_);
    ExpectCommand(0, "cmp -l -b %s/file%d.p2p %s/file%d", dir, num_, dir, num_);

    string name = StringPrintf("file%d", num_);
    ExpectFileSize(testdir_path_, name.c_str(), 2000);
  }

  FilePath testdir_path_;
  uint16_t port_;
  int num_;

  DISALLOW_COPY_AND_ASSIGN(MultipleClientThread);
};

static gboolean p2p_multiple_test_check(gpointer user_data) {
  tuple<const FilePath*, GMainLoop*, Server*>* tuple_val =
    static_cast<tuple<const FilePath*, GMainLoop*, Server*>*>(user_data);
  const FilePath* testdir_path = std::get<0>(*tuple_val);
  GMainLoop* loop = std::get<1>(*tuple_val);
  Server* server = std::get<2>(*tuple_val);

  EXPECT_EQ(server->NumConnections(), 5);

  for (int n = 0; n < kMultipleTestNumFiles; n++) {
    // Check the _downloaded_ file exists and matches the 1000 bytes
    // of the original file.
    ExpectCommand(0,
                  "cmp -b -n 1000 %s/file%d.p2p %s/file%d",
                  testdir_path->value().c_str(),
                  n,
                  testdir_path->value().c_str(),
                  n);
    // OK, complete it
    ExpectCommand(0,
                  "dd if=/dev/zero of=%s/file%d.p2p conv=notrunc oflag=append "
                  "bs=1000 count=1",
                  testdir_path->value().c_str(),
                  n);
  }

  g_main_loop_quit(loop);

  return FALSE;  // Remove timeout source
}

TEST(P2PHttpServer, Multiple) {
  FilePath testdir_path = SetupTestDir("multiple");

  // Create N 1000 byte files with EAs indicating length of 2000 bytes.
  // Put random content in them.
  for (int n = 0; n < kMultipleTestNumFiles; n++) {
    ExpectCommand(0,
                  "dd if=/dev/urandom of=%s/file%d.p2p bs=1000 count=1",
                  testdir_path.value().c_str(),
                  n);
    ExpectCommand(0,
                  "setfattr -n user.cros-p2p-filesize -v 2000 %s/file%d.p2p",
                  testdir_path.value().c_str(),
                  n);
  }

  // Bring up the HTTP server
  Server server(testdir_path, 0, STDOUT_FILENO, ConnectionDelegate::Construct);
  EXPECT_TRUE(server.Start());

  GMainLoop* loop = g_main_loop_new(NULL, FALSE);

  // Then start N threads for downloading, one for each file.
  vector<MultipleClientThread*> threads;
  for (int n = 0; n < kMultipleTestNumFiles; n++) {
    MultipleClientThread* thread =
        new MultipleClientThread(testdir_path, server.Port(), n);
    thread->Start();
    threads.push_back(thread);
  }

  // Schedule a timeout one second away - here we'll check that that
  // each download has begun ... this proves that the HTTP server is
  // capable of handling multiple requests, concurrently.
  //
  // Once we've done that, we'll quit the loop
  //
  tuple<const FilePath*, GMainLoop*, Server*> tuple_val =
      std::make_tuple(&testdir_path, loop, &server);
  g_timeout_add(1000, p2p_multiple_test_check, &tuple_val);

  g_main_loop_run(loop);

  // Wait for all downloads to finish
  for (auto& t : threads) {
    t->Join();
    delete t;
  }

  // Cleanup
  g_main_loop_unref(loop);
  server.Stop();
  TeardownTestDir(testdir_path);
}

}  // namespace http_server

}  // namespace p2p
