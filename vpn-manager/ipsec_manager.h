// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VPN_MANAGER_IPSEC_MANAGER_H_
#define VPN_MANAGER_IPSEC_MANAGER_H_

#include <sys/socket.h>

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/time/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "vpn-manager/service_manager.h"

namespace vpn_manager {

class Daemon;

// Manages the ipsec daemon.  This manager orchestrates configuring and
// launching the strongswan starter process which in turn launches the
// charon daemon.
class IpsecManager : public ServiceManager {
 public:
  IpsecManager(const std::string& esp,
               const std::string& ike,
               int ipsec_timeout,
               const std::string& left_protoport,
               bool rekey,
               const std::string& right_protoport,
               const std::string& tunnel_group,
               const std::string& type,
               const base::FilePath& temp_path,
               const base::FilePath& persistent_path);
  ~IpsecManager() override = default;

  // Initialize the object to control IKE version |ike_version| daemon,
  // connecting to the given |remote_address|, with given paths to
  // pre-shared key file |psk_file|, server certificate authority file
  // |server_ca_file|, client key file |client_key_file|, and client
  // certificate file |client_cert_file|.
  bool Initialize(int ike_version,
                  const struct sockaddr& remote_address,
                  const std::string& psk_file,
                  const std::string& xauth_credentials_file,
                  const std::string& server_ca_file,
                  const std::string& server_id,
                  const std::string& client_cert_tpm_slot,
                  const std::string& client_cert_tpm_id,
                  const std::string& tpm_user_pin);

  bool Start() override;
  void Stop() override;
  int Poll() override;
  void ProcessOutput() override;
  bool IsChild(pid_t pid) override;
  void OnSyslogOutput(const std::string& prefix,
                      const std::string& line) override;

  // Returns the stderr output file descriptor of our child process.
  int output_fd() const { return output_fd_; }

 private:
  friend class IpsecManagerTest;
  FRIEND_TEST(IpsecManagerTest, CreateIpsecRunDirectory);
  FRIEND_TEST(IpsecManagerTest, PollWaitIfNotUpYet);
  FRIEND_TEST(IpsecManagerTest, PollTimeoutWaiting);
  FRIEND_TEST(IpsecManagerTest, PollTransitionToUp);
  FRIEND_TEST(IpsecManagerTest, PollNothingIfRunning);
  FRIEND_TEST(IpsecManagerTest, FormatSecretsNoSlot);
  FRIEND_TEST(IpsecManagerTest, FormatSecretsNonZeroSlot);
  FRIEND_TEST(IpsecManagerTest, FormatSecretsXauthCredentials);
  FRIEND_TEST(IpsecManagerTest, FormatStrongswanConfigFile);
  FRIEND_TEST(IpsecManagerTest, StartStarter);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, FormatSecrets);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, FormatStarterConfigFile);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, GetAddressesFromRemoteHost);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, Start);
  FRIEND_TEST(IpsecManagerTestIkeV1Psk, WriteConfigFiles);
  FRIEND_TEST(IpsecManagerTestIkeV1Certs, FormatSecrets);
  FRIEND_TEST(IpsecManagerTestIkeV1Certs, FormatStarterConfigFile);
  FRIEND_TEST(IpsecManagerTestIkeV1Certs, WriteConfigFiles);
  FRIEND_TEST(IpsecManagerTestBogusCerts, FormatStarterConfigFile);

  bool ReadCertificateSubject(const base::FilePath& filepath,
                              std::string* output);
  bool FormatIpsecSecret(std::string* formatted);
  bool FormatXauthSecret(std::string* formatted);
  bool FormatSecrets(std::string* formatted);
  void KillCurrentlyRunning();
  bool WriteConfigFile(const std::string& output_name,
                       const std::string& contents);
  bool MakeSymbolicLink(const std::string& output_name,
                        const base::FilePath& source_path);
  bool WriteConfigFiles();
  bool CreateIpsecRunDirectory();
  std::string FormatStrongswanConfigFile();
  std::string FormatStarterConfigFile();
  bool StartStarter();
  bool SetIpsecGroup(const base::FilePath& file_path);

  // Command line flags.
  std::string esp_;
  std::string ike_;
  int ipsec_timeout_;
  std::string left_protoport_;
  bool rekey_;
  std::string right_protoport_;
  std::string tunnel_group_;
  std::string type_;

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
  // Writeable directory that the root filesystem has symbolic links to for
  // all VPN configuration files we care about.
  base::FilePath persistent_path_;
  // File whose existence signifies ipsec is now up.
  std::string ipsec_up_file_;
  // String with which to prefix ipsec output log lines.
  std::string ipsec_prefix_;
  // File containing starter process's process id.
  std::string starter_pid_file_;
  // File containing charon process's process id.
  std::string charon_pid_file_;
  // Remote address of IPsec connection.
  struct sockaddr remote_address_;
  // Remote address of IPsec connection (as a string).
  std::string remote_address_text_;
  // File containing the IPsec pre-shared key.
  std::string psk_file_;
  // File containing the server certificate authority in DER format.
  std::string server_ca_file_;
  // Subject of the server certificate authority certificate.
  std::string server_ca_subject_;
  // ID that server must send to identify itself.
  std::string server_id_;
  // PKCS#11 slot containing the client certificate.
  std::string client_cert_slot_;
  // PKCS#11 object id of the client certificate.
  std::string client_cert_id_;
  // PKCS#11 user PIN needed to get the client certificate.
  std::string user_pin_;
  // Last partial line read from output_fd_.
  std::string partial_output_line_;
  // File containing the XAUTH username and password.
  std::string xauth_credentials_file_;
  // File containing the XAUTH username gained from FormatSecrets().
  std::string xauth_identity_;
  // Time when ipsec was started.
  base::TimeTicks start_ticks_;
  // IPsec starter daemon.
  std::unique_ptr<Daemon> starter_daemon_;
  // IPsec charon daemon.
  std::unique_ptr<Daemon> charon_daemon_;
};

}  // namespace vpn_manager

#endif  // VPN_MANAGER_IPSEC_MANAGER_H_
