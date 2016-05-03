//
// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/binder/service_binder_adaptor.h"

#include <binder/Status.h>
#include <utils/String8.h>
// TODO(samueltan): remove these includes once b/27270173 is resolved,
// and Service is no longer reliant on D-Bus service constants.
#if defined(__ANDROID__)
#include <dbus/service_constants.h>
#else
#include <chromeos/dbus/service_constants.h>
#endif  // __ANDROID__

#include "shill/binder/service_binder_service.h"
#include "shill/logging.h"
#include "shill/service.h"
#include "shill/vpn/vpn_service.h"

using android::binder::Status;
using android::IBinder;
using android::sp;
using android::String8;
using android::system::connectivity::shill::IPropertyChangedCallback;
using android::system::connectivity::shill::IService;
using std::string;

namespace {
const char kBinderRpcReasonString[] = "Binder RPC";
// Generic error code to indicate failure in any Binder method handler.
const int kErrorCode = -1;
}  // namespace

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kBinder;
static string ObjectID(ServiceBinderAdaptor* s) {
  return "Service binder adaptor (id " + s->GetRpcIdentifier() + ", " +
         s->service()->unique_name() + ")";
}
}  // namespace Logging

ServiceBinderAdaptor::ServiceBinderAdaptor(BinderControl* control,
                                           Service* service,
                                           const std::string& id)
    : BinderAdaptor(control, id), service_(service), weak_ptr_factory_(this) {
  set_binder_service(
      new ServiceBinderService(weak_ptr_factory_.GetWeakPtr(), id));
}

