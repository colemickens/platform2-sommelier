// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_H_
#define SHILL_ETHERNET_H_

#include <map>
#include <string>

#include <base/cancelable_callback.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>

#include "shill/certificate_file.h"
#include "shill/device.h"
#include "shill/event_dispatcher.h"
#include "shill/refptr_types.h"
#include "shill/supplicant_eap_state_handler.h"
#include "shill/supplicant_event_delegate_interface.h"

namespace shill {

class CertificateFile;
class EapListener;
class EthernetEapProvider;
class ProxyFactory;
class Sockets;
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
  virtual ~Ethernet();

  virtual void Start(Error *error, const EnabledStateChangedCallback &callback);
  virtual void Stop(Error *error, const EnabledStateChangedCallback &callback);
  virtual void LinkEvent(unsigned int flags, unsigned int change);
  virtual void ConnectTo(EthernetService *service);
  virtual void DisconnectFrom(EthernetService *service);

  // Test to see if conditions are correct for EAP authentication (both
  // credentials and a remote EAP authenticator is present) and initiate
  // an authentication if possible.
  virtual void TryEapAuthentication();

  // Implementation of SupplicantEventDelegateInterface.  These methods
  // are called by SupplicantInterfaceProxy, in response to events from
  // wpa_supplicant.
  virtual void BSSAdded(
      const ::DBus::Path &BSS,
      const std::map<std::string, ::DBus::Variant> &properties);
  virtual void BSSRemoved(const ::DBus::Path &BSS);
  virtual void Certification(
      const std::map<std::string, ::DBus::Variant> &properties);
  virtual void EAPEvent(
      const std::string &status, const std::string &parameter);
  virtual void PropertiesChanged(
      const std::map<std::string, ::DBus::Variant> &properties);
  virtual void ScanDone();

 private:
  friend class EthernetTest;

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

  void SetupWakeOnLan();

  EthernetServiceRefPtr service_;
  bool link_up_;

  // Track whether we have completed EAP authentication successfully.
  bool is_eap_authenticated_;

  // Track whether an EAP authenticator has been detected on this link.
  bool is_eap_detected_;
  scoped_ptr<EapListener> eap_listener_;

  // Track the progress of EAP authentication.
  SupplicantEAPStateHandler eap_state_handler_;

  // Proxy instances used to talk to wpa_supplicant.
  scoped_ptr<SupplicantProcessProxyInterface> supplicant_process_proxy_;
  scoped_ptr<SupplicantInterfaceProxyInterface> supplicant_interface_proxy_;
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

  scoped_ptr<Sockets> sockets_;

  base::WeakPtrFactory<Ethernet> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Ethernet);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_H_
