// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_XMPP_XMPP_CONNECTION_H_
#define BUFFET_XMPP_XMPP_CONNECTION_H_

#include <string>

#include <base/macros.h>

namespace buffet {

class XmppConnection {
 public:
  XmppConnection() {}
  virtual ~XmppConnection();

  // Initialize the XMPP client. (Connects to talk.google.com:5222).
  virtual bool Initialize();

  int GetFileDescriptor() const { return fd_; }

  // Needs to be called when new data is available from the connection.
  virtual bool Read(std::string* msg) const;

  // Start talking to the XMPP server (authenticate, etc.)
  virtual bool Write(const std::string& msg) const;

 private:
  // The file descriptor for the connection.
  int fd_{-1};

  DISALLOW_COPY_AND_ASSIGN(XmppConnection);
};

}  // namespace buffet

#endif  // BUFFET_XMPP_XMPP_CONNECTION_H_

