// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpn-manager/ipsec_manager.h"

#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <chromeos/process.h>
#include <gflags/gflags.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "vpn-manager/daemon.h"

#pragma GCC diagnostic ignored "-Wstrict-aliasing"
// Cisco ASA L2TP/IPsec setup instructions indicate using md5 for
// authentication for the IPsec SA.  Default StrongS/WAN setup is
// to only propose SHA1.
DEFINE_string(esp, "aes128-sha1,3des-sha1,aes128-md5,3des-md5",
              "esp proposals");
// Windows RRAS requires modp1024 dh-group.  Strongswan's
// default is modp1536 which it does not support.
DEFINE_string(ike, "3des-sha1-modp1024", "ike proposals");
DEFINE_int32(ipsec_timeout, 30, "timeout for ipsec to be established");
DEFINE_string(leftprotoport, "17/1701", "client protocol/port");
DEFINE_bool(nat_traversal, true, "Enable NAT-T nat traversal");
DEFINE_bool(pfs, false, "pfs");
DEFINE_bool(rekey, true, "rekey");
DEFINE_string(rightprotoport, "17/1701", "server protocol/port");
DEFINE_string(tunnel_group, "", "Cisco Tunnel Group Name");
DEFINE_string(type, "transport", "IPsec type (transport or tunnel)");
#pragma GCC diagnostic error "-Wstrict-aliasing"

using ::base::FilePath;
using ::base::StringPrintf;
using ::chromeos::Process;
using ::chromeos::ProcessImpl;

