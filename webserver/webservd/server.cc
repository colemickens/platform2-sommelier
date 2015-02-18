// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webserver/webservd/server.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <limits>

#include <base/rand_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/async_event_sequencer.h>

#include "webserver/webservd/dbus_protocol_handler.h"
#include "webserver/webservd/protocol_handler.h"
#include "webserver/webservd/utils.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;

namespace {

void OnFirewallSuccess(const std::string& itf_name,
                       uint16_t port,
                       bool allowed) {
  if (allowed) {
    LOG(INFO) << "Successfully opened up port " << port << " on interface "
              << itf_name;
  } else {
    LOG(ERROR) << "Failed to open up port " << port << ", interface: "
               << itf_name;
  }
}

void IgnoreFirewallDBusMethodError(chromeos::Error* error) {
}

}  // anonymous namespace

namespace webservd {

Server::Server(ExportedObjectManager* object_manager, const Config& config)
    : dbus_object_{new DBusObject{
          object_manager, object_manager->GetBus(),
          org::chromium::WebServer::ServerAdaptor::GetObjectPath()}},
      config_{config} {
  dbus_adaptor_.SetDefaultHttp(dbus::ObjectPath{"/"});
  dbus_adaptor_.SetDefaultHttps(dbus::ObjectPath{"/"});

  int fds[2];
  PCHECK(pipe(fds) == 0) << "Failed to create firewall lifeline pipe";
  lifeline_read_fd_ = fds[0];
  lifeline_write_fd_ = fds[1];
}

Server::~Server() {
  close(lifeline_read_fd_);
  close(lifeline_write_fd_);
}

void Server::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());

  InitTlsData();

  for (auto& pair : config_.protocol_handlers)
    CreateProtocolHandler(pair.first, &pair.second);

  permission_broker_object_manager_.reset(
      new org::chromium::PermissionBroker::ObjectManagerProxy{
          dbus_object_->GetBus()});
  permission_broker_object_manager_->SetPermissionBrokerAddedCallback(
      base::Bind(&Server::OnPermissionBrokerOnline,
                 weak_ptr_factory_.GetWeakPtr()));

  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Failed exporting Server.", true));

  for (const auto& pair : protocol_handler_map_) {
    pair.second->RegisterAsync(
        sequencer->GetHandler("Failed exporting ProtocolHandler.", false));
  }
  sequencer->OnAllTasksCompletedCall({completion_callback});
}

void Server::OnPermissionBrokerOnline(
    org::chromium::PermissionBrokerProxy* proxy) {
  LOG(INFO) << "Permission broker is on-line. "
            << "Opening firewall for protocol handlers";
  dbus::FileDescriptor dbus_fd{lifeline_read_fd_};
  dbus_fd.CheckValidity();
  for (auto& pair : config_.protocol_handlers) {
    VLOG(1) << "Firewall request: Protocol Handler = " << pair.first
            << ", Port = " << pair.second.port << ", Interface = "
            << pair.second.interface_name;
    proxy->RequestTcpPortAccessAsync(
        pair.second.port,
        pair.second.interface_name,
        dbus_fd,
        base::Bind(&OnFirewallSuccess, pair.second.interface_name,
                   pair.second.port),
        base::Bind(&IgnoreFirewallDBusMethodError));
  }
}

std::string Server::Ping() {
  return "Web Server is running";
}

void Server::ProtocolHandlerStarted(ProtocolHandler* handler) {
  CHECK(protocol_handler_map_.find(handler) == protocol_handler_map_.end())
      << "Protocol handler already registered";
  std::string dbus_id;
  if (handler->GetID() == ProtocolHandler::kHttp ||
      handler->GetID() == ProtocolHandler::kHttps)
    dbus_id = handler->GetID();
  else
    dbus_id = std::to_string(++last_protocol_handler_index_);

  std::string path = base::StringPrintf("/org/chromium/WebServer/Servers/%s",
                                        dbus_id.c_str());
  dbus::ObjectPath object_path{path};
  std::unique_ptr<DBusProtocolHandler> dbus_protocol_handler{
      new DBusProtocolHandler{dbus_object_->GetObjectManager().get(),
                              object_path,
                              handler,
                              this}
  };
  protocol_handler_map_.emplace(handler, std::move(dbus_protocol_handler));
  if (handler->GetID() == ProtocolHandler::kHttp)
    dbus_adaptor_.SetDefaultHttps(object_path);
  else if (handler->GetID() == ProtocolHandler::kHttps)
    dbus_adaptor_.SetDefaultHttp(object_path);
}

void Server::ProtocolHandlerStopped(ProtocolHandler* handler) {
  CHECK_EQ(1u, protocol_handler_map_.erase(handler))
      << "Unknown protocol handler";
}

void Server::CreateProtocolHandler(
    const std::string& id, Config::ProtocolHandler* handler_config) {
  std::unique_ptr<ProtocolHandler> protocol_handler{
      new ProtocolHandler{id, this}};

  if (protocol_handler->Start(handler_config))
    protocol_handlers_.emplace(id, std::move(protocol_handler));
}

void Server::InitTlsData() {
  if (!TLS_certificate_.empty())
    return;  // Already initialized.

  // TODO(avakulenko): verify these constants and provide sensible values
  // for the long-term. See brbug.com/227
  const int kKeyLengthBits = 1024;
  const base::TimeDelta kCertExpiration = base::TimeDelta::FromDays(365);
  const char kCommonName[] = "Brillo device";

  // Create the X509 certificate.
  int cert_serial_number = base::RandInt(0, std::numeric_limits<int>::max());
  auto cert =
      CreateCertificate(cert_serial_number, kCertExpiration, kCommonName);

  // Create RSA key pair.
  auto rsa_key_pair = GenerateRSAKeyPair(kKeyLengthBits);

  // Store the private key to a temp buffer.
  // Do not assign it to |TLS_private_key_| yet until the end when we are sure
  // everything else has worked out.
  chromeos::SecureBlob private_key = StoreRSAPrivateKey(rsa_key_pair.get());

  // Create EVP key and set it to the certificate.
  auto key = std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)>{EVP_PKEY_new(),
                                                            EVP_PKEY_free};
  CHECK(key.get());
  // Transfer ownership of |rsa_key_pair| to |key|.
  CHECK(EVP_PKEY_assign_RSA(key.get(), rsa_key_pair.release()));
  CHECK(X509_set_pubkey(cert.get(), key.get()));

  // Sign the certificate.
  CHECK(X509_sign(cert.get(), key.get(), EVP_sha256()));

  TLS_certificate_ = StoreCertificate(cert.get());
  TLS_certificate_fingerprint_ = GetSha256Fingerprint(cert.get());
  TLS_private_key_ = std::move(private_key);

  // Update the TLS data in protocol handler config.
  for (auto& pair : config_.protocol_handlers) {
    if (pair.second.use_tls) {
      pair.second.certificate = TLS_certificate_;
      pair.second.certificate_fingerprint = TLS_certificate_fingerprint_;
      pair.second.private_key = TLS_private_key_;
    }
  }
}

}  // namespace webservd
