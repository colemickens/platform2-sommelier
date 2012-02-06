// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ADAPTOR_INTERFACES_
#define SHILL_ADAPTOR_INTERFACES_

#include <string>
#include <vector>

#include <base/basictypes.h>

#include "shill/accessor_interface.h"

namespace shill {

class Error;

// These are the functions that a Device adaptor must support
class DeviceAdaptorInterface {
 public:
  virtual ~DeviceAdaptorInterface() {}

  // Getter for the opaque identifier that represents this object on the
  // RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcIdentifier() = 0;

  // Getter for the opaque identifier that represents this object's
  // connection to the RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcConnectionIdentifier() = 0;

  virtual void UpdateEnabled() = 0;

  virtual void EmitBoolChanged(const std::string &name, bool value) = 0;
  virtual void EmitUintChanged(const std::string &name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string &name, int value) = 0;
  virtual void EmitStringChanged(const std::string &name,
                                 const std::string &value) = 0;
  virtual void EmitStringmapsChanged(const std::string &name,
                                     const Stringmaps &value) = 0;
  virtual void EmitKeyValueStoreChanged(const std::string &name,
                                        const KeyValueStore &value) = 0;
};

// These are the functions that an IPConfig adaptor must support
class IPConfigAdaptorInterface {
 public:
  virtual ~IPConfigAdaptorInterface() {}

  // Getter for the opaque identifier that represents this object on the
  // RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcIdentifier() = 0;

  virtual void EmitBoolChanged(const std::string &name, bool value) = 0;
  virtual void EmitUintChanged(const std::string &name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string &name, int value) = 0;
  virtual void EmitStringChanged(const std::string &name,
                                 const std::string &value) = 0;
};

// These are the functions that a Manager adaptor must support
class ManagerAdaptorInterface {
 public:
  virtual ~ManagerAdaptorInterface() {}

  // Getter for the opaque identifier that represents this object on the
  // RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcIdentifier() = 0;

  virtual void UpdateRunning() = 0;

  virtual void EmitBoolChanged(const std::string &name, bool value) = 0;
  virtual void EmitUintChanged(const std::string &name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string &name, int value) = 0;
  virtual void EmitStringChanged(const std::string &name,
                                 const std::string &value) = 0;
  virtual void EmitStringsChanged(const std::string &name,
                                  const std::vector<std::string> &value) = 0;
  virtual void EmitRpcIdentifierArrayChanged(
      const std::string &name,
      const std::vector<std::string> &value) = 0;

  virtual void EmitStateChanged(const std::string &new_state) = 0;
};

// These are the functions that a Profile adaptor must support
class ProfileAdaptorInterface {
 public:
  virtual ~ProfileAdaptorInterface() {}

  // Getter for the opaque identifier that represents this object on the
  // RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcIdentifier() = 0;

  virtual void EmitBoolChanged(const std::string &name, bool value) = 0;
  virtual void EmitUintChanged(const std::string &name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string &name, int value) = 0;
  virtual void EmitStringChanged(const std::string &name,
                                 const std::string &value) = 0;
};

// These are the functions that a Service adaptor must support
class ServiceAdaptorInterface {
 public:
  virtual ~ServiceAdaptorInterface() {}

  // Getter for the opaque identifier that represents this object on the
  // RPC interface to which the implementation is adapting.
  virtual const std::string &GetRpcIdentifier() = 0;

  virtual void UpdateConnected() = 0;

  virtual void EmitBoolChanged(const std::string &name, bool value) = 0;
  virtual void EmitUint8Changed(const std::string &name, uint8 value) = 0;
  virtual void EmitUintChanged(const std::string &name, uint32 value) = 0;
  virtual void EmitIntChanged(const std::string &name, int value) = 0;
  virtual void EmitStringChanged(const std::string &name,
                                 const std::string &value) = 0;
  virtual void EmitStringmapChanged(const std::string &name,
                                    const Stringmap &value) = 0;
};

// A ReturnerInterface instance (along with its ownership) is passed by the
// adaptor to the method handler. The handler releases ownership and initiates
// an RPC return by calling one of the Return* methods.
class ReturnerInterface {
 public:
  virtual void Return() = 0;
  virtual void ReturnError(const Error &error) = 0;

 protected:
  // Destruction happens through the Return* methods.
  virtual ~ReturnerInterface() {}
};

}  // namespace shill
#endif  // SHILL_ADAPTOR_INTERFACES_
