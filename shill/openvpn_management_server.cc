// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_management_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/event_dispatcher.h"
#include "shill/glib.h"
#include "shill/openvpn_driver.h"
#include "shill/scope_logger.h"
#include "shill/sockets.h"

using base::Bind;
using base::IntToString;
using base::SplitString;
using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

OpenVPNManagementServer::OpenVPNManagementServer(OpenVPNDriver *driver,
                                                 GLib *glib)
    : driver_(driver),
      glib_(glib),
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
  OpenVPNManagementServer::Stop();
}

bool OpenVPNManagementServer::Start(EventDispatcher *dispatcher,
                                    Sockets *sockets,
                                    vector<string> *options) {
  SLOG(VPN, 2) << __func__;
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

  SLOG(VPN, 2) << "Listening socket: " << socket;
  sockets_ = sockets;
  socket_ = socket;
  ready_handler_.reset(
      dispatcher->CreateReadyHandler(
          socket, IOHandler::kModeInput, ready_callback_));
  dispatcher_ = dispatcher;

  // Append openvpn management API options.
  options->push_back("--management");
  options->push_back(inet_ntoa(addr.sin_addr));
  options->push_back(IntToString(ntohs(addr.sin_port)));
  options->push_back("--management-client");
  options->push_back("--management-query-passwords");
  driver_->AppendFlag(
      flimflam::kOpenVPNAuthUserPassProperty, "--auth-user-pass", options);
  if (driver_->AppendValueOption(flimflam::kOpenVPNStaticChallengeProperty,
                                 "--static-challenge",
                                 options)) {
    options->push_back("1");  // Force echo.
  }
  return true;
}

void OpenVPNManagementServer::Stop() {
  SLOG(VPN, 2) << __func__;
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
  SLOG(VPN, 2) << __func__ << "(" << fd << ")";
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
  SLOG(VPN, 2) << __func__ << "(" << data->len << ")";
  vector<string> messages;
  SplitString(
      string(reinterpret_cast<char *>(data->buf), data->len), '\n', &messages);
  for (vector<string>::const_iterator it = messages.begin();
       it != messages.end(); ++it) {
    ProcessMessage(*it);
  }
}

void OpenVPNManagementServer::ProcessMessage(const string &message) {
  SLOG(VPN, 2) << __func__ << "(" << message << ")";
  if (message.empty()) {
    return;
  }
  if (!ProcessInfoMessage(message) &&
      !ProcessNeedPasswordMessage(message) &&
      !ProcessFailedPasswordMessage(message) &&
      !ProcessStateMessage(message)) {
    LOG(WARNING) << "OpenVPN management message ignored: " << message;
  }
}

bool OpenVPNManagementServer::ProcessInfoMessage(const string &message) {
  SLOG(VPN, 2) << __func__ << "(" << message << ")";
  if (!StartsWithASCII(message, ">INFO:", true)) {
    return false;
  }
  LOG(INFO) << "Processing info message.";
  return true;
}

bool OpenVPNManagementServer::ProcessNeedPasswordMessage(
    const string &message) {
  SLOG(VPN, 2) << __func__ << "(" << message << ")";
  if (!StartsWithASCII(message, ">PASSWORD:Need ", true)) {
    return false;
  }
  LOG(INFO) << "Processing need-password message.";
  string tag = ParseNeedPasswordTag(message);
  if (tag == "Auth") {
    if (message.find("SC:") != string::npos) {
      PerformStaticChallenge(tag);
    } else {
      NOTIMPLEMENTED()
          << ": User/password (no-OTP) authentication not implemented.";
      driver_->Cleanup(Service::kStateFailure);
    }
  } else if (StartsWithASCII(tag, "User-Specific TPM Token", true)) {
    SupplyTPMToken(tag);
  } else {
    NOTIMPLEMENTED() << ": Unsupported need-password message: " << message;
    driver_->Cleanup(Service::kStateFailure);
  }
  return true;
}

