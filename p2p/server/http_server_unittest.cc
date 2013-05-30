// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/testutil.h"
#include "server/http_server.h"

#include <glib-object.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <base/bind.h>
#include <base/threading/simple_thread.h>
#include <base/stringprintf.h>

using testing::_;
using testing::StrictMock;

using p2p::testutil::SetupTestDir;
using p2p::testutil::TeardownTestDir;
using p2p::testutil::ExpectCommand;
using p2p::testutil::ExpectFileSize;
using p2p::testutil::RunGMainLoop;

using std::string;
using std::vector;

using base::FilePath;

namespace p2p {

namespace server {

// ------------------------------------------------------------------------

class HttpServerListener {
 public:
  explicit HttpServerListener(HttpServer* server) {
    server->SetNumConnectionsCallback(base::Bind(
        &HttpServerListener::NumConnectionsCallback, base::Unretained(this)));
  }

  virtual void NumConnectionsCallback(int num_connections) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerListener);
};

class MockHttpServerListener : public HttpServerListener {
 public:
  explicit MockHttpServerListener(HttpServer* server)
      : HttpServerListener(server) {}
  MOCK_METHOD1(NumConnectionsCallback, void(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHttpServerListener);
};

// ------------------------------------------------------------------------

static const int kMultipleTestNumFiles = 5;

class ClientThread : public base::SimpleThread {
 public:
  ClientThread(FilePath testdir_path, uint16_t port, int num)
      : base::SimpleThread("client", base::SimpleThread::Options()),
        testdir_path_(testdir_path),
        port_(port),
        num_(num) {}

 private:
  virtual void Run() {
    const char* dir = testdir_path_.value().c_str();
    ExpectCommand(0,
                  "curl -s -o %s/dl_%d "
                  "http://127.0.0.1:%d/file",
                  dir,
                  num_,
                  port_);
    ExpectCommand(0, "cmp -l -b %s/file.p2p %s/dl_%d", dir, dir, num_);

    string name = StringPrintf("dl_%d", num_);
    ExpectFileSize(testdir_path_, name.c_str(), 2000);
  }

  FilePath testdir_path_;
  uint16_t port_;
  int num_;

  DISALLOW_COPY_AND_ASSIGN(ClientThread);
};

TEST(HttpServer, Basic) {
  FilePath testdir = SetupTestDir("http-server");

  // Forces HttpServer to run p2p-http-server from the build
  // directory
  setenv("RUN_UNINSTALLED", "1", 1);

  HttpServer* server = HttpServer::Construct(testdir, 16725);
  server->Start();
  StrictMock<MockHttpServerListener> listener(server);

  // Now set the expectations for the number of connections. We'll
  // climb all the way up to N and then go back to 0. So we'll
  // get to each integer in the open interval twice and each
  // of the boundary points just once, e.g. for N=5
  //
  // 0 -> 1  (twice)
  // 1 -> 2  (twice)
  // 2 -> 3  (twice)
  // 3 -> 4  (twice)
  // 4 -> 5  (once)
  // 5 -> 4  (twice)
  // 4 -> 3  (twice)
  // 3 -> 2  (twice)
  // 2 -> 1  (twice)
  // 1 -> 0  (once)
  //
  for (int n = 0; n <= kMultipleTestNumFiles; n++) {
    int times = 2;
    if (n == 0 || n == kMultipleTestNumFiles)
      times = 1;
    EXPECT_CALL(listener, NumConnectionsCallback(n)).Times(times);
  }

  // Create a 1000 byte file (with random content) with an EAs
  // indicating that it's 2000 bytes. This will make clients hang
  // and thus enable us to reliably get the NumConnections count
  // to N.
  ExpectCommand(0,
                "dd if=/dev/urandom of=%s/file.p2p bs=1000 count=1",
                testdir.value().c_str());
  ExpectCommand(0,
                "setfattr -n user.cros-p2p-filesize -v 2000 %s/file.p2p",
                testdir.value().c_str());

  // Start N threads for downloading, one for each file.
  vector<ClientThread*> threads;
  for (int n = 0; n < kMultipleTestNumFiles; n++) {
    ClientThread* thread = new ClientThread(testdir, 16725, n);
    thread->Start();
    threads.push_back(thread);
  }

  // Allow clients to start - this ensures that the server reaches
  // the number of connections
  RunGMainLoop(500);

  // Now, complete the file. This causes each client to finish up
  ExpectCommand(0,
                "dd if=/dev/zero of=%s/file.p2p conv=notrunc "
                "oflag=append bs=1000 count=1",
                testdir.value().c_str());

  // Wait for all downloads to finish
  for (auto& t : threads) {
    t->Join();
    delete t;
  }

  RunGMainLoop(2000);

  server->Stop();
  delete server;
  TeardownTestDir(testdir);
}

}  // namespace server

}  // namespace p2p
