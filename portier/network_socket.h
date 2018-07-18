// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_NETWORK_SOCKET_H_
#define PORTIER_NETWORK_SOCKET_H_

#include <net/if.h>  // Interface stuff.

#include <string>

#include <base/macros.h>

#include "portier/ll_address.h"
#include "portier/status.h"

namespace portier {

// A generic interface for network related sockets.
class NetworkSocket {
 public:
  enum class State { UNINITIALIZED, READY, CLOSED };
  static std::string GetStateName(State state);

  // Socket file descriptor.  Returns -1 if the socket is closed.
  int fd() const { return fd_; }
  // Index of network interface as assigned by the kernel.  Used for various
  // system calls.
  int index() const { return index_; }
  // Name of network interface, as provided during construction.
  const std::string& name() const { return name_; }
  // Socket state.
  State state() const { return state_; }

  bool IsReady() const;
  bool IsClosed() const;

  // Interface information getters.

  // Get link-layer address assigned to this interface.
  Status GetLinkLayerAddress(LLAddress* ll_address) const;

  // Get the MTU of the interface.  This may not be available on all
  // interfaces.
  Status GetLinkMTU(uint32_t* mtu) const;

  // Determine if the interface is a loopback interface.
  Status GetLoopbackFlag(bool* loopback_flag) const;

  // Enabled or disable non-blocking mode.  When enabled, reads and writes
  // to the socket will not block.  If no data is available to receive,
  // read calls will return immediately without data.  Writing when the
  // interface is busy will queue packet to be sent.
  Status SetNonBlockingMode(bool enabled);

  // Closes socket.  Should be overrided by base class if there are
  // special steps required.
  virtual Status Close();

 protected:
  explicit NetworkSocket(const std::string& if_name);
  virtual Status Init() = 0;

  bool IsUnitialized() const;

  // A special close function used internally.  Does not make much
  // validation checking.
  void CloseFd();

  // Initializes the struct ifreq for making an ioctl() call for the
  // given socket's associated interface.  Simply sets all attributes
  // to zero and copies the name of the interface into the struct's
  // ifr_name field.
  void PrepareIfRequestStruct(struct ifreq* ifreq) const;

  // Sets and gets the Linux interface flags via an ioctl() call
  Status SetInterfaceFlags(int16_t flags);
  Status GetInterfaceFlags(int16_t* flags) const;

  // Interface name (Ex. eth0).
  std::string name_;

  // Interface index as identified by the kernel.
  int index_;

  // Socket file descriptor.
  int fd_;

  // Internal state of socket.
  State state_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSocket);
};

}  // namespace portier

#endif  // PORTIER_NETWORK_SOCKET_H_
