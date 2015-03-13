// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_ETHERNET_H_
#define SHILL_ETHERNET_ETHERNET_H_

#include <map>
#include <memory>
#include <string>

#include <base/cancelable_callback.h>
#include <base/memory/weak_ptr.h>

#include "shill/certificate_file.h"
#include "shill/device.h"
#include "shill/event_dispatcher.h"
#include "shill/refptr_types.h"
#include "shill/supplicant/supplicant_eap_state_handler.h"
#include "shill/supplicant/supplicant_event_delegate_interface.h"

namespace shill {

class CertificateFile;
class EapListener;
class EthernetEapProvider;
class ProxyFactory;
class Sockets;
class StoreInterface;
class SupplicantEAPStateHandler;
class SupplicantInterfaceProxyInterface;
class SupplicantProcessProxyInterface;

class Ethernet : public Device, public SupplicantEventDelegateInterface {
 public:
  Ethernet(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Metrics *metrics,
           Manager *manager,
           const std::string& link_name,
           const std::string &address,
           int interface_index);
  ~Ethernet() override;

  void Start(Error *error,
             const EnabledStateChangedCallback &callback) override;
  void Stop(Error *error, const EnabledStateChangedCallback &callback) override;
  void LinkEvent(unsigned int flags, unsigned int change) override;
  bool Load(StoreInterface *storage) override;
  bool Save(StoreInterface *storage) override;

  virtual void ConnectTo(EthernetService *service);
  virtual void DisconnectFrom(EthernetService *service);

  // Test to see if conditions are correct for EAP authentication (both
  // credentials and a remote EAP authenticator is present) and initiate
  // an authentication if possible.
  virtual void TryEapAuthentication();

  // Implementation of SupplicantEventDelegateInterface.  These methods
  // are called by SupplicantInterfaceProxy, in response to events from
  // wpa_supplicant.
  void BSSAdded(
      const ::DBus::Path &BSS,
      const std::map<std::string, ::DBus::Variant> &properties) override;
  void BSSRemoved(const ::DBus::Path &BSS) override;
  void Certification(
      const std::map<std::string, ::DBus::Variant> &properties) override;
  void EAPEvent(
  const std::string &status, const std::string &parameter) override;
  void PropertiesChanged(
      const std::map<std::string, ::DBus::Variant> &properties) override;
  void ScanDone(const bool &/*success*/) override;

  virtual bool link_up() const { return link_up_; }

 private:
  friend class EthernetTest;
  friend class EthernetServiceTest;  // For weak_ptr_factory_.
  friend class PPPoEServiceTest;     // For weak_ptr_factory_.

  // Return a pointer to the EAP provider for Ethernet devices.
  EthernetEapProvider *GetEapProvider();

  // Return a reference to the shared service that contains EAP credentials
  // for Ethernet.
  ServiceConstRefPtr GetEapService();

  // Invoked by |eap_listener_| when an EAP authenticator is detected.
  void OnEapDetected();

  // Start and stop a supplicant instance on this link.
  bool StartSupplicant();
  void StopSupplicant();

  // Start the EAP authentication process.
  bool StartEapAuthentication();

  // Change our EAP authentication state.
  void SetIsEapAuthenticated(bool is_eap_authenticated);

  // Callback tasks run as a result of event delegate methods.
  void CertificationTask(const std::string &subject, uint32_t depth);
  void EAPEventTask(const std::string &status, const std::string &parameter);
  void SupplicantStateChangedTask(const std::string &state);

  // Callback task run as a result of TryEapAuthentication().
  void TryEapAuthenticationTask();

  // Accessors for the PPoE property.
  bool GetPPPoEMode(Error *error);
  bool ConfigurePPPoEMode(const bool &mode, Error *error);
  void ClearPPPoEMode(Error *error);

  // Helpers for creating services with |this| as their device.
  EthernetServiceRefPtr CreateEthernetService();
  EthernetServiceRefPtr CreatePPPoEService();

  void SetupWakeOnLan();

  ControlInterface *control_interface_;

  EthernetServiceRefPtr service_;
  bool link_up_;

  // Track whether we have completed EAP authentication successfully.
  bool is_eap_authenticated_;

  // Track whether an EAP authenticator has been detected on this link.
  bool is_eap_detected_;
  std::unique_ptr<EapListener> eap_listener_;

  // Track the progress of EAP authentication.
  SupplicantEAPStateHandler eap_state_handler_;

  // Proxy instances used to talk to wpa_supplicant.
  std::unique_ptr<SupplicantProcessProxyInterface> supplicant_process_proxy_;
  std::unique_ptr<SupplicantInterfaceProxyInterface>
      supplicant_interface_proxy_;
  std::string supplicant_interface_path_;
  std::string supplicant_network_path_;

  // Certificate file instance to generate public key data for remote
  // authentication.
  CertificateFile certificate_file_;

  // Make sure TryEapAuthenticationTask is only queued for execution once
  // at a time.
  base::CancelableClosure try_eap_authentication_callback_;

  // Store cached copy of proxy factory singleton for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  std::unique_ptr<Sockets> sockets_;

  base::WeakPtrFactory<Ethernet> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Ethernet);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_ETHERNET_H_
