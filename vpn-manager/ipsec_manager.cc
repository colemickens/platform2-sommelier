// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpn-manager/ipsec_manager.h"

#include <arpa/inet.h>  // for inet_ntop and inet_pton
#include <grp.h>
#include <netdb.h>  // for getaddrinfo
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/process.h"
#include "gflags/gflags.h"
#include "openssl/x509.h"

#pragma GCC diagnostic ignored "-Wstrict-aliasing"
// Windows RRAS requires modp1024 dh-group.  Strongswan's
// default is modp1536 which it does not support.
DEFINE_string(ike, "3des-sha1-modp1024", "ike proposals");
DEFINE_int32(ipsec_timeout, 30, "timeout for ipsec to be established");
DEFINE_string(leftprotoport, "17/1701", "client protocol/port");
DEFINE_bool(nat_traversal, true, "Enable NAT-T nat traversal");
DEFINE_bool(pfs, false, "pfs");
DEFINE_bool(rekey, false, "rekey");
DEFINE_string(rightprotoport, "17/1701", "server protocol/port");
DEFINE_string(type, "transport", "IPsec type (transport or tunnel)");
#pragma GCC diagnostic error "-Wstrict-aliasing"

const char kIpsecConnectionName[] = "ipsec_managed";
const char kIpsecGroupName[] = "ipsec";
const char kIpsecRunPath[] = "/var/run/ipsec";
const char kIpsecUpFile[] = "/var/run/ipsec/up";
const char kIpsecServiceName[] = "ipsec";
const char kStarterPidFile[] = "/var/run/starter.pid";
const mode_t kIpsecRunPathMode = (S_IRUSR | S_IWUSR | S_IXUSR |
                                  S_IRGRP | S_IWGRP | S_IXGRP);
const char kStatefulContainer[] = "/mnt/stateful_partition/etc";

// Give IPsec layer 2 seconds to shut down before killing it.
const int kTermTimeout = 2;

using ::chromeos::Process;
using ::chromeos::ProcessImpl;

IpsecManager::IpsecManager()
    : ServiceManager(kIpsecServiceName),
      force_local_address_(NULL),
      force_remote_address_(NULL),
      output_fd_(-1),
      ike_version_(0),
      ipsec_group_(0),
      stateful_container_(kStatefulContainer),
      ipsec_run_path_(kIpsecRunPath),
      ipsec_up_file_(kIpsecUpFile),
      starter_pid_file_(kStarterPidFile),
      starter_(new ProcessImpl) {
}

