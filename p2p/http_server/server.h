// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_HTTP_SERVER_SERVER_H__
#define P2P_HTTP_SERVER_SERVER_H__

#include "http_server/server_interface.h"
#include "common/server_message.h"
#include "common/clock_interface.h"

#include <string>
#include <map>

#include <glib.h>

#include <base/command_line.h>
#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <base/threading/simple_thread.h>


namespace p2p {

namespace http_server {

class ConnectionDelegate;

class Server : public ServerInterface {
 public:
  // Constructs a new Server object.
  //
  // This constructor doesn't start the server - to start listening on
  // the socket, the Start() method will need to be called.
  Server(const base::FilePath& directory, uint16 port);

  virtual ~Server();

  // ServerInterface override methods.
  virtual bool Start();
  virtual void Stop();
  virtual void SetMaxDownloadRate(int64_t bytes_per_sec);
  virtual uint16 Port();
  virtual int NumConnections();
  virtual p2p::common::ClockInterface* Clock();
  virtual void ConnectionTerminated(ConnectionDelegate* delegate);
  virtual void ReportServerMessage(p2p::util::P2PServerMessageType msg_type,
                                   int64_t value);

 private:
  // Callback used clients connect to our listening socket.
  static gboolean OnIOChannelActivity(GIOChannel* source,
                                      GIOCondition condition,
                                      gpointer data);

  // Updates number of connections. May be called from any thread.
  //
  // As a side-effect, prints the number of connection on stdout
  // for reporting to higher-level code.
  void UpdateNumConnections(int delta_num_connections);

  // Clock used for time-keeping and sleeping.
  //
  // TODO(zeuthen): Make it possible to set this to a FakeClock,
  // probably as part of resolving crbug.com/269212. When doing that,
  // remember to make p2p::common::FakeClock thread-safe (e.g. use
  // atomic operations) since we're going to use it from multiple
  // threads.
  scoped_ptr<p2p::common::ClockInterface> clock_;

  // Thread pool used for worker threads.
  base::DelegateSimpleThreadPool thread_pool_;

  // The path of the directory we're serving .p2p files from.
  base::FilePath directory_;

  // The file descriptor for the directory corresponding to |directory_|.
  int dirfd_;

  // The TCP port to listen on.
  uint16 port_;

  // The maximum download rate of 0 if there is no limit.
  int64_t max_download_rate_;

  // Set to true only if the server is running.
  bool started_;

  // The file descriptor for the socket we're listening on.
  int listen_fd_;

  // The GLib source id for our socket.
  guint listen_source_id_;

  // The current number of connected clients.
  int num_connections_;

  // Object-wide lock.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(Server);
};

}  // namespace http_server

}  // namespace p2p

#endif  // P2P_HTTP_SERVER_SERVER_H__
