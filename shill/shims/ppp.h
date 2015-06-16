// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHIMS_PPP_H_
#define SHILL_SHIMS_PPP_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <base/lazy_instance.h>

namespace DBus {
class BusDispatcher;
class Connection;
}  // namespace DBus

namespace shill {

namespace shims {

class TaskProxy;

class PPP {
 public:
  ~PPP();

  // This is a singleton -- use PPP::GetInstance()->Foo().
  static PPP* GetInstance();

  void Init();

  bool GetSecret(std::string* username, std::string* password);
  void OnAuthenticateStart();
  void OnAuthenticateDone();
  void OnConnect(const std::string& ifname);
  void OnDisconnect();

 protected:
  PPP();

 private:
  friend struct base::DefaultLazyInstanceTraits<PPP>;

  bool CreateProxy();
  void DestroyProxy();

  static std::string ConvertIPToText(const void* addr);

  std::unique_ptr<DBus::BusDispatcher> dispatcher_;
  std::unique_ptr<DBus::Connection> connection_;
  std::unique_ptr<TaskProxy> proxy_;
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(PPP);
};

}  // namespace shims

}  // namespace shill

#endif  // SHILL_SHIMS_PPP_H_
