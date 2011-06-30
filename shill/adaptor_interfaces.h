// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ADAPTOR_INTERFACES_
#define SHILL_ADAPTOR_INTERFACES_

#include <string>

#include <base/basictypes.h>

namespace shill {

// These are the functions that a Manager adaptor must support
class ManagerAdaptorInterface {
 public:
  virtual ~ManagerAdaptorInterface() {}

  // Getter for the opaque identifier that represents this object on the
  // RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcIdentifier() = 0;

  virtual void UpdateRunning() = 0;

  virtual void EmitBoolChanged(const std::string& name, bool value) = 0;
  virtual void EmitUintChanged(const std::string& name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string& name, int value) = 0;
  virtual void EmitStringChanged(const std::string& name,
                                 const std::string& value) = 0;

  virtual void EmitStateChanged(const std::string& new_state) = 0;
};

// These are the functions that a Service adaptor must support
class ServiceAdaptorInterface {
 public:
  virtual ~ServiceAdaptorInterface() {}

  // Getter for the opaque identifier that represents this object on the
  // RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcIdentifier() = 0;

  virtual void UpdateConnected() = 0;

  virtual void EmitBoolChanged(const std::string& name, bool value) = 0;
  virtual void EmitUintChanged(const std::string& name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string& name, int value) = 0;
  virtual void EmitStringChanged(const std::string& name,
                                 const std::string& value) = 0;
};

// These are the functions that a Device adaptor must support
class DeviceAdaptorInterface {
 public:
  virtual ~DeviceAdaptorInterface() {}

  // Getter for the opaque identifier that represents this object on the
  // RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcIdentifier() = 0;

  virtual void UpdateEnabled() = 0;

  virtual void EmitBoolChanged(const std::string& name, bool value) = 0;
  virtual void EmitUintChanged(const std::string& name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string& name, int value) = 0;
  virtual void EmitStringChanged(const std::string& name,
                                 const std::string& value) = 0;
};

// These are the functions that a Profile adaptor must support
class ProfileAdaptorInterface {
 public:
  virtual ~ProfileAdaptorInterface() {}

  virtual void EmitBoolChanged(const std::string& name, bool value) = 0;
  virtual void EmitUintChanged(const std::string& name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string& name, int value) = 0;
  virtual void EmitStringChanged(const std::string& name,
                                 const std::string& value) = 0;
};

}  // namespace shill
#endif  // SHILL_ADAPTOR_INTERFACES_