bool IpsecManager::Initialize(int ike_version,
                              const std::string& remote_host,
                              const std::string& psk_file,
                              const std::string& server_ca_file,
                              const std::string& server_id,
                              const std::string& client_cert_tpm_slot,
                              const std::string& client_cert_tpm_id,
                              const std::string& tpm_user_pin) {
  if (remote_host.empty()) {
    LOG(ERROR) << "Missing remote host to IPsec layer";
    return false;
  }
  remote_host_ = remote_host;

  if (psk_file.empty()) {
    if (server_ca_file.empty() && server_id.empty() &&
        client_cert_tpm_slot.empty() && client_cert_tpm_id.empty() &&
        tpm_user_pin.empty()) {
      LOG(ERROR) << "Must specify either PSK or certificates for IPsec layer";
      return false;
    }

    if (ike_version != 1) {
      LOG(ERROR) << "Only IKE version 1 is supported with certificates";
      return false;
    }

    // Must be a certificate based connection.
    FilePath server_ca_path(server_ca_file);
    if (!file_util::PathExists(server_ca_path)) {
      LOG(ERROR) << "Invalid server CA file for IPsec layer: "
                 << server_ca_file;
      return false;
    }
    if (!ReadCertificateSubject(server_ca_path, &server_ca_subject_)) {
      LOG(ERROR) << "Unable to read certificate subject from: "
                 << server_ca_file;
      return false;
    }
    server_ca_file_ = server_ca_file;

    if (server_id.empty()) {
      LOG(ERROR) << "Must specify an ID for the server";
      return false;
    }
    server_id_ = server_id;

    if (client_cert_tpm_slot.empty()) {
      LOG(ERROR) << "Must specify the slot containing the certificate";
      return false;
    }
    client_cert_tpm_slot_ = client_cert_tpm_slot;

    if (client_cert_tpm_id.empty()) {
      LOG(ERROR) << "Must specify the key ID for the certificate";
      return false;
    }
    client_cert_tpm_id_ = client_cert_tpm_id;

    if (tpm_user_pin.empty()) {
      LOG(ERROR) << "Must specify user PIN for TPM to use certificate";
      return false;
    }
    tpm_user_pin_ = tpm_user_pin;
  } else {
    if (!server_ca_file.empty() ||
        !server_id.empty() ||
        !client_cert_tpm_slot.empty() ||
        !client_cert_tpm_id.empty() ||
        !tpm_user_pin.empty()) {
      LOG(ERROR) << "Specified both PSK and certificates for IPsec layer";
      return false;
    }
    if (!file_util::PathExists(FilePath(psk_file))) {
      LOG(ERROR) << "Invalid PSK file for IPsec layer: " << psk_file;
      return false;
    }
    psk_file_ = psk_file;
  }

  if (ike_version != 1 && ike_version != 2) {
    LOG(ERROR) << "Unsupported IKE version" << ike_version;
    return false;
  }
  ike_version_ = ike_version;

  file_util::Delete(FilePath(kIpsecUpFile), false);

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

bool IpsecManager::ConvertSockAddrToIPString(
    const struct sockaddr& socket_address,
    std::string* output) {
  char str[INET6_ADDRSTRLEN] = { 0 };
  switch (socket_address.sa_family) {
    case AF_INET:
      if (!inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(
              &socket_address)->sin_addr, str, INET6_ADDRSTRLEN)) {
        LOG(ERROR) << "inet_ntop failed";
        return false;
      }
      break;
    case AF_INET6:
      if (!inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6*>(
              &socket_address)->sin6_addr, str, INET6_ADDRSTRLEN)) {
        LOG(ERROR) << "inet_ntop failed";
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Unknown address family";
      return false;
  }
  *output = str;
  return true;
}

bool IpsecManager::GetAddressesFromRemoteHost(
    const std::string& remote_host,
    std::string* remote_address_text,
    std::string* local_address_text) {
  static const char kService[] = "80";
  if (force_local_address_ != NULL) {
    *local_address_text = force_local_address_;
    *remote_address_text = force_remote_address_;
    return true;
  }
  struct addrinfo *remote_address;
  int s = getaddrinfo(remote_host.c_str(), kService, NULL,
                      &remote_address);
  if (s != 0) {
    LOG(ERROR) << "getaddrinfo failed: " << gai_strerror(s);
    return false;
  }
  if (!ConvertSockAddrToIPString(*remote_address->ai_addr,
                                 remote_address_text)) {
    return false;
  }
  int sock = HANDLE_EINTR(socket(AF_INET, SOCK_DGRAM, 0));
  if (sock < 0) {
    LOG(ERROR) << "Unable to create socket";
    return false;
  }
  if (HANDLE_EINTR(
          connect(sock, remote_address->ai_addr, sizeof(sockaddr))) != 0) {
    LOG(ERROR) << "Unable to connect";
    HANDLE_EINTR(close(sock));
    return false;
  }
  bool result = false;
  struct sockaddr local_address;
  socklen_t addr_len = sizeof(local_address);
  if (getsockname(sock, &local_address, &addr_len) != 0) {
    int saved_errno = errno;
    LOG(ERROR) << "getsockname failed on socket connecting to "
               << remote_address_text << ": " << saved_errno;
    goto error_label;
  }
  if (!ConvertSockAddrToIPString(local_address, local_address_text))
    goto error_label;
  LOG(INFO) << "Remote address " << remote_address_text << " has local address "
            << *local_address_text;
  result = true;

 error_label:
  HANDLE_EINTR(close(sock));
  freeaddrinfo(remote_address);
  return result;
}