// static
string OpenVPNManagementServer::ParseNeedPasswordTag(const string &message) {
  SLOG(VPN, 2) << __func__ << "(" << message << ")";
  size_t start = message.find('\'');
  if (start == string::npos) {
    return string();
  }
  size_t end = message.find('\'', start + 1);
  if (end == string::npos) {
    return string();
  }
  return message.substr(start + 1, end - start - 1);
}

void OpenVPNManagementServer::PerformStaticChallenge(const string &tag) {
  LOG(INFO) << "Perform static challenge: " << tag;
  string user =
      driver_->args()->LookupString(flimflam::kOpenVPNUserProperty, "");
  string password =
      driver_->args()->LookupString(flimflam::kOpenVPNPasswordProperty, "");
  string otp =
      driver_->args()->LookupString(flimflam::kOpenVPNOTPProperty, "");
  if (user.empty() || password.empty() || otp.empty()) {
    NOTIMPLEMENTED() << ": Missing credentials.";
    driver_->Cleanup(Service::kStateFailure);
    return;
  }
  gchar *b64_password =
      glib_->Base64Encode(reinterpret_cast<const guchar *>(password.data()),
                          password.size());
  gchar *b64_otp =
      glib_->Base64Encode(reinterpret_cast<const guchar *>(otp.data()),
                          otp.size());
  if (!b64_password || !b64_otp) {
    LOG(ERROR) << "Unable to base64-encode credentials.";
    return;
  }
  SendUsername(tag, user);
  SendPassword(tag, StringPrintf("SCRV1:%s:%s", b64_password, b64_otp));
  glib_->Free(b64_otp);
  glib_->Free(b64_password);
  // Don't reuse OTP.
  driver_->args()->RemoveString(flimflam::kOpenVPNOTPProperty);
}

void OpenVPNManagementServer::SupplyTPMToken(const string &tag) {
  SLOG(VPN, 2) << __func__ << "(" << tag << ")";
  string pin = driver_->args()->LookupString(flimflam::kOpenVPNPinProperty, "");
  if (pin.empty()) {
    NOTIMPLEMENTED() << ": Missing PIN.";
    driver_->Cleanup(Service::kStateFailure);
    return;
  }
  SendPassword(tag, pin);
}

bool OpenVPNManagementServer::ProcessFailedPasswordMessage(
    const string &message) {
  SLOG(VPN, 2) << __func__ << "(" << message << ")";
  if (!StartsWithASCII(message, ">PASSWORD:Verification Failed:", true)) {
    return false;
  }
  NOTIMPLEMENTED();
  driver_->Cleanup(Service::kStateFailure);
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
  SLOG(VPN, 2) << __func__ << "(" << message << ")";
  if (!StartsWithASCII(message, ">STATE:", true)) {
    return false;
  }
  LOG(INFO) << "Processing state message.";
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
  SLOG(VPN, 2) << __func__;
  ssize_t len = sockets_->Send(connected_socket_, data.data(), data.size(), 0);
  PLOG_IF(ERROR, len < 0 || static_cast<size_t>(len) != data.size())
      << "Send failed.";
}

void OpenVPNManagementServer::SendState(const string &state) {
  SLOG(VPN, 2) << __func__ << "(" << state << ")";
  Send(StringPrintf("state %s\n", state.c_str()));
}

void OpenVPNManagementServer::SendUsername(const string &tag,
                                           const string &username) {
  SLOG(VPN, 2) << __func__;
  Send(StringPrintf("username \"%s\" %s\n", tag.c_str(), username.c_str()));
}

void OpenVPNManagementServer::SendPassword(const string &tag,
                                           const string &password) {
  SLOG(VPN, 2) << __func__;
  Send(StringPrintf("password \"%s\" \"%s\"\n", tag.c_str(), password.c_str()));
}

}  // namespace shill
