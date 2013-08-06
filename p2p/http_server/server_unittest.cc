// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/constants.h"
#include "common/util.h"
#include "common/testutil.h"
#include "http_server/server.h"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <cctype>
#include <cinttypes>
#include <vector>
#include <tuple>

#include <gtest/gtest.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/file_path.h>
#include <base/threading/simple_thread.h>
#include <base/stringprintf.h>

using std::string;
using std::tuple;
using std::vector;

using base::FilePath;
using base::Time;
using base::TimeDelta;

using p2p::constants::kBytesPerMB;
using p2p::testutil::SetupTestDir;
using p2p::testutil::TeardownTestDir;
using p2p::testutil::ExpectCommand;
using p2p::testutil::ExpectFileSize;

namespace p2p {

namespace http_server {

// ------------------------------------------------------------------------

class BasicClientThread : public base::SimpleThread {
 public:
  BasicClientThread(FilePath testdir_path, uint16_t port, GMainLoop* loop)
      : base::SimpleThread("test-basic", base::SimpleThread::Options()),
        testdir_path_(testdir_path),
        port_(port),
        loop_(loop) {}

 private:
  virtual void Run();

  FilePath testdir_path_;
  uint16_t port_;
  GMainLoop* loop_;

  DISALLOW_COPY_AND_ASSIGN(BasicClientThread);
};

void BasicClientThread::Run() {
  const char* dir = testdir_path_.value().c_str();

  // Check download of entire file (16384 bytes)
  ExpectCommand(
      0, "curl -s -o %s/whole_file http://127.0.0.1:%d/file", dir, port_);
  ExpectCommand(0, "cmp -b %s/file.p2p %s/whole_file", dir, dir);
  ExpectFileSize(testdir_path_, "whole_file", 16384);

  // Check partial download (first 1000 bytes)
  ExpectCommand(0,
                "curl -s --range 0-999 "
                "-o %s/part_beginning http://127.0.0.1:%d/file",
                dir,
                port_);
  ExpectCommand(0, "cmp -b -n 1000 %s/file.p2p %s/part_beginning", dir, dir);
  ExpectFileSize(testdir_path_, "part_beginning", 1000);

  // Check partial download (800 bytes in the middle)
  ExpectCommand(0,
                "curl -s --range 5000-5799 "
                "-o %s/part_middle http://127.0.0.1:%d/file",
                dir,
                port_);
  ExpectCommand(0,
                "cmp -b -i 5000:0 -n 800 "
                "%s/file.p2p %s/part_middle",
                dir,
                dir);
  ExpectFileSize(testdir_path_, "part_middle", 800);

  // Check partial download (last 3384 bytes)
  ExpectCommand(0,
                "curl -s --range 13000- "
                "-o %s/part_end http://127.0.0.1:%d/file",
                dir,
                port_);
  ExpectCommand(0, "cmp -b -i 13000:0 %s/file.p2p %s/part_end", dir, dir);
  ExpectFileSize(testdir_path_, "part_end", 3384);

  // Check download of empty file succeeds
  //
  // (note that curl doesn't actually create the file - that's an
  // oddity with the curl program, not the code under test.)
  ExpectCommand(0, "curl -s http://127.0.0.1:%d/empty", port_);

  // Check download of non-existent file fails
  ExpectCommand(22, "curl --fail -s http://127.0.0.1:%d/not_here", port_);

  g_main_loop_quit(loop_);
}

TEST(P2PHttpServer, Basic) {
  FilePath testdir_path = SetupTestDir("basic");

  // Create an empty file
  ExpectCommand(
      0, "touch %s", testdir_path.Append("empty.p2p").value().c_str());

  // Create a file with well-known content (4096 32-bit ints)
  FilePath a_path = testdir_path.Append("file.p2p");
  int a_fd = open(a_path.value().c_str(), O_CREAT | O_WRONLY, 0644);
  EXPECT_NE(a_fd, -1);
  for (int n = 0; n < 4096; n++) {
    uint32_t encoded = htonl(n);
    EXPECT_EQ(write(a_fd, &encoded, 4), 4);
  }
  EXPECT_EQ(close(a_fd), 0);

  // Bring up the HTTP server
  Server server(testdir_path, 0);
  EXPECT_TRUE(server.Start());

  GMainLoop* loop = g_main_loop_new(NULL, FALSE);

  // Run clients in a separate thread
  BasicClientThread client_thread(testdir_path, server.Port(), loop);
  client_thread.Start();

  // The client thread will quit this mainloop once it's done
  g_main_loop_run(loop);

  // Cleanup
  g_main_loop_unref(loop);
  client_thread.Join();
  server.Stop();
  TeardownTestDir(testdir_path);
}

// ------------------------------------------------------------------------

class WaitingClientThread : public base::SimpleThread {
 public:
  WaitingClientThread(FilePath testdir_path,
                      uint16_t port,
                      GMainLoop* loop)
      : base::SimpleThread("test-waiting", base::SimpleThread::Options()),
        testdir_path_(testdir_path),
        port_(port),
        loop_(loop) {}

