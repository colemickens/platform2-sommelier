// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONTROL_INTERFACE_
#define SHILL_CONTROL_INTERFACE_

#include <string>

namespace shill {

class Manager;

using std::string;

// This is the Inteface for "partner" objects which are in charge of
// RPC to the various core classes.
class ProxyInterface {
 public:
  virtual void SetProperty(const string &key, const string &value) = 0;
  virtual const string *GetProperty(const string &key) = 0;
  virtual void ClearProperty(const string &key) = 0;
  virtual ~ProxyInterface() {}
};

// These are the functions that a Manager proxy must support
class ManagerProxyInterface {
 public:
  virtual void UpdateRunning() = 0;
  virtual ~ManagerProxyInterface() {}
};

// This is the Interface for an object factory that creates proxy objects
class ControlInterface {
 public:
  virtual ManagerProxyInterface *CreateManagerProxy(Manager *manager) = 0;
  virtual ~ControlInterface() {}
};

}  // namespace shill
#endif  // SHILL_CONTROL_INTERFACE_
