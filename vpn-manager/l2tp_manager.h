// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _VPN_MANAGER_L2TP_MANAGER_H_
#define _VPN_MANAGER_L2TP_MANAGER_H_

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "gtest/gtest_prod.h"  // for FRIEND_TEST
#include "vpn-manager/service_manager.h"

namespace chromeos {
class Process;
}

// Manages the L2TP daemon.  This manager orchestrates configuring and
// launching the L2TP daemon, initiating the L2TP connection, and
// detecting when PPP has been set up.  It also sends user credentials
// to PPP through the L2TP control fifo unless the user has specified
// a PPP plugin should be used, which it will defer to.  Current
// implementation assumes a connection that has been stopped will not
// be started again with the same object.
class L2tpManager : public ServiceManager {
 public:
  L2tpManager();

  // Initialize the object using |remote_host|.  Returns false if
  // an illegal set of parameters has been given.  Has no side effects
  // other than setting up the object.
  bool Initialize(const std::string& remote_host);

  virtual bool Start();
  virtual void Stop();
  virtual int Poll();
  virtual void ProcessOutput();
  virtual bool IsChild(pid_t pid);

  // Returns the stderr output file descriptor of our child process.
  int output_fd() const { return output_fd_; }

 private:
  friend class L2tpManagerTest;
  FRIEND_TEST(L2tpManagerTest, FormatL2tpdConfiguration);
  FRIEND_TEST(L2tpManagerTest, FormatPppdConfiguration);
  FRIEND_TEST(L2tpManagerTest, Initiate);
  FRIEND_TEST(L2tpManagerTest, PollInitiateConnection);
  FRIEND_TEST(L2tpManagerTest, PollNothingIfRunning);
  FRIEND_TEST(L2tpManagerTest, PollTimeoutWaitingForControl);
  FRIEND_TEST(L2tpManagerTest, PollTimeoutWaitingForUp);
  FRIEND_TEST(L2tpManagerTest, PollTransitionToUp);
  FRIEND_TEST(L2tpManagerTest, PollWaitIfNotUpYet);
  FRIEND_TEST(L2tpManagerTest, Start);
  FRIEND_TEST(L2tpManagerTest, Terminate);

  std::string FormatL2tpdConfiguration(const std::string& ppp_config_path);
  std::string FormatPppdConfiguration();
  bool Initiate();
  bool Terminate();

  // Has the L2TP connection been initiated yet.
  bool was_initiated_;
  // l2tp daemon stderr pipe file descriptor.
  int output_fd_;
  // Start time of the l2tp daemon.
  base::TimeTicks start_ticks_;
  // Remote host for L2TP connection.
  std::string remote_host_;
  // Last partial line read from output_fd_.
  std::string partial_output_line_;
  // Path to a file whose existence indicates the ppp device is up.
  FilePath ppp_interface_path_;
  // Path to l2tp daemon's control file.
  FilePath l2tpd_control_path_;
  // Running l2tp process.
  scoped_ptr<chromeos::Process> l2tpd_;
};

#endif  // _VPN_MANAGER_L2TP_MANAGER_H_