void ServiceBinderAdaptor::EmitBoolChanged(const string& name, bool /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ServiceBinderAdaptor::EmitUint8Changed(const string& name,
                                            uint8_t /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ServiceBinderAdaptor::EmitUint16Changed(const string& name,
                                             uint16_t /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ServiceBinderAdaptor::EmitUint16sChanged(const string& name,
                                              const Uint16s& /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ServiceBinderAdaptor::EmitUintChanged(const string& name,
                                           uint32_t /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ServiceBinderAdaptor::EmitIntChanged(const string& name, int /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ServiceBinderAdaptor::EmitRpcIdentifierChanged(const string& name,
                                                    const string& /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ServiceBinderAdaptor::EmitStringChanged(const string& name,
                                             const string& /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ServiceBinderAdaptor::EmitStringmapChanged(const string& name,
                                                const Stringmap& /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

Status ServiceBinderAdaptor::Connect() {
  SLOG(this, 2) << __func__;
  Error e;
  service_->UserInitiatedConnect(kBinderRpcReasonString, &e);
  return e.ToBinderStatus();
}

Status ServiceBinderAdaptor::GetState(int32_t* _aidl_return) {
  SLOG(this, 2) << __func__;
  Error e;
  string state = service_->CalculateState(&e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  if (state == shill::kStateIdle) {
    *_aidl_return = IService::STATE_IDLE;
  } else if (state == shill::kStateAssociation) {
    *_aidl_return = IService::STATE_ASSOC;
  } else if (state == shill::kStateConfiguration) {
    *_aidl_return = IService::STATE_CONFIG;
  } else if (state == shill::kStateReady) {
    *_aidl_return = IService::STATE_READY;
  } else if (state == shill::kStateFailure) {
    *_aidl_return = IService::STATE_FAILURE;
  } else if (state == shill::kStatePortal) {
    *_aidl_return = IService::STATE_PORTAL;
  } else if (state == shill::kStateOnline) {
    *_aidl_return = IService::STATE_ONLINE;
  } else {
    LOG(ERROR) << __func__ << ": unsupported state";
    return Status::fromServiceSpecificError(
        kErrorCode, String8("Unsupported state"));
  }
  return Status::ok();
}

Status ServiceBinderAdaptor::GetStrength(int8_t* _aidl_return) {
  SLOG(this, 2) << __func__;
  *_aidl_return = service_->strength();
  return Status::ok();
}

Status ServiceBinderAdaptor::GetError(int32_t* _aidl_return) {
  SLOG(this, 2) << __func__;
  return ShillErrorToIServiceErrorType(service_->error(), _aidl_return);
}

Status ServiceBinderAdaptor::GetTethering(int32_t* _aidl_return) {
  SLOG(this, 2) << __func__;
  Error e;
  string tethering = service_->GetTethering(&e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  if (tethering == kTetheringConfirmedState) {
    *_aidl_return = IService::TETHERING_CONFIRMED;
  } else if (tethering == kTetheringSuspectedState) {
    *_aidl_return = IService::TETHERING_SUSPECTED;
  } else if (tethering == kTetheringNotDetectedState) {
    *_aidl_return = IService::TETHERING_NOT_DETECTED;
  } else {
    LOG(ERROR) << __func__ << ": unsupported tethering state";
    return Status::fromServiceSpecificError(
        kErrorCode, String8("Unsupported tethering state"));
  }
  return Status::ok();
}

Status ServiceBinderAdaptor::GetType(int32_t* _aidl_return) {
  SLOG(this, 2) << __func__;
  Error e;
  string technology = service_->CalculateTechnology(&e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  return ShillTechnologyToIServiceType(technology, _aidl_return);
}

Status ServiceBinderAdaptor::GetPhysicalTechnology(int32_t* _aidl_return) {
  SLOG(this, 2) << __func__;
  int32_t service_type;
  GetType(&service_type);
  if (service_type != IService::TYPE_VPN) {
    LOG(ERROR) << __func__ << ": this method is only valid for VPN services";
    return Status::fromServiceSpecificError(
        kErrorCode, String8("This method is only valid for VPN services"));
  }
  // This is safe, because we know that the service is a VPN service.
  VPNService* vpn_service = static_cast<VPNService*>(service_);
  Error e;
  string physical_techology = vpn_service->GetPhysicalTechnologyProperty(&e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  return ShillTechnologyToIServiceType(physical_techology, _aidl_return);
}

Status ServiceBinderAdaptor::RegisterPropertyChangedSignalHandler(
    const sp<IPropertyChangedCallback>& callback) {
  AddPropertyChangedSignalHandler(callback);
  return Status::ok();
}

Status ServiceBinderAdaptor::ShillTechnologyToIServiceType(
    const string& technology, int32_t* type) {
  if (technology == kTypeEthernet) {
    *type = IService::TYPE_ETHERNET;
  } else if (technology == kTypeWifi) {
    *type = IService::TYPE_WIFI;
  } else if (technology == kTypeWimax) {
    *type = IService::TYPE_WIMAX;
  } else if (technology == kTypeCellular) {
    *type = IService::TYPE_CELLULAR;
  } else if (technology == kTypeVPN) {
    *type = IService::TYPE_VPN;
  } else if (technology == kTypePPPoE) {
    *type = IService::TYPE_PPPOE;
  } else {
    LOG(ERROR) << __func__ << ": unsupported technology type";
    return Status::fromServiceSpecificError(
        kErrorCode, String8("Unsupported technology type"));
  }
  return Status::ok();
}

Status ServiceBinderAdaptor::ShillErrorToIServiceErrorType(
    const string& error, int32_t* error_type) {
  if (error == kErrorAaaFailed) {
    *error_type = IService::ERROR_AAA_FAILED;
  } else if (error == kErrorActivationFailed) {
    *error_type = IService::ERROR_ACTIVATION_FAILED;
  } else if (error == kErrorBadPassphrase) {
    *error_type = IService::ERROR_BAD_PASSPHRASE;
  } else if (error == kErrorBadWEPKey) {
    *error_type = IService::ERROR_BAD_WEP_KEY;
  } else if (error == kErrorConnectFailed) {
    *error_type = IService::ERROR_CONNECT_FAILED;
  } else if (error == kErrorDNSLookupFailed) {
    *error_type = IService::ERROR_DNS_LOOKUP_FAILED;
  } else if (error == kErrorDhcpFailed) {
    *error_type = IService::ERROR_DHCP_FAILED;
  } else if (error == kErrorHTTPGetFailed) {
    *error_type = IService::ERROR_HTTP_GET_FAILED;
  } else if (error == kErrorInternal) {
    *error_type = IService::ERROR_INTERNAL;
  } else if (error == kErrorInvalidFailure) {
    *error_type = IService::ERROR_INVALID_FAILURE;
  } else if (error == kErrorIpsecCertAuthFailed) {
    *error_type = IService::ERROR_IPSEC_CERT_AUTH_FAILED;
  } else if (error == kErrorIpsecPskAuthFailed) {
    *error_type = IService::ERROR_IPSEC_PSK_AUTH_FAILED;
  } else if (error == kErrorNeedEvdo) {
    *error_type = IService::ERROR_NEED_EVDO;
  } else if (error == kErrorNeedHomeNetwork) {
    *error_type = IService::ERROR_NEED_HOME_NETWORK;
  } else if (error == kErrorNoFailure) {
    *error_type = IService::ERROR_NO_FAILURE;
  } else if (error == kErrorOtaspFailed) {
    *error_type = IService::ERROR_OTASP_FAILED;
  } else if (error == kErrorOutOfRange) {
    *error_type = IService::ERROR_OUT_OF_RANGE;
  } else if (error == kErrorPinMissing) {
    *error_type = IService::ERROR_PIN_MISSING;
  } else if (error == kErrorPppAuthFailed) {
    *error_type = IService::ERROR_PPP_AUTH_FAILED;
  } else if (error == kErrorUnknownFailure) {
    *error_type = IService::ERROR_UNKNOWN_FAILURE;
  } else {
    LOG(ERROR) << __func__ << ": unsupported error";
    return Status::fromServiceSpecificError(kErrorCode,
                                            String8("Unsupported error"));
  }
  return Status::ok();
}

}  // namespace shill