bool IpsecManager::FormatSecrets(std::string* formatted) {
  std::string secret_mode;
  std::string secret;
  if (psk_file_.empty()) {
    secret_mode = StringPrintf("PIN %%smartcard%s:%s",
                               client_cert_tpm_slot_.c_str(),
                               client_cert_tpm_id_.c_str());
    secret = tpm_user_pin_;
  } else {
    secret_mode = "PSK";
    if (!file_util::ReadFileToString(FilePath(psk_file_), &secret)) {
      LOG(ERROR) << "Unable to read PSK from " << psk_file_;
      return false;
    }
    TrimWhitespaceASCII(secret, TRIM_TRAILING, &secret);
  }
  std::string local_address;
  std::string remote_address;
  if (!GetAddressesFromRemoteHost(remote_host_, &remote_address,
                                  &local_address)) {
    LOG(ERROR) << "Local IP address could not be determined for PSK mode";
    return false;
  }
  *formatted = StringPrintf("%s %s : %s \"%s\"\n", local_address.c_str(),
                            remote_address.c_str(), secret_mode.c_str(),
                            secret.c_str());
  return true;
}

void IpsecManager::KillCurrentlyRunning() {
  if (!file_util::PathExists(FilePath(starter_pid_file_)))
    return;
  starter_->ResetPidByFile(starter_pid_file_);
  if (Process::ProcessExists(starter_->pid()))
    starter_->Reset(0);
  else
    starter_->Release();
  file_util::Delete(FilePath(starter_pid_file_), false);
}

