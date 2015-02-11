// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/xmpp/xmpp_connection.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string>

#include <base/files/file_util.h>
#include <chromeos/syslog_logging.h>

namespace buffet {

XmppConnection::~XmppConnection() {
  if (fd_ > 0) {
    close(fd_);
  }
}

bool XmppConnection::Initialize() {
  LOG(INFO) << "Opening XMPP connection";

  fd_ = socket(AF_INET, SOCK_STREAM, 0);

  // TODO(nathanbullock): Use gethostbyname_r.
  struct hostent* server = gethostbyname("talk.google.com");
  if (server == NULL) {
    LOG(WARNING) << "Failed to find host talk.google.com";
    return false;
  }

  sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy(server->h_addr, &(serv_addr.sin_addr), server->h_length);
  serv_addr.sin_port = htons(5222);

  if (connect(fd_, (struct sockaddr*)&serv_addr, sizeof(sockaddr)) < 0) {
    LOG(WARNING) << "Failed to connect to talk.google.com:5222";
    return false;
  }

  return true;
}

bool XmppConnection::Read(std::string* msg) const {
  char buffer[4096];  // This should be large enough for our purposes.
  ssize_t bytes = HANDLE_EINTR(read(fd_, buffer, sizeof(buffer)));
  if (bytes < 0) {
    LOG(WARNING) << "Failure reading";
    return false;
  }
  *msg = std::string{buffer, static_cast<size_t>(bytes)};
  LOG(INFO) << "Read: (" << msg->size() << ")" << *msg;
  return true;
}

bool XmppConnection::Write(const std::string& msg) const {
  LOG(INFO) << "Write: (" << msg.size() << ")" << msg;
  if (!base::WriteFileDescriptor(fd_, msg.c_str(), msg.size())) {
    LOG(WARNING) << "Failure writing";
    return false;
  }
  return true;
}

}  // namespace buffet
