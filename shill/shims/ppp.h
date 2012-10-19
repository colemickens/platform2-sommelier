// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHIMS_PPP_H_
#define SHILL_SHIMS_PPP_H_

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace base {
class AtExitManager;
}  // namespace base

namespace DBus {
class BusDispatcher;
class Connection;
}  // namespace DBus

namespace shill {

namespace shims {

class TaskProxy;

class PPP {
 public:
  PPP();
  ~PPP();

  void Start();
  void Stop();

  bool GetSecret(std::string *username, std::string *password);
  void OnConnect(const std::string &ifname);
  void OnDisconnect();

 private:
  bool CreateProxy();
  void DestroyProxy();

  static std::string ConvertIPToText(const void *addr);

  scoped_ptr<DBus::BusDispatcher> dispatcher_;
  scoped_ptr<DBus::Connection> connection_;
  scoped_ptr<TaskProxy> proxy_;

  static base::AtExitManager *exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(PPP);
};

}  // namespace shims

}  // namespace shill

#endif  // SHILL_SHIMS_PPP_H_
