// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xmpp/xmpp_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string>

#include <base/files/file_util.h>
#include <chromeos/data_encoding.h>
#include <chromeos/syslog_logging.h>

namespace buffet {

namespace {

std::string BuildXmppStartStreamCommand() {
  return "<stream:stream to='clouddevices.gserviceaccount.com' "
      "xmlns:stream='http://etherx.jabber.org/streams' "
      "xml:lang='*' version='1.0' xmlns='jabber:client'>";
}

std::string BuildXmppAuthenticateCommand(
    const std::string& account, const std::string& token) {
  chromeos::Blob credentials;
  credentials.push_back(0);
  credentials.insert(credentials.end(), account.begin(), account.end());
  credentials.push_back(0);
  credentials.insert(credentials.end(), token.begin(), token.end());
  std::string msg = "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' "
      "mechanism='X-OAUTH2' auth:service='oauth2' "
      "auth:allow-non-google-login='true' "
      "auth:client-uses-full-bind-result='true' "
      "xmlns:auth='http://www.google.com/talk/protocol/auth'>" +
      chromeos::data_encoding::Base64Encode(credentials) +
      "</auth>";
  return msg;
}

std::string BuildXmppBindCommand() {
  return "<iq type='set' id='0'>"
      "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'/></iq>";
}

std::string BuildXmppStartSessionCommand() {
  return "<iq type='set' id='1'>"
      "<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/></iq>";
}

std::string BuildXmppSubscribeCommand(const std::string& account) {
  return "<iq type='set' to='" + account + "' "
      "id='pushsubscribe1'><subscribe xmlns='google:push'>"
      "<item channel='cloud_devices' from=''/>"
      "</subscribe></iq>";
}

}  // namespace

void XmppClient::Read() {
  std::string msg;
  if (!connection_->Read(&msg) || msg.size() <= 0) {
    LOG(ERROR) << "Failed to read from stream";
    return;
  }

  // TODO(nathanbullock): Need to add support for TLS (brillo:191).
  switch (state_) {
    case XmppState::kStarted:
      if (std::string::npos != msg.find(":features") &&
          std::string::npos != msg.find("X-GOOGLE-TOKEN")) {
        state_ = XmppState::kAuthenticationStarted;
        connection_->Write(BuildXmppAuthenticateCommand(
            account_, access_token_));
      }
      break;
    case XmppState::kAuthenticationStarted:
      if (std::string::npos != msg.find("success")) {
        state_ = XmppState::kStreamRestartedPostAuthentication;
        connection_->Write(BuildXmppStartStreamCommand());
      }
      break;
    case XmppState::kStreamRestartedPostAuthentication:
      if (std::string::npos != msg.find(":features") &&
          std::string::npos != msg.find(":xmpp-session")) {
        state_ = XmppState::kBindSent;
        connection_->Write(BuildXmppBindCommand());
      }
      break;
    case XmppState::kBindSent:
      if (std::string::npos != msg.find("iq") &&
          std::string::npos != msg.find("result")) {
        state_ = XmppState::kSessionStarted;
        connection_->Write(BuildXmppStartSessionCommand());
      }
      break;
    case XmppState::kSessionStarted:
      if (std::string::npos != msg.find("iq") &&
          std::string::npos != msg.find("result")) {
        state_ = XmppState::kSubscribeStarted;
        connection_->Write(BuildXmppSubscribeCommand(account_));
      }
      break;
    case XmppState::kSubscribeStarted:
      if (std::string::npos != msg.find("iq") &&
          std::string::npos != msg.find("result")) {
        state_ = XmppState::kSubscribed;
      }
      break;
    default:
      break;
  }
}

void XmppClient::StartStream() {
  state_ = XmppState::kStarted;
  connection_->Write(BuildXmppStartStreamCommand());
}

}  // namespace buffet