 private:
  virtual void Run();

  FilePath testdir_path_;
  uint16_t port_;
  GMainLoop* loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitingClientThread);
};

void WaitingClientThread::Run() {
  const char* dir = testdir_path_.value().c_str();

  // Check download of entire file (20000 bytes)
  ExpectCommand(0, "curl -s -o %s/file http://127.0.0.1:%d/file", dir, port_);
  ExpectCommand(0, "cmp -b %s/file.p2p %s/file", dir, dir);
  ExpectFileSize(testdir_path_, "file", 20000);

  g_main_loop_quit(loop_);
}

static gboolean p2p_waiting_test_write_more(gpointer user_data) {
  const FilePath* testdir_path = static_cast<FilePath*>(user_data);
  ExpectCommand(0,
                "dd if=/dev/zero of=%s/file.p2p conv=notrunc "
                "oflag=append bs=5000 count=1",
                testdir_path->value().c_str());
  return FALSE;  // Remove timeout source
}

TEST(P2PHttpServer, Waiting) {
  FilePath testdir_path = SetupTestDir("waiting");

  // Create a 10000 byte file with an EA saying it's 20000 bytes
  ExpectCommand(0,
                "dd if=/dev/zero of=%s/file.p2p bs=10000 count=1",
                testdir_path.value().c_str());
  ExpectCommand(0,
                "setfattr -n user.cros-p2p-filesize -v 20000 %s/file.p2p",
                testdir_path.value().c_str());

  // Now add some timeouts spaced apart
  //
  //  - first write another 5000 bytes to the file we're downloading
  //  - then write the final 5000 bytes to file file we're downloading
  //
  g_timeout_add(1000, p2p_waiting_test_write_more, &testdir_path);
  g_timeout_add(2000, p2p_waiting_test_write_more, &testdir_path);

  // Bring up the HTTP server
  Server server(testdir_path, 0);
  EXPECT_TRUE(server.Start());

  GMainLoop* loop = g_main_loop_new(NULL, FALSE);

  // Run clients in a separate thread
  WaitingClientThread client_thread(testdir_path, server.Port(), loop);
  client_thread.Start();

  // The client thread will quit this mainloop once it's done
  g_main_loop_run(loop);

  // Cleanup
  g_main_loop_unref(loop);
  client_thread.Join();
  server.Stop();
  TeardownTestDir(testdir_path);
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
  Server server(testdir_path, 0);
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

// ------------------------------------------------------------------------

class LimitDownloadSpeedThread : public base::SimpleThread {
 public:
  LimitDownloadSpeedThread(uint16_t http_port, GMainLoop* loop)
      : base::SimpleThread("test-basic", base::SimpleThread::Options()),
        http_port_(http_port),
        loop_(loop) {}

 private:
  virtual void Run() {
    ExpectCommand(
        0, "curl -s -o /dev/null http://127.0.0.1:%d/file", http_port_);
    g_main_loop_quit(loop_);
  }

  uint16_t http_port_;
  GMainLoop* loop_;

  DISALLOW_COPY_AND_ASSIGN(LimitDownloadSpeedThread);
};

static TimeDelta LimitDownloadSpeedDoDownload(Server *server) {
  Time begin_time, end_time;
  TimeDelta duration;

  begin_time = server->Clock()->GetMonotonicTime();

  // We need to run the mainloop to accept connections, so initiate the
  // download from another thread. The client thread will quit this
  // mainloop once it's done
  GMainLoop* loop = g_main_loop_new(NULL, FALSE);
  LimitDownloadSpeedThread client_thread(server->Port(), loop);
  client_thread.Start();
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  end_time = server->Clock()->GetMonotonicTime();
  duration = end_time - begin_time;

  client_thread.Join();

  return duration;
}

TEST(P2PHttpServer, LimitDownloadSpeed) {
  FilePath testdir_path = SetupTestDir("limit-download-speed");
  TimeDelta duration;
  double bytes_per_sec;
  int64_t file_size = 30 * kBytesPerMB;

  // Create the test file
  ExpectCommand(0,
                "dd if=/dev/zero of=%s/file.p2p bs=%d count=%d",
                testdir_path.value().c_str(),
                int(kBytesPerMB),
                int(file_size / kBytesPerMB));

  // Bring up the HTTP server
  Server server(testdir_path, 0);
  EXPECT_TRUE(server.Start());

  // Download the file without any limitation and check that it's more
  // than 50 MB/s (yes, weassume the test machine is at least _that_
  // capable).
  duration = LimitDownloadSpeedDoDownload(&server);
  bytes_per_sec = 1000.0 * file_size / duration.InMillisecondsF();
  EXPECT_GE(bytes_per_sec, 50 * kBytesPerMB);

  // Now limit the speed of the HTTP server to 5 MB/s and check
  // that we're approximately in that range.
  server.SetMaxDownloadRate(5 * kBytesPerMB);
  duration = LimitDownloadSpeedDoDownload(&server);
  bytes_per_sec = 1000.0 * file_size / duration.InMillisecondsF();
  EXPECT_GE(bytes_per_sec, 0.9 * 5 * kBytesPerMB);
  EXPECT_LE(bytes_per_sec, 1.1 * 5 * kBytesPerMB);

  server.Stop();
  TeardownTestDir(testdir_path);
}

static gboolean
OnExtendFile(gpointer user_data) {
  const FilePath *testdir_path = reinterpret_cast<const FilePath*>(user_data);
  int64_t file_size = 30 * kBytesPerMB;

  ExpectCommand(0,
                "dd if=/dev/zero of=%s/file.p2p bs=%d count=%d",
                testdir_path->value().c_str(),
                int(kBytesPerMB),
                int(file_size / kBytesPerMB));

  return FALSE; // Remove source.
}

TEST(P2PHttpServer, DisregardTimeWaitingFromTransferBudget) {
  FilePath testdir_path = SetupTestDir("disregard-time-waiting");
  TimeDelta duration;
  double bytes_per_sec;
  int64_t file_size = 30 * kBytesPerMB;

  // Create the test file and set its expected size.
  ExpectCommand(0,
                "touch %s/file.p2p",
                testdir_path.value().c_str());
  ExpectCommand(0,
                "setfattr -n user.cros-p2p-filesize -v %d %s/file.p2p",
                int(file_size),
                testdir_path.value().c_str());

  // Bring up the HTTP server
  Server server(testdir_path, 0);
  EXPECT_TRUE(server.Start());

  // Schedule the file to get bigger in 3 seconds
  g_timeout_add(3000, OnExtendFile, &testdir_path);

  // Limit the speed of the HTTP server to 5 MB/s and check that we're
  // approximately in that range AFTER subtracting the three seconds
  // we spent sleeping.
  server.SetMaxDownloadRate(5 * kBytesPerMB);
  duration = LimitDownloadSpeedDoDownload(&server) -
    TimeDelta::FromSeconds(3);
  bytes_per_sec = 1000.0 * file_size / duration.InMillisecondsF();
  EXPECT_GE(bytes_per_sec, 0.9 * 5 * kBytesPerMB);
  EXPECT_LE(bytes_per_sec, 1.1 * 5 * kBytesPerMB);

  server.Stop();
  TeardownTestDir(testdir_path);
}

}  // namespace http_server

}  // namespace p2p
