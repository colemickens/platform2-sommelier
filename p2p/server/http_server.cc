// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/constants.h"
#include "server/http_server.h"

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <glib.h>

#include <base/logging.h>

using std::string;
using std::vector;

using base::FilePath;

namespace p2p {

namespace server {

class HttpServerExternalProcess : public HttpServer {
 public:
  HttpServerExternalProcess(const FilePath& root_dir, uint16_t port);
  ~HttpServerExternalProcess();

  virtual bool Start();

  virtual bool Stop();

  virtual bool IsRunning();

  virtual void SetNumConnectionsCallback(NumConnectionsCallback callback);

 private:
  // Helper function to update |num_connections_| and fire
  // |num_connections_callback_| if something has changed.
  void UpdateNumConnections(int num_connections);

  // Used for waking up and processing stdout from the child process.
  // If the output matches lines of the form "num-connections: %d",
  // calls UpdateNumConnections() with the parsed integer.
  static gboolean OnIOChannelActivity(GIOChannel* source,
                                      GIOCondition condition,
                                      gpointer user_data);

  // The path to serve files from.
  FilePath root_dir_;

  // The TCP port number for the HTTP server.
  uint16_t port_;

  // The current number of connections to the HTTP server.
  int num_connections_;
  GPid pid_;
  int child_stdout_fd_;
  guint child_stdout_channel_source_id_;
  NumConnectionsCallback num_connections_callback_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerExternalProcess);
};

HttpServerExternalProcess::HttpServerExternalProcess(
    const FilePath& root_dir,
    uint16_t port)
    : root_dir_(root_dir),
      port_(port),
      num_connections_(0),
      pid_(0),
      child_stdout_fd_(-1) {}

HttpServerExternalProcess::~HttpServerExternalProcess() {
  if (child_stdout_channel_source_id_ != 0) {
    g_source_remove(child_stdout_channel_source_id_);
  }
  if (child_stdout_fd_ != -1) {
    close(child_stdout_fd_);
  }
  if (pid_ != 0) {
    kill(pid_, SIGTERM);
  }
}

bool HttpServerExternalProcess::Start() {
  string command_line;

  if (pid_ != 0) {
    LOG(ERROR) << "Server is already running with pid " << pid_;
    return false;
  }
  CHECK_EQ(child_stdout_fd_, -1);

  vector<const char*> args;
  string dir_arg = string("--directory=") + root_dir_.value();
  string port_arg = string("--port=") + std::to_string(port_);
  string http_binary_path = string(PACKAGE_SBIN_DIR "/") +
    p2p::constants::kHttpServerBinaryName;

  if (getenv("RUN_UNINSTALLED") != NULL) {
    http_binary_path = string(TOP_BUILDDIR "/http_server/") +
      p2p::constants::kHttpServerBinaryName;
  }

  args.push_back(http_binary_path.c_str());
  args.push_back(dir_arg.c_str());
  args.push_back(port_arg.c_str());
  args.push_back(NULL);

  GError* error = NULL;
  if (!g_spawn_async_with_pipes(NULL,  // working_dir
                                const_cast<gchar**>(args.data()),
                                NULL,  // envp
                                (GSpawnFlags) 0,
                                NULL,
                                NULL,  // child_setup, user_data
                                &pid_,
                                NULL,  // standard_input
                                &child_stdout_fd_,
                                NULL,  // standard_error
                                &error)) {
    LOG(ERROR) << "Error launching process: " << error->message;
    g_error_free(error);
    return false;
  }

  GIOChannel* io_channel = g_io_channel_unix_new(child_stdout_fd_);
  child_stdout_channel_source_id_ = g_io_add_watch(
      io_channel,
      static_cast<GIOCondition>(G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP),
      static_cast<GIOFunc>(OnIOChannelActivity),
      this);
  CHECK(child_stdout_channel_source_id_ != 0);
  g_io_channel_unref(io_channel);

  return true;
}

gboolean HttpServerExternalProcess::OnIOChannelActivity(GIOChannel* source,
                                                        GIOCondition condition,
                                                        gpointer user_data) {
  HttpServerExternalProcess* server =
      reinterpret_cast<HttpServerExternalProcess*>(user_data);
  gchar* str = NULL;

  GError* error = NULL;
  if (g_io_channel_read_line(source,
                             &str,
                             NULL,  // len
                             NULL,  // line_terminator
                             &error) !=
      G_IO_STATUS_NORMAL) {
    LOG(ERROR) << "Error reading from pipe: " << error->message;
    g_error_free(error);
    return TRUE;  // keep source around
  }

  int value = 0;
  if (sscanf(str, "num-connections: %d\n", &value) == 1) {
    server->UpdateNumConnections(value);
  } else {
    LOG(ERROR) << "Unrecognized status message `" << str << "'";
  }
  g_free(str);

  return TRUE;  // keep source around
}

void HttpServerExternalProcess::UpdateNumConnections(int num_connections) {
  if (num_connections_ == num_connections)
    return;

  num_connections_ = num_connections;

  if (!num_connections_callback_.is_null())
    num_connections_callback_.Run(num_connections);
}

bool HttpServerExternalProcess::Stop() {
  if (pid_ == 0) {
    LOG(ERROR) << "Server is not running";
    return false;
  }

  if (child_stdout_channel_source_id_ != 0) {
    g_source_remove(child_stdout_channel_source_id_);
  }
  child_stdout_channel_source_id_ = 0;

  if (child_stdout_fd_ != -1) {
    close(child_stdout_fd_);
  }
  child_stdout_fd_ = -1;

  if (pid_ != 0) {
    kill(pid_, SIGTERM);
  }
  pid_ = 0;

  return true;
}

bool HttpServerExternalProcess::IsRunning() { return pid_ != 0; }

void HttpServerExternalProcess::SetNumConnectionsCallback(
    NumConnectionsCallback callback) {
  num_connections_callback_ = callback;
}

// ----------------------------------------------------------------------------

HttpServer* HttpServer::Construct(const FilePath& root_dir,
                                  uint16_t port) {
  return new HttpServerExternalProcess(root_dir, port);
}

}  // namespace server

}  // namespace p2p