namespace vpn_manager {

namespace {

const char kDefaultCertSlot[] = "0";
const char kIpsecCaCertsName[] = "cacert.der";
const char kIpsecStarterConfName[] = "ipsec.conf";
const char kIpsecSecretsName[] = "ipsec.secrets";
const char kIpsecGroupName[] = "ipsec";
const char kIpsecRunPath[] = "/var/run/ipsec";
const char kIpsecUpFile[] = "/var/run/ipsec/up";
const char kIpsecServiceName[] = "ipsec";
const char kStarterPidFile[] = "/var/run/starter.pid";
const char kStrongswanConfName[] = "strongswan.conf";
const char kCharonPidFile[] = "/var/run/charon.pid";
const mode_t kIpsecRunPathMode = S_IRWXU | S_IRWXG;
const mode_t kPersistentPathMode =
    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
const char kIpsecAuthenticationFailurePattern[] =
    "*discarding duplicate packet*STATE_MAIN_I3*";
const char kSmartcardModuleName[] = "crypto_module";

}  // namespace

IpsecManager::IpsecManager()
    : ServiceManager(kIpsecServiceName),
      force_local_address_(NULL),
      output_fd_(-1),
      ike_version_(0),
      ipsec_group_(0),
      persistent_path_(GetRootPersistentPath()),
      ipsec_run_path_(kIpsecRunPath),
      ipsec_up_file_(kIpsecUpFile),
      starter_daemon_(new Daemon(kStarterPidFile)),
      charon_daemon_(new Daemon(kCharonPidFile)) {
}

bool IpsecManager::Initialize(int ike_version,
                              const struct sockaddr& remote_address,
                              const std::string& psk_file,
                              const std::string& xauth_credentials_file,
                              const std::string& server_ca_file,
                              const std::string& server_id,
                              const std::string& client_cert_slot,
                              const std::string& client_cert_id,
                              const std::string& user_pin) {
  if (!ConvertSockAddrToIPString(remote_address, &remote_address_text_)) {
    LOG(ERROR) << "Unable to convert sockaddr to name for remote host";
    RegisterError(kServiceErrorInternal);
    return false;
  }
  remote_address_ = remote_address;

  if (psk_file.empty()) {
    if (server_ca_file.empty() && server_id.empty() &&
        client_cert_id.empty() &&
        user_pin.empty()) {
      LOG(ERROR) << "Must specify either PSK or certificates for IPsec layer";
      RegisterError(kServiceErrorInvalidArgument);
      return false;
    }

    if (ike_version != 1) {
      LOG(ERROR) << "Only IKE version 1 is supported with certificates";
      RegisterError(kServiceErrorInvalidArgument);
      return false;
    }

    // Must be a certificate based connection.
    FilePath server_ca_path(server_ca_file);
    if (!base::PathExists(server_ca_path)) {
      LOG(ERROR) << "Invalid server CA file for IPsec layer: "
                 << server_ca_file;
      RegisterError(kServiceErrorInvalidArgument);
      return false;
    }
    if (!ReadCertificateSubject(server_ca_path, &server_ca_subject_)) {
      LOG(ERROR) << "Unable to read certificate subject from: "
                 << server_ca_file;
      RegisterError(kServiceErrorInvalidArgument);
      return false;
    }
    server_ca_file_ = server_ca_file;
    server_id_ = server_id;

    if (client_cert_slot.empty()) {
      client_cert_slot_ = kDefaultCertSlot;
    } else {
      client_cert_slot_ = client_cert_slot;
    }

    if (client_cert_id.empty()) {
      LOG(ERROR) << "Must specify the PKCS#11 ID for the certificate";
      RegisterError(kServiceErrorInvalidArgument);
      return false;
    }
    client_cert_id_ = client_cert_id;

    if (user_pin.empty()) {
      LOG(ERROR) << "Must specify the PKCS#11 user PIN for the certificate";
      RegisterError(kServiceErrorInvalidArgument);
      return false;
    }
    user_pin_ = user_pin;
  } else {
    if (!server_ca_file.empty() ||
        !server_id.empty() ||
        !client_cert_id.empty()) {
      LOG(WARNING) << "Specified both certificates and PSK to IPsec layer";
    }
    if (!base::PathExists(FilePath(psk_file))) {
      LOG(ERROR) << "Invalid PSK file for IPsec layer: " << psk_file;
      RegisterError(kServiceErrorInvalidArgument);
      return false;
    }
    psk_file_ = psk_file;
  }

  if (!xauth_credentials_file.empty()) {
    if (!base::PathExists(FilePath(xauth_credentials_file))) {
      LOG(ERROR) << "Invalid xauth credentials file: "
                 << xauth_credentials_file;
      RegisterError(kServiceErrorInvalidArgument);
      return false;
    }
    xauth_credentials_file_ = xauth_credentials_file;
  }

  if (ike_version != 1 && ike_version != 2) {
    LOG(ERROR) << "Unsupported IKE version" << ike_version;
    RegisterError(kServiceErrorInvalidArgument);
    return false;
  }
  ike_version_ = ike_version;

  base::DeleteFile(FilePath(kIpsecUpFile), false);

  return true;
}

bool IpsecManager::ReadCertificateSubject(const FilePath& filepath,
                                          std::string* output) {
  FILE* fp = fopen(filepath.value().c_str(), "rb");
  if (!fp) {
    LOG(ERROR) << "Unable to read certificate";
    return false;
  }
  X509* cert = d2i_X509_fp(fp, NULL);
  if (cert == NULL) {
    fseek(fp, 0, SEEK_SET);
    cert = PEM_read_X509(fp, NULL, NULL, NULL);
  }
  fclose(fp);
  if (!cert) {
    LOG(ERROR) << "Error parsing certificate";
    return false;
  }
  BIO* bio = BIO_new(BIO_s_mem());
  if (!X509_NAME_print_ex(bio, X509_get_subject_name(cert), 0,
                          XN_FLAG_SEP_CPLUS_SPC)) {
    LOG(ERROR) << "Could not print certificate name";
    BIO_free(bio);
    X509_free(cert);
    return false;
  }
  char* name_ptr;
  int length = BIO_get_mem_data(bio, &name_ptr);
  output->assign(name_ptr, length);
  BIO_free(bio);
  X509_free(cert);
  return true;
}

bool IpsecManager::FormatIpsecSecret(std::string* formatted) {
  std::string secret_mode;
  std::string secret;
  if (psk_file_.empty()) {
    secret_mode = StringPrintf("PIN %%smartcard%s@%s:%s",
                               client_cert_slot_.c_str(),
                               kSmartcardModuleName,
                               client_cert_id_.c_str());
    secret = user_pin_;
  } else {
    secret_mode = "PSK";
    if (!base::ReadFileToString(FilePath(psk_file_), &secret)) {
      LOG(ERROR) << "Unable to read PSK from " << psk_file_;
      return false;
    }
    base::TrimWhitespaceASCII(secret, base::TRIM_TRAILING, &secret);
  }
  std::string local_address_text;
  if (force_local_address_ != NULL) {
    local_address_text = force_local_address_;
  } else {
    struct sockaddr local_address;
    if (!GetLocalAddressFromRemote(remote_address_, &local_address)) {
      LOG(ERROR) << "Local IP address could not be determined for PSK mode";
      return false;
    }
    if (!ConvertSockAddrToIPString(local_address, &local_address_text)) {
      LOG(ERROR) << "Unable to convert local address to string";
      return false;
    }
  }
  *formatted = StringPrintf("%s %s : %s \"%s\"\n", local_address_text.c_str(),
                            remote_address_text_.c_str(), secret_mode.c_str(),
                            secret.c_str());
  return true;
}

bool IpsecManager::FormatXauthSecret(std::string* formatted) {
  if (xauth_credentials_file_.empty()) {
    xauth_identity_ = "";
    return true;
  }

  std::string xauth_contents;
  if (!base::ReadFileToString(FilePath(xauth_credentials_file_),
                              &xauth_contents)) {
    LOG(ERROR) << "Unable to read XAUTH credentials from "
               << xauth_credentials_file_;
    return false;
  }
  std::vector<std::string> xauth_parts;
  base::SplitString(xauth_contents, '\n', &xauth_parts);
  if (xauth_parts.size() < 2) {
    LOG(ERROR) << "Unable to parse XAUTH credentials from "
               << xauth_credentials_file_;
    return false;
  }

  // Save this identity for use in the ipsec starter file.
  xauth_identity_ = xauth_parts[0];
  std::string xauth_password = xauth_parts[1];
  *formatted = StringPrintf(
      "%s : XAUTH \"%s\"\n", xauth_identity_.c_str(), xauth_password.c_str());
  return true;
}

bool IpsecManager::FormatSecrets(std::string* formatted) {
  std::string ipsec_secret, xauth_secret;
  if (!FormatIpsecSecret(&ipsec_secret) || !FormatXauthSecret(&xauth_secret)) {
    return false;
  }

  *formatted = ipsec_secret + xauth_secret;
  return true;
}

void IpsecManager::KillCurrentlyRunning() {
  starter_daemon_->FindProcess();
  charon_daemon_->FindProcess();
  starter_daemon_->ClearProcess();
  charon_daemon_->ClearProcess();
}

bool IpsecManager::StartStarter() {
  KillCurrentlyRunning();
  LOG(INFO) << "Starting starter";
  Process* starter = starter_daemon_->CreateProcess();
  starter->AddArg(IPSEC_STARTER);
  starter->AddArg("--nofork");
  starter->RedirectUsingPipe(STDERR_FILENO, false);
  if (!starter->Start()) {
    LOG(ERROR) << "Starter did not start successfully";
    return false;
  }
  output_fd_ = starter->GetPipe(STDERR_FILENO);
  pid_t starter_pid = starter->pid();
  LOG(INFO) << "Starter started as pid " << starter_pid;
  ipsec_prefix_ = StringPrintf("ipsec[%d]: ", starter_pid);
  return true;
}

inline void AppendBoolSetting(std::string* config, const char* key,
                              bool value) {
  config->append(StringPrintf("\t%s=%s\n", key, value ? "yes" : "no"));
}

inline void AppendStringSetting(std::string* config, const char* key,
                                const std::string& value) {
  config->append(StringPrintf("\t%s=\"%s\"\n", key, value.c_str()));
}

inline void AppendIntSetting(std::string* config, const char* key,
                             int value) {
  config->append(StringPrintf("\t%s=%d\n", key, value));
}

std::string IpsecManager::FormatStrongswanConfigFile() {
  std::string config;
  config.append("libstrongswan {\n");
  config.append("  plugins {\n");
  config.append("    pkcs11 {\n");
  config.append("      modules {\n");
  config.append(StringPrintf("        %s {\n", kSmartcardModuleName));
  config.append(StringPrintf("          path = %s\n", PKCS11_LIB));
  config.append("        }\n");
  config.append("      }\n");
  config.append("    }\n");
  config.append("  }\n");
  config.append("}\n");
  config.append("charon {\n");
  config.append("  accept_unencrypted_mainmode_messages = yes\n");
  config.append("  ignore_routing_tables = 0\n");
  config.append("  install_routes = no\n");
  config.append("  routing_table = 0\n");
  config.append("}\n");
  return config;
}

std::string IpsecManager::FormatStarterConfigFile() {
  std::string config;
  config.append("config setup\n");
  if (debug()) {
    AppendStringSetting(&config, "charondebug", "dmn 2, mgr 2, ike 2, net 2");
  }

  config.append("conn managed\n");
  AppendStringSetting(&config, "ike", FLAGS_ike);
  AppendStringSetting(&config, "esp", FLAGS_esp);
  AppendStringSetting(&config, "keyexchange",
                      ike_version_ == 1 ? "ikev1" : "ikev2");
  if (!psk_file_.empty()) {
    if (!xauth_identity_.empty()) {
      AppendStringSetting(&config, "authby", "xauthpsk");
      AppendStringSetting(&config, "xauth", "client");
      AppendStringSetting(&config, "xauth_identity", xauth_identity_);
    } else {
      AppendStringSetting(&config, "authby", "psk");
    }
  }
  AppendBoolSetting(&config, "rekey", FLAGS_rekey);
  AppendStringSetting(&config, "left", "%defaultroute");
  if (!client_cert_slot_.empty()) {
    std::string smartcard = StringPrintf("%%smartcard%s@%s:%s",
                                         client_cert_slot_.c_str(),
                                         kSmartcardModuleName,
                                         client_cert_id_.c_str());
    AppendStringSetting(&config, "leftcert", smartcard);
  }
  std::string tunnel_group = FLAGS_tunnel_group;
  if (!tunnel_group.empty()) {
    AppendStringSetting(&config, "aggressive", "yes");
    std::string hex_tunnel_id =
      base::HexEncode(tunnel_group.c_str(), tunnel_group.length());
    std::string left_id = StringPrintf("@#%s", hex_tunnel_id.c_str());
    AppendStringSetting(&config, "leftid", left_id);
  }
  AppendStringSetting(&config, "leftprotoport", FLAGS_leftprotoport);
  AppendStringSetting(&config, "leftupdown", IPSEC_UPDOWN);
  AppendStringSetting(&config, "right", remote_address_text_);
  if (!server_ca_subject_.empty()) {
    AppendStringSetting(&config, "rightca", server_ca_subject_);
  }
  if (server_id_.empty()) {
    AppendStringSetting(&config, "rightid", "%any");
  } else {
    AppendStringSetting(&config, "rightid", server_id_);
  }
  AppendStringSetting(&config, "rightprotoport", FLAGS_rightprotoport);
  AppendStringSetting(&config, "type", FLAGS_type);
  AppendStringSetting(&config, "auto", "start");
  return config;
}

bool IpsecManager::SetIpsecGroup(const FilePath& file_path) {
  return chown(file_path.value().c_str(), getuid(), ipsec_group_) == 0;
}

bool IpsecManager::WriteConfigFile(const std::string& output_name,
                                   const std::string& contents) {
  FilePath temp_file = temp_path()->Append(output_name);
  base::DeleteFile(temp_file, false);
  if (base::PathExists(temp_file)) {
    LOG(ERROR) << "Unable to remove existing file "
               << temp_file.value();
    return false;
  }
  if (!base::WriteFile(temp_file, contents.c_str(), contents.length()) ||
      !SetIpsecGroup(temp_file)) {
    LOG(ERROR) << "Unable to write " << output_name
               << " file " << temp_file.value();
    return false;
  }

  return MakeSymbolicLink(output_name, temp_file);
}

bool IpsecManager::MakeSymbolicLink(const std::string& output_name,
                                    const FilePath& source_path) {
  FilePath symlink_path = persistent_path_.Append(output_name);
  // Use unlink to remove the symlink directly since base::DeleteFile
  // cannot delete dangling symlinks.
  unlink(symlink_path.value().c_str());
  if (base::PathExists(symlink_path)) {
    LOG(ERROR) << "Unable to remove existing file "
               << symlink_path.value();
    return false;
  }
  if (symlink(source_path.value().c_str(), symlink_path.value().c_str()) < 0) {
    PLOG(ERROR) << "Unable to symlink config file "
                << symlink_path.value() << " -> "
                << source_path.value();
    return false;
  }
  return true;
}

bool IpsecManager::WriteConfigFiles() {
  // The strongSwan binaries have hard-coded paths to /etc, which on a
  // ChromeOS image are symlinks to a fixed place |persistent_path_|.
  // We create the configuration files in /var/run and link from the
  // |persistent_path_| to these newly created files.
  if (!base::CreateDirectory(persistent_path_)) {
    LOG(ERROR) << "Unable to create container directory "
               << persistent_path_.value();
    return false;
  }

  // Make sure the base path is accessible for read by non-root users.
  // This will allow items like CA certificates to be visible by the
  // l2tpipsec process even after it has dropped privileges.
  if (chmod(temp_base_path(), kPersistentPathMode) != 0) {
    PLOG(ERROR) << "Unable to change permissions of base path directory "
                << temp_base_path();
    return false;
  }

  if (chmod(persistent_path_.value().c_str(), kPersistentPathMode) != 0) {
    PLOG(ERROR) << "Unable to change permissions of container directory "
                << persistent_path_.value();
    return false;
  }

  std::string formatted_secrets;
  if (!FormatSecrets(&formatted_secrets)) {
    LOG(ERROR) << "Unable to create secrets contents";
    return false;
  }

  if (!WriteConfigFile(kIpsecSecretsName, formatted_secrets)) {
    return false;
  }

  if (!WriteConfigFile(kStrongswanConfName, FormatStrongswanConfigFile())) {
    return false;
  }
  if (!WriteConfigFile(kIpsecStarterConfName, FormatStarterConfigFile())) {
    return false;
  }

  if (!server_ca_file_.empty()) {
    // We have a contract with shill that certificate files it
    // creates will be readable by the ipsec client.  As such we do
    // not need to copy this file, and can link to it directly.
    return MakeSymbolicLink(kIpsecCaCertsName, FilePath(server_ca_file_));
  }

  return true;
}

bool IpsecManager::CreateIpsecRunDirectory() {
  if (!base::CreateDirectory(FilePath(ipsec_run_path_)) ||
      !SetIpsecGroup(FilePath(ipsec_run_path_)) ||
      chmod(ipsec_run_path_.c_str(), kIpsecRunPathMode) != 0) {
    LOG(ERROR) << "Unable to create " << ipsec_run_path_;
    return false;
  }
  return true;
}

bool IpsecManager::Start() {
  if (!ipsec_group_) {
    struct group group_buffer;
    struct group* group_result = NULL;
    char buffer[256];
    if (getgrnam_r(kIpsecGroupName, &group_buffer, buffer,
                   sizeof(buffer), &group_result) != 0 || !group_result) {
      LOG(ERROR) << "Cannot find group id for " << kIpsecGroupName;
      RegisterError(kServiceErrorInternal);
      return false;
    }
    ipsec_group_ = group_result->gr_gid;
    DLOG(INFO) << "Using ipsec group " << ipsec_group_;
  }
  if (!WriteConfigFiles() ||
      !CreateIpsecRunDirectory() ||
      !StartStarter()) {
    RegisterError(kServiceErrorInternal);
    return false;
  }

  start_ticks_ = base::TimeTicks::Now();

  return true;
}

int IpsecManager::Poll() {
  if (is_running()) return -1;
  if (start_ticks_.is_null()) return -1;
  if (!base::PathExists(FilePath(ipsec_up_file_))) {
    if (base::TimeTicks::Now() - start_ticks_ >
        base::TimeDelta::FromSeconds(FLAGS_ipsec_timeout)) {
      LOG(ERROR) << "IPsec connection timed out";
      RegisterError(kServiceErrorIpsecConnectionFailed);
      OnStopped(false);
      // Poll in 1 second in order to check exit conditions.
    }
    return 1000;
  }

  // This indicates that the connection came up successfully.
  LOG(INFO) << "IPsec connection now up";
  OnStarted();
  return -1;
}

void IpsecManager::ProcessOutput() {
  WriteFdToSyslog(output_fd_, ipsec_prefix_, &partial_output_line_);
}

bool IpsecManager::IsChild(pid_t pid) {
  return pid == starter_daemon_->GetPid();
}

void IpsecManager::Stop() {
  // If the process started in StartStarter() is no longer running, see if
  // there is a pid file associated to a different running instance of the
  // starter.
  if (!starter_daemon_->IsRunning())
    starter_daemon_->FindProcess();
  charon_daemon_->FindProcess();

  bool unclean_termination = !starter_daemon_->Terminate();
  charon_daemon_->Terminate();
  OnStopped(unclean_termination);
}

void IpsecManager::OnSyslogOutput(const std::string& prefix,
                                  const std::string& line) {
  if (MatchPattern(line, kIpsecAuthenticationFailurePattern)) {
    if (psk_file_.empty()) {
      LOG(ERROR) << "IPsec certificate authentication failed";
      RegisterError(kServiceErrorIpsecCertificateAuthenticationFailed);
    } else {
      LOG(ERROR) << "IPsec pre-shared key authentication failed";
      RegisterError(kServiceErrorIpsecPresharedKeyAuthenticationFailed);
    }
  }
}

}  // namespace vpn_manager
