// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_management_server.h"

#include <netinet/in.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/event_dispatcher.h"
#include "shill/openvpn_driver.h"
#include "shill/sockets.h"

using base::Bind;
using base::SplitString;
using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

OpenVPNManagementServer::OpenVPNManagementServer(OpenVPNDriver *driver)
    : driver_(driver),
      weak_ptr_factory_(this),
      ready_callback_(Bind(&OpenVPNManagementServer::OnReady,
                           weak_ptr_factory_.GetWeakPtr())),
      input_callback_(Bind(&OpenVPNManagementServer::OnInput,
                           weak_ptr_factory_.GetWeakPtr())),
      sockets_(NULL),
      socket_(-1),
      dispatcher_(NULL),
      connected_socket_(-1) {}

OpenVPNManagementServer::~OpenVPNManagementServer() {
  Stop();
}

bool OpenVPNManagementServer::Start(EventDispatcher *dispatcher,
                                    Sockets *sockets) {
  VLOG(2) << __func__;
  if (sockets_) {
    return true;
  }

  int socket = sockets->Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket < 0) {
    PLOG(ERROR) << "Unable to create management server socket.";
    return false;
  }

  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (sockets->Bind(
          socket, reinterpret_cast<struct sockaddr *>(&addr), addrlen) < 0 ||
      sockets->Listen(socket, 1) < 0 ||
      sockets->GetSockName(
          socket, reinterpret_cast<struct sockaddr *>(&addr), &addrlen) < 0) {
    PLOG(ERROR) << "Socket setup failed.";
    sockets->Close(socket);
    return false;
  }

  VLOG(2) << "Listening socket: " << socket;
  sockets_ = sockets;
  socket_ = socket;
  ready_handler_.reset(
      dispatcher->CreateReadyHandler(
          socket, IOHandler::kModeInput, ready_callback_));
  dispatcher_ = dispatcher;
  return true;
}

void OpenVPNManagementServer::Stop() {
  VLOG(2) << __func__;
  if (!sockets_) {
    return;
  }
  input_handler_.reset();
  if (connected_socket_ >= 0) {
    sockets_->Close(connected_socket_);
    connected_socket_ = -1;
  }
  dispatcher_ = NULL;
  ready_handler_.reset();
  if (socket_ >= 0) {
    sockets_->Close(socket_);
    socket_ = -1;
  }
  sockets_ = NULL;
}

void OpenVPNManagementServer::OnReady(int fd) {
  VLOG(2) << __func__ << "(" << fd << ")";
  connected_socket_ = sockets_->Accept(fd, NULL, NULL);
  if (connected_socket_ < 0) {
    PLOG(ERROR) << "Connected socket accept failed.";
    return;
  }
  ready_handler_.reset();
  input_handler_.reset(
      dispatcher_->CreateInputHandler(connected_socket_, input_callback_));
  SendState("on");
}

void OpenVPNManagementServer::OnInput(InputData *data) {
  VLOG(2) << __func__ << "(" << data->len << ")";
  vector<string> messages;
  SplitString(
      string(reinterpret_cast<char *>(data->buf), data->len), '\n', &messages);
  for (vector<string>::const_iterator it = messages.begin();
       it != messages.end(); ++it) {
    ProcessMessage(*it);
  }
}

void OpenVPNManagementServer::ProcessMessage(const string &message) {
  VLOG(2) << __func__ << "(" << message << ")";
  LOG_IF(WARNING,
         !ProcessInfoMessage(message) &&
         !ProcessNeedPasswordMessage(message) &&
         !ProcessFailedPasswordMessage(message) &&
         !ProcessStateMessage(message))
      << "OpenVPN management message ignored: " << message;
}

bool OpenVPNManagementServer::ProcessInfoMessage(const string &message) {
  return StartsWithASCII(message, ">INFO:", true);
}

bool OpenVPNManagementServer::ProcessNeedPasswordMessage(
    const string &message) {
  if (!StartsWithASCII(message, ">PASSWORD:Need ", true)) {
    return false;
  }
  NOTIMPLEMENTED();
  return true;
}

bool OpenVPNManagementServer::ProcessFailedPasswordMessage(
    const string &message) {
  if (!StartsWithASCII(message, ">PASSWORD:Verification Failed:", true)) {
    return false;
  }
  NOTIMPLEMENTED();
  return true;
}

// >STATE:* message support. State messages are of the form:
//    >STATE:<date>,<state>,<detail>,<local-ip>,<remote-ip>
// where:
// <date> is the current time (since epoch) in seconds
// <state> is one of:
//    INITIAL, CONNECTING, WAIT, AUTH, GET_CONFIG, ASSIGN_IP, ADD_ROUTES,
//    CONNECTED, RECONNECTING, EXITING, RESOLVE, TCP_CONNECT
// <detail> is a free-form string giving details about the state change
// <local-ip> is a dotted-quad for the local IPv4 address (when available)
// <remote-ip> is a dotted-quad for the remote IPv4 address (when available)
bool OpenVPNManagementServer::ProcessStateMessage(const string &message) {
  if (!StartsWithASCII(message, ">STATE:", true)) {
    return false;
  }
  vector<string> details;
  SplitString(message, ',', &details);
  if (details.size() > 1) {
    if (details[1] == "RECONNECTING") {
      driver_->OnReconnecting();
    }
    // The rest of the states are currently ignored.
  }
  return true;
}

void OpenVPNManagementServer::Send(const string &data) {
  VLOG(2) << __func__ << "(" << data << ")";
  ssize_t len = sockets_->Send(connected_socket_, data.data(), data.size(), 0);
  PLOG_IF(ERROR, len < 0 || static_cast<size_t>(len) != data.size())
      << "Send failed: " << data;
}

void OpenVPNManagementServer::SendState(const string &state) {
  VLOG(2) << __func__ << "(" << state << ")";
  Send(StringPrintf("state %s\n", state.c_str()));
}

}  // namespace shill
