// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/testutil.h"

#include "server/peer_update_manager.h"
#include "server/mock_http_server.h"
#include "server/mock_service_publisher.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include <base/logging.h>
#include <base/bind.h>
#include <metrics/metrics_library_mock.h>

using testing::_;
using testing::AtLeast;
using testing::StrictMock;

using base::FilePath;

using p2p::testutil::SetupTestDir;
using p2p::testutil::TeardownTestDir;
using p2p::testutil::ExpectCommand;
using p2p::testutil::RunGMainLoop;

namespace p2p {

namespace server {

// If there are no files present, ensure that we don't publish
// anything and don't start the HTTP server
TEST(PeerUpdateManager, NoFilesPresent) {
  FilePath testdir = SetupTestDir("no-files-present");

  StrictMock<MockHttpServer> server;
  StrictMock<MockServicePublisher> publisher;
  StrictMock<MetricsLibraryMock> metrics_lib;

  EXPECT_CALL(server, SetNumConnectionsCallback(_)).Times(1);
  EXPECT_CALL(publisher, files()).Times(AtLeast(0));

  FileWatcher* watcher = FileWatcher::Construct(testdir, ".p2p");
  PeerUpdateManager manager(watcher, &publisher, &server, &metrics_lib);
  manager.Init();

  RunGMainLoop(2000);

  delete watcher;
  TeardownTestDir(testdir);
}

// If there are files present at startup, ensure that we publish them
// and start the HTTP server
TEST(PeerUpdateManager, FilesPresent) {
  FilePath testdir = SetupTestDir("files-present");

  StrictMock<MockHttpServer> server;
  StrictMock<MockServicePublisher> publisher;
  StrictMock<MetricsLibraryMock> metrics_lib;

  ExpectCommand(0, "touch %s", testdir.Append("a.p2p").value().c_str());
  ExpectCommand(0, "touch %s", testdir.Append("b.p2p").value().c_str());
  ExpectCommand(0, "echo -n xyz > %s", testdir.Append("c.p2p").value().c_str());

  EXPECT_CALL(server, SetNumConnectionsCallback(_)).Times(1);
  EXPECT_CALL(publisher, files()).Times(AtLeast(0));

  EXPECT_CALL(publisher, AddFile("a", 0)).Times(1);
  EXPECT_CALL(publisher, AddFile("b", 0)).Times(1);
  EXPECT_CALL(publisher, AddFile("c", 3)).Times(1);

  EXPECT_CALL(server, IsRunning()).Times((AtLeast(1)));
  EXPECT_CALL(server, Start()).Times((1));

  FileWatcher* watcher = FileWatcher::Construct(testdir, ".p2p");
  PeerUpdateManager manager(watcher, &publisher, &server, &metrics_lib);
  manager.Init();

  RunGMainLoop(2000);

  delete watcher;
  TeardownTestDir(testdir);
}

// If there are files present at startup and we remove one of them,
// check that the one we remove is subsequently removed from the publisher
TEST(PeerUpdateManager, RemoveFile) {
  FilePath testdir = SetupTestDir("remove-file");

  StrictMock<MockHttpServer> server;
  StrictMock<MockServicePublisher> publisher;
  StrictMock<MetricsLibraryMock> metrics_lib;

  ExpectCommand(0, "touch %s", testdir.Append("a.p2p").value().c_str());
  ExpectCommand(0, "touch %s", testdir.Append("b.p2p").value().c_str());
  ExpectCommand(0, "echo -n xyz > %s", testdir.Append("c.p2p").value().c_str());

  EXPECT_CALL(server, SetNumConnectionsCallback(_)).Times(1);
  EXPECT_CALL(publisher, files()).Times(AtLeast(0));

  EXPECT_CALL(publisher, AddFile("a", 0)).Times(1);
  EXPECT_CALL(publisher, AddFile("b", 0)).Times(1);
  EXPECT_CALL(publisher, AddFile("c", 3)).Times(1);
  EXPECT_CALL(publisher, RemoveFile("c")).Times(1);

  EXPECT_CALL(server, IsRunning()).Times((AtLeast(1)));
  EXPECT_CALL(server, Start()).Times((1));

  FileWatcher* watcher = FileWatcher::Construct(testdir, ".p2p");
  PeerUpdateManager manager(watcher, &publisher, &server, &metrics_lib);
  manager.Init();

  EXPECT_CALL(metrics_lib, SendToUMA("P2P.Server.FileCount", 2, _, _, _));
  ExpectCommand(0, "rm -f %s", testdir.Append("c.p2p").value().c_str());

  RunGMainLoop(2000);

  delete watcher;
  TeardownTestDir(testdir);
}

// If there are files present at startup and we remove all of them,
// check that they're removed and the HTTP server is stopped
TEST(PeerUpdateManager, RemoveLastFile) {
  FilePath testdir = SetupTestDir("remove-file");

  StrictMock<MockHttpServer> server;
  StrictMock<MockServicePublisher> publisher;
  StrictMock<MetricsLibraryMock> metrics_lib;

  ExpectCommand(0, "touch %s", testdir.Append("a.p2p").value().c_str());
  ExpectCommand(0, "touch %s", testdir.Append("b.p2p").value().c_str());

  EXPECT_CALL(server, SetNumConnectionsCallback(_)).Times(1);
  EXPECT_CALL(publisher, files()).Times(AtLeast(0));

  EXPECT_CALL(publisher, AddFile("a", 0)).Times(1);
  EXPECT_CALL(publisher, AddFile("b", 0)).Times(1);
  EXPECT_CALL(publisher, RemoveFile("b")).Times(1);
  EXPECT_CALL(publisher, RemoveFile("a")).Times(1);

  EXPECT_CALL(server, IsRunning()).Times((AtLeast(1)));
  EXPECT_CALL(server, Start()).Times((1));
  EXPECT_CALL(server, Stop()).Times((1));

  FileWatcher* watcher = FileWatcher::Construct(testdir, ".p2p");
  PeerUpdateManager manager(watcher, &publisher, &server, &metrics_lib);
  manager.Init();

  EXPECT_CALL(metrics_lib, SendToUMA("P2P.Server.FileCount", 1, _, _, _));
  EXPECT_CALL(metrics_lib, SendToUMA("P2P.Server.FileCount", 0, _, _, _));

  ExpectCommand(0, "rm -f %s", testdir.Append("a.p2p").value().c_str());
  ExpectCommand(0, "rm -f %s", testdir.Append("b.p2p").value().c_str());

  RunGMainLoop(2000);

  delete watcher;
  TeardownTestDir(testdir);
}

// Check that we propagate number of connections to the publisher
TEST(PeerUpdateManager, HttpNumConnections) {
  FilePath testdir = SetupTestDir("http-num-connections");

  StrictMock<MockHttpServer> server;
  StrictMock<MockServicePublisher> publisher;
  StrictMock<MetricsLibraryMock> metrics_lib;

  ExpectCommand(0, "touch %s", testdir.Append("a.p2p").value().c_str());
  ExpectCommand(0, "touch %s", testdir.Append("b.p2p").value().c_str());
  ExpectCommand(0, "echo -n xyz > %s", testdir.Append("c.p2p").value().c_str());

  EXPECT_CALL(server, SetNumConnectionsCallback(_)).Times(1);
  EXPECT_CALL(publisher, files()).Times(AtLeast(0));

  EXPECT_CALL(publisher, AddFile("a", 0)).Times(1);
  EXPECT_CALL(publisher, AddFile("b", 0)).Times(1);
  EXPECT_CALL(publisher, AddFile("c", 3)).Times(1);

  EXPECT_CALL(server, IsRunning()).Times((AtLeast(1)));
  EXPECT_CALL(server, Start()).Times((1));

  EXPECT_CALL(publisher, SetNumConnections(1)).Times(1);
  EXPECT_CALL(publisher, SetNumConnections(2)).Times(1);
  EXPECT_CALL(publisher, SetNumConnections(5)).Times(1);
  EXPECT_CALL(publisher, SetNumConnections(0)).Times(1);

  FileWatcher* watcher = FileWatcher::Construct(testdir, ".p2p");
  PeerUpdateManager manager(watcher, &publisher, &server, &metrics_lib);
  manager.Init();

  server.fake().SetNumConnections(1);
  server.fake().SetNumConnections(2);
  server.fake().SetNumConnections(5);
  server.fake().SetNumConnections(0);

  RunGMainLoop(2000);

  delete watcher;
  TeardownTestDir(testdir);
}

}  // namespace server

}  // namespace p2p