bool IpsecManager::StartStarter() {
  KillCurrentlyRunning();
  LOG(INFO) << "Starting starter";
  starter_->AddArg(IPSEC_STARTER);
  starter_->AddArg("--nofork");
  starter_->RedirectUsingPipe(STDERR_FILENO, false);
  if (!starter_->Start()) {
    LOG(ERROR) << "Starter did not start successfully";
    return false;
  }
  output_fd_ = starter_->GetPipe(STDERR_FILENO);
  pid_t starter_pid = starter_->pid();
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

std::string IpsecManager::FormatStarterConfigFile() {
  std::string config;
  config.append("config setup\n");
  if (ike_version_ == 1) {
    AppendBoolSetting(&config, "charonstart", false);
  } else {
    AppendBoolSetting(&config, "plutostart", false);
  }
  AppendBoolSetting(&config, "nat_traversal", FLAGS_nat_traversal);
  AppendStringSetting(&config, "pkcs11module", PKCS11_LIB);
  config.append("conn managed\n");
  AppendStringSetting(&config, "ike", FLAGS_ike);
  AppendStringSetting(&config, "keyexchange",
                      ike_version_ == 1 ? "ikev1" : "ikev2");
  if (!psk_file_.empty()) AppendStringSetting(&config, "authby", "psk");
  AppendBoolSetting(&config, "pfs", FLAGS_pfs);
  AppendBoolSetting(&config, "rekey", FLAGS_rekey);
  AppendStringSetting(&config, "left", "%defaultroute");
  if (!client_cert_tpm_slot_.empty()) {
    std::string smartcard = StringPrintf("%%smartcard%s:%s",
                                         client_cert_tpm_slot_.c_str(),
                                         client_cert_tpm_id_.c_str());
    AppendStringSetting(&config, "leftcert", smartcard);
  }
  AppendStringSetting(&config, "leftprotoport", FLAGS_leftprotoport);
  AppendStringSetting(&config, "leftupdown", IPSEC_UPDOWN);
  AppendStringSetting(&config, "right", remote_host_);
  if (!server_ca_subject_.empty()) {
    AppendStringSetting(&config, "rightca", server_ca_subject_);
  }
  if (!server_id_.empty()) AppendStringSetting(&config, "rightid", server_id_);
  AppendStringSetting(&config, "rightprotoport", FLAGS_rightprotoport);
  AppendStringSetting(&config, "type", FLAGS_type);
  AppendStringSetting(&config, "auto", "start");
  return config;
}

bool IpsecManager::SetIpsecGroup(const FilePath& file_path) {
  return chown(file_path.value().c_str(), getuid(), ipsec_group_) == 0;
}

bool IpsecManager::WriteConfigFiles() {
  // We need to keep secrets in /mnt/stateful_partition/etc for now
  // because pluto loses permissions to /home/chronos before it tries
  // reading secrets. Ditto for CA certs.
  // TODO(kmixter): write this via a fifo.
  FilePath secrets_path_ = FilePath(stateful_container_).
      Append("ipsec.secrets");
  file_util::Delete(secrets_path_, false);
  std::string formatted_secrets;
  if (!FormatSecrets(&formatted_secrets)) {
    LOG(ERROR) << "Unable to create secrets contents";
    return false;
  }
  if (!file_util::WriteFile(secrets_path_, formatted_secrets.c_str(),
                            formatted_secrets.length()) ||
      !SetIpsecGroup(secrets_path_)) {
    LOG(ERROR) << "Unable to write secrets file " << secrets_path_.value();
    return false;
  }
  FilePath starter_config_path = temp_path()->Append("ipsec.conf");
  std::string starter_config = FormatStarterConfigFile();
  if (!file_util::WriteFile(starter_config_path, starter_config.c_str(),
                            starter_config.size()) ||
      !SetIpsecGroup(starter_config_path)) {
    LOG(ERROR) << "Unable to write ipsec config files";
    return false;
  }
  FilePath config_symlink_path = FilePath(stateful_container_).
      Append("ipsec.conf");
  // Use unlink to remove the symlink directly since file_util::Delete
  // cannot delete dangling symlinks.
  unlink(config_symlink_path.value().c_str());
  if (file_util::PathExists(config_symlink_path)) {
    LOG(ERROR) << "Unable to remove existing file "
               << config_symlink_path.value();
    return false;
  }
  if (symlink(starter_config_path.value().c_str(),
              config_symlink_path.value().c_str()) < 0) {
    int saved_errno = errno;
    LOG(ERROR) << "Unable to symlink config file "
               << config_symlink_path.value() << " -> "
               << starter_config_path.value() << ": " << saved_errno;
    return false;
  }

  if (!server_ca_file_.empty()) {
    FilePath target_ca_path = FilePath(stateful_container_).
        Append("cacert.der");
    unlink(target_ca_path.value().c_str());
    if (file_util::PathExists(target_ca_path)) {
      LOG(ERROR) << "Unable to delete old CA cert";
      return false;
    }
    if (!file_util::CopyFile(FilePath(server_ca_file_), target_ca_path) ||
        !SetIpsecGroup(target_ca_path)) {
      LOG(ERROR) << "Unable to copy CA cert file to stateful partition";
      return false;
    }
  }

  return true;
}

bool IpsecManager::CreateIpsecRunDirectory() {
  if (!file_util::CreateDirectory(FilePath(ipsec_run_path_)) ||
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
      return false;
    }
    ipsec_group_ = group_result->gr_gid;
    DLOG(INFO) << "Using ipsec group " << ipsec_group_;
  }
  if (!WriteConfigFiles())
    return false;
  if (!CreateIpsecRunDirectory())
    return false;
  if (!StartStarter())
    return false;

  start_ticks_ = base::TimeTicks::Now();

  return true;
}

int IpsecManager::Poll() {
  if (is_running()) return -1;
  if (start_ticks_.is_null()) return -1;
  if (!file_util::PathExists(FilePath(ipsec_up_file_))) {
    if (base::TimeTicks::Now() - start_ticks_ >
        base::TimeDelta::FromSeconds(FLAGS_ipsec_timeout)) {
      LOG(ERROR) << "IPsec connection timed out";
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
  ServiceManager::WriteFdToSyslog(output_fd_, ipsec_prefix_,
                                  &partial_output_line_);
}

bool IpsecManager::IsChild(pid_t pid) {
  return pid == starter_->pid();
}

void IpsecManager::Stop() {
  if (starter_->pid() == 0) {
    return;
  }

  if (!starter_->Kill(SIGTERM, kTermTimeout)) {
    starter_->Kill(SIGKILL, 0);
    OnStopped(true);
    return;
  }
  OnStopped(false);
}
