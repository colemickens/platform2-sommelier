// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_XMPP_XMPP_CLIENT_H_
#define BUFFET_XMPP_XMPP_CLIENT_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "buffet/xmpp/xmpp_connection.h"

namespace buffet {

class XmppClient final {
 public:
  // |account| is the robot account for buffet and |access_token|
  // it the OAuth token. Note that the OAuth token expires fairly frequently
  // so you will need to reset the XmppClient every time this happens.
  XmppClient(const std::string& account,
             const std::string& access_token,
             std::unique_ptr<XmppConnection> connection)
      : account_{account},
        access_token_{access_token},
        connection_{std::move(connection)} {}

  int GetFileDescriptor() const {
    return connection_->GetFileDescriptor();
  }

  // Needs to be called when new data is available from the connection.
  bool Read();

  // Start talking to the XMPP server (authenticate, etc.)
  void StartStream();

  // Internal states for the XMPP stream.
  enum class XmppState {
    kNotStarted,
    kStarted,
    kAuthenticationStarted,
    kAuthenticationFailed,
    kStreamRestartedPostAuthentication,
    kBindSent,
    kSessionStarted,
    kSubscribeStarted,
    kSubscribed,
  };

 private:
  // Robot account name for the device.
  std::string account_;

  // OAuth access token for the account. Expires fairly frequently.
  std::string access_token_;

  // The connection to the XMPP server.
  std::unique_ptr<XmppConnection> connection_;

  XmppState state_{XmppState::kNotStarted};

  friend class TestHelper;
  DISALLOW_COPY_AND_ASSIGN(XmppClient);
};

}  // namespace buffet

#endif  // BUFFET_XMPP_XMPP_CLIENT_H_

