// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_DBUS_TRANSITION_H_
#define MOCK_DBUS_TRANSITION_H_

#include "dbus_transition.h"

#include <string>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>

namespace cryptohome {

class MockDBusReply : public DBusReply {
 public:
  MockDBusReply() : DBusReply(NULL, NULL) { }
  virtual ~MockDBusReply() {}

  MOCK_METHOD0(Run, void(void));
};

class MockDBusErrorReply : public DBusErrorReply {
 public:
  MockDBusErrorReply() : DBusErrorReply(NULL, NULL) { }
  virtual ~MockDBusErrorReply() {}

  MOCK_METHOD0(Run, void(void));
};

class MockDBusReplyFactory : public DBusReplyFactory {
 public:
  MockDBusReplyFactory() { }
  virtual ~MockDBusReplyFactory() { }

  MOCK_METHOD2(NewReply, DBusReply*(DBusGMethodInvocation*, std::string*));
  MOCK_METHOD2(NewErrorReply, DBusErrorReply*(DBusGMethodInvocation*, GError*));
};

}  // namespace cryptohome

#endif  /* !MOCK_DBUS_TRANSITION_H_ */
