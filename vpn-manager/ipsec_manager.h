// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _VPN_MANAGER_IPSEC_MANAGER_H_
#define _VPN_MANAGER_IPSEC_MANAGER_H_

#include <string>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "gtest/gtest_prod.h"  // for FRIEND_TEST
#include "vpn-manager/service_manager.h"

class FilePath;
namespace chromeos {
class Process;
}

// Manages the ipsec daemon.  This manager orchestrates configuring and
// launching the strongswan starter process which in turn launches the
// appropriate IKE v1 (pluto) or IKE v2 (charon) daemon.
class IpsecManager : public ServiceManager {
 public:
  IpsecManager();

  // Initialize the object to control IKE version |ike_version| daemon,
  // connecting to the give |remote_hostname|, with given paths to
  // pre-shared key file |psk_file|, server certificate authority file
  // |server_ca_file|, client key file |client_key_file|, and client
  // certificate file |client_cert_file|.
  bool Initialize(int ike_version,
                  const std::string& remote_hostname,
                  const std::string& psk_file,
                  const std::string& server_ca_file,
                  const std::string& client_key_file,
                  const std::string& client_cert_file);

  virtual bool Start();
  virtual void Stop();
  virtual int Poll();
  virtual void ProcessOutput();
  virtual bool IsChild(pid_t pid);

  // Returns the stderr output file descriptor of our child process.
  int output_fd() const { return output_fd_; }

 private:
  friend class IpsecManagerTest;
  FRIEND_TEST(IpsecManagerTest, CreateIpsecRunDirectory);
  FRIEND_TEST(IpsecManagerTest, PollWaitIfNotUpYet);
  FRIEND_TEST(IpsecManagerTest, PollTimeoutWaiting);
  FRIEND_TEST(IpsecManagerTest, PollTransitionToUp);
  FRIEND_TEST(IpsecManagerTest, PollNothingIfRunning);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, FormatPsk);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, FormatStarterConfigFile);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, GetAddressesFromRemoteHost);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, Start);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, StartStarterAlreadyRunning);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, StartStarterNotYetRunning);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, WriteConfigFiles);

  bool ConvertSockAddrToIPString(const struct sockaddr& socket_address,
                                 std::string* output);
  bool GetAddressesFromRemoteHost(const std::string& remote_hostname,
                                  std::string* remote_address_text,
                                  std::string* local_address_text);
  bool FormatPsk(const FilePath& input_file, std::string* formatted);
  void KillCurrentlyRunning();
  bool WriteConfigFiles();
  bool CreateIpsecRunDirectory();
  std::string FormatStarterConfigFile();
  bool StartStarter();
  bool SetIpsecGroup(const FilePath& file_path);

  // for testing, always return these values from
  // GetAddressesFromRemoteHostname.
  const char* force_local_address_;
  const char* force_remote_address_;
  // ipsec daemon stderr pipe file descriptor.
  int output_fd_;
  // IKE key exchange version to use.
  int ike_version_;
  // Group id of the "ipsec" group on this machine.  This is the group
  // that we expect the underlying IKE daemons to run as.
  gid_t ipsec_group_;
  // Writeable directory to which we can write configuration files for
  // ipsec daemons.
  std::string stateful_container_;
  // Directory containing run files for ipsec that we create with
  // permissions locked to ipsec group.
  std::string ipsec_run_path_;
  // File whose existence signifies ipsec is now up.
  std::string ipsec_up_file_;
  // String with which to prefix ipsec output log lines.
  std::string ipsec_prefix_;
  // File containing starter process's process id.
  std::string starter_pid_file_;
  // Remote hostname of IPsec connection.
  std::string remote_host_;
  // File containing the IPsec pre-shared key.
  std::string psk_file_;
  // File containing the server certificate authority.
  std::string server_ca_file_;
  // File containing the client private key.
  std::string client_key_file_;
  // File containing the client certificate.
  std::string client_cert_file_;
  // Last partial line read from output_fd_.
  std::string partial_output_line_;
  // Time when ipsec was started.
  base::TimeTicks start_ticks_;
  // IPsec starter process.
  scoped_ptr<chromeos::Process> starter_;
};

#endif  // _VPN_MANAGER_IPSEC_MANAGER_H_
