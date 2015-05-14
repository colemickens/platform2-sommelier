// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/notification/xmpp_channel.h"

#include <string>

#include <base/bind.h>
#include <chromeos/backoff_entry.h>
#include <chromeos/data_encoding.h>
#include <chromeos/streams/file_stream.h>

#include "buffet/notification/notification_delegate.h"
#include "buffet/utils.h"

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

// Backoff policy.
// Note: In order to ensure a minimum of 20 seconds between server errors,
// we have a 30s +- 10s (33%) jitter initial backoff.
const chromeos::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  30 * 1000,  // 30 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.33,  // 33%.

  // Maximum amount of time we are willing to delay our request in ms.
  10 * 60 * 1000,  // 10 minutes.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

const char kDefaultXmppHost[] = "talk.google.com";
const uint16_t kDefaultXmppPort = 5222;

}  // namespace

XmppChannel::XmppChannel(const std::string& account,
                         const std::string& access_token,
                         const scoped_refptr<base::TaskRunner>& task_runner)
    : account_{account},
      access_token_{access_token},
      backoff_entry_{&kDefaultBackoffPolicy},
      task_runner_{task_runner} {
  read_socket_data_.resize(4096);
}

void XmppChannel::OnMessageRead(size_t size) {
  std::string msg(read_socket_data_.data(), size);
  bool message_sent = false;

  VLOG(2) << "Received XMPP packet: " << msg;

  // TODO(nathanbullock): Need to add support for TLS (brillo:191).
  switch (state_) {
    case XmppState::kStarted:
      if (std::string::npos != msg.find(":features") &&
          std::string::npos != msg.find("X-GOOGLE-TOKEN")) {
        state_ = XmppState::kAuthenticationStarted;
        SendMessage(BuildXmppAuthenticateCommand(account_, access_token_));
        message_sent = true;
      }
      break;
    case XmppState::kAuthenticationStarted:
      if (std::string::npos != msg.find("success")) {
        state_ = XmppState::kStreamRestartedPostAuthentication;
        SendMessage(BuildXmppStartStreamCommand());
        message_sent = true;
      } else if (std::string::npos != msg.find("not-authorized")) {
        state_ = XmppState::kAuthenticationFailed;
        if (delegate_)
          delegate_->OnPermanentFailure();
        return;
      }
      break;
    case XmppState::kStreamRestartedPostAuthentication:
      if (std::string::npos != msg.find(":features") &&
          std::string::npos != msg.find(":xmpp-session")) {
        state_ = XmppState::kBindSent;
        SendMessage(BuildXmppBindCommand());
        message_sent = true;
      }
      break;
    case XmppState::kBindSent:
      if (std::string::npos != msg.find("iq") &&
          std::string::npos != msg.find("result")) {
        state_ = XmppState::kSessionStarted;
        SendMessage(BuildXmppStartSessionCommand());
        message_sent = true;
      }
      break;
    case XmppState::kSessionStarted:
      if (std::string::npos != msg.find("iq") &&
          std::string::npos != msg.find("result")) {
        state_ = XmppState::kSubscribeStarted;
        SendMessage(BuildXmppSubscribeCommand(account_));
        message_sent = true;
      }
      break;
    case XmppState::kSubscribeStarted:
      if (std::string::npos != msg.find("iq") &&
          std::string::npos != msg.find("result")) {
        state_ = XmppState::kSubscribed;
        if (delegate_)
          delegate_->OnConnected(GetName());
      }
      break;
    default:
      break;
  }
  if (!message_sent)
    WaitForMessage();
}

void XmppChannel::SendMessage(const std::string& message) {
  write_socket_data_ = message;
  chromeos::ErrorPtr error;
  VLOG(2) << "Sending XMPP message: " << message;

  bool ok = stream_->WriteAllAsync(
      write_socket_data_.data(),
      write_socket_data_.size(),
      base::Bind(&XmppChannel::OnMessageSent, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&XmppChannel::OnError, weak_ptr_factory_.GetWeakPtr()),
      &error);

  if (!ok)
    OnError(error.get());
}

void XmppChannel::OnMessageSent() {
  chromeos::ErrorPtr error;
  if (!stream_->FlushBlocking(&error)) {
    OnError(error.get());
    return;
  }
  WaitForMessage();
}

void XmppChannel::WaitForMessage() {
  chromeos::ErrorPtr error;
  bool ok = stream_->ReadAsync(
      read_socket_data_.data(),
      read_socket_data_.size(),
      base::Bind(&XmppChannel::OnMessageRead, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&XmppChannel::OnError, weak_ptr_factory_.GetWeakPtr()),
      &error);

  if (!ok)
    OnError(error.get());
}

void XmppChannel::OnError(const chromeos::Error* error) {
  Stop();
  Start(delegate_);
}

void XmppChannel::Connect(const std::string& host, uint16_t port,
                          const base::Closure& callback) {
  int socket_fd = ConnectSocket(host, port);
  if (socket_fd >= 0) {
    raw_socket_ =
      chromeos::FileStream::FromFileDescriptor(socket_fd, true, nullptr);
    if (!raw_socket_) {
      close(socket_fd);
      socket_fd = -1;
    }
  }

  backoff_entry_.InformOfRequest(raw_socket_ != nullptr);
  if (raw_socket_) {
    stream_ = raw_socket_.get();
    callback.Run();
  } else {
    VLOG(1) << "Delaying connection to XMPP server " << host << " for "
            << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
            << " milliseconds.";
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&XmppChannel::Connect, weak_ptr_factory_.GetWeakPtr(),
                    host, port, callback),
        backoff_entry_.GetTimeUntilRelease());
  }
}

std::string XmppChannel::GetName() const {
  return "xmpp";
}

void XmppChannel::AddChannelParameters(base::DictionaryValue* channel_json) {
  // No extra parameters needed for XMPP.
}

void XmppChannel::Start(NotificationDelegate* delegate) {
  CHECK(state_ == XmppState::kNotStarted);
  delegate_ = delegate;
  Connect(kDefaultXmppHost, kDefaultXmppPort,
          base::Bind(&XmppChannel::OnConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void XmppChannel::Stop() {
  if (state_ == XmppState::kSubscribed && delegate_)
    delegate_->OnDisconnected();

  weak_ptr_factory_.InvalidateWeakPtrs();
  if (raw_socket_) {
    raw_socket_->CloseBlocking(nullptr);
    raw_socket_.reset();
  }
  stream_ = nullptr;
  state_ = XmppState::kNotStarted;
}

void XmppChannel::OnConnected() {
  state_ = XmppState::kStarted;
  SendMessage(BuildXmppStartStreamCommand());
}

}  // namespace buffet
