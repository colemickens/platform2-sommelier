// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_OPENVPN_MANAGEMENT_SERVER_
#define SHILL_OPENVPN_MANAGEMENT_SERVER_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/cancelable_callback.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace shill {

class EventDispatcher;
class GLib;
class InputData;
class IOHandler;
class OpenVPNDriver;
class Sockets;

class OpenVPNManagementServer {
 public:
  OpenVPNManagementServer(OpenVPNDriver *driver, GLib *glib);
  virtual ~OpenVPNManagementServer();

  // Returns false on failure. On success, returns true and appends management
  // interface openvpn options to |options|.
  virtual bool Start(EventDispatcher *dispatcher,
                     Sockets *sockets,
                     std::vector<std::string> *options);

  virtual void Stop();

 private:
  friend class OpenVPNManagementServerTest;
  FRIEND_TEST(OpenVPNManagementServerTest, OnInput);
  FRIEND_TEST(OpenVPNManagementServerTest, OnReady);
  FRIEND_TEST(OpenVPNManagementServerTest, OnReadyAcceptFail);
  FRIEND_TEST(OpenVPNManagementServerTest, ParseNeedPasswordTag);
  FRIEND_TEST(OpenVPNManagementServerTest, PerformStaticChallenge);
  FRIEND_TEST(OpenVPNManagementServerTest, PerformStaticChallengeNoCreds);
  FRIEND_TEST(OpenVPNManagementServerTest, ProcessInfoMessage);
  FRIEND_TEST(OpenVPNManagementServerTest, ProcessMessage);
  FRIEND_TEST(OpenVPNManagementServerTest, ProcessNeedPasswordMessageAuthSC);
  FRIEND_TEST(OpenVPNManagementServerTest, ProcessNeedPasswordMessageTPMToken);
  FRIEND_TEST(OpenVPNManagementServerTest, ProcessNeedPasswordMessageUnknown);
  FRIEND_TEST(OpenVPNManagementServerTest, ProcessStateMessage);
  FRIEND_TEST(OpenVPNManagementServerTest, Send);
  FRIEND_TEST(OpenVPNManagementServerTest, SendPassword);
  FRIEND_TEST(OpenVPNManagementServerTest, SendState);
  FRIEND_TEST(OpenVPNManagementServerTest, SendUsername);
  FRIEND_TEST(OpenVPNManagementServerTest, Start);
  FRIEND_TEST(OpenVPNManagementServerTest, Stop);
  FRIEND_TEST(OpenVPNManagementServerTest, SupplyTPMToken);
  FRIEND_TEST(OpenVPNManagementServerTest, SupplyTPMTokenNoPIN);

  // IO handler callbacks.
  void OnReady(int fd);
  void OnInput(InputData *data);

  void Send(const std::string &data);
  void SendState(const std::string &state);
  void SendUsername(const std::string &tag, const std::string &username);
  void SendPassword(const std::string &tag, const std::string &password);

  void ProcessMessage(const std::string &message);
  bool ProcessInfoMessage(const std::string &message);
  bool ProcessNeedPasswordMessage(const std::string &message);
  bool ProcessFailedPasswordMessage(const std::string &message);
  bool ProcessStateMessage(const std::string &message);

  void PerformStaticChallenge(const std::string &tag);
  void SupplyTPMToken(const std::string &tag);

  static std::string ParseNeedPasswordTag(const std::string &message);

  OpenVPNDriver *driver_;
  GLib *glib_;
  base::WeakPtrFactory<OpenVPNManagementServer> weak_ptr_factory_;
  base::Callback<void(int)> ready_callback_;
  base::Callback<void(InputData *)> input_callback_;

  Sockets *sockets_;
  int socket_;
  scoped_ptr<IOHandler> ready_handler_;
  EventDispatcher *dispatcher_;
  int connected_socket_;
  scoped_ptr<IOHandler> input_handler_;

  DISALLOW_COPY_AND_ASSIGN(OpenVPNManagementServer);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_MANAGEMENT_SERVER_
