// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/privet_handler.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/location.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>
#include <chromeos/http/http_request.h>
#include <chromeos/strings/string_utils.h>

#include "privetd/cloud_delegate.h"
#include "privetd/constants.h"
#include "privetd/device_delegate.h"
#include "privetd/identity_delegate.h"
#include "privetd/security_delegate.h"
#include "privetd/wifi_delegate.h"

namespace privetd {

namespace {

const char kInfoVersionKey[] = "version";
const char kInfoVersionValue[] = "3.0";

const char kNameKey[] = "name";
const char kDescrptionKey[] = "description";
const char kLocationKey[] = "location";

const char kGcdKey[] = "gcd";
const char kWifiKey[] = "wifi";
const char kStatusKey[] = "status";
const char kErrorKey[] = "error";
const char kCryptoKey[] = "crypto";
const char kStatusErrorValue[] = "error";

const char kInfoIdKey[] = "id";
const char kInfoServicesKey[] = "services";
const char kInfoClassKey[] = "class";
const char kInfoModelIdKey[] = "modelId";

const char kInfoEndpointsKey[] = "endpoints";
const char kInfoEndpointsHttpPortKey[] = "httpPort";
const char kInfoEndpointsHttpUpdatePortKey[] = "httpUpdatesPort";
const char kInfoEndpointsHttpsPortKey[] = "httpsPort";
const char kInfoEndpointsHttpsUpdatePortKey[] = "httpsUpdatesPort";

const char kInfoAuthenticationKey[] = "authentication";

const char kInfoWifiCapabilitiesKey[] = "capabilities";
const char kInfoWifiSsidKey[] = "ssid";
const char kInfoWifiHostedSsidKey[] = "hostedSsid";

const char kInfoUptimeKey[] = "uptime";
const char kInfoApiKey[] = "api";

const char kPairingKey[] = "pairing";
const char kPairingSessionIdKey[] = "sessionId";
const char kPairingDeviceCommitmentKey[] = "deviceCommitment";
const char kPairingClientCommitmentKey[] = "clientCommitment";
const char kPairingFingerprintKey[] = "certFingerprint";
const char kPairingSignatureKey[] = "certSignature";

const char kAuthTypeAnonymousValue[] = "anonymous";
const char kAuthTypePairingValue[] = "pairing";
const char kAuthTypeCloudValue[] = "cloud";

const char kAuthModeKey[] = "mode";
const char kAuthCodeKey[] = "authCode";
const char kAuthRequestedScopeKey[] = "requestedScope";
const char kAuthScopeAutoValue[] = "auto";

const char kAuthAccessTokenKey[] = "accessToken";
const char kAuthTokenTypeKey[] = "tokenType";
const char kAuthExpiresInKey[] = "expiresIn";
const char kAuthScopeKey[] = "scope";

const char kAuthorizationHeaderPrefix[] = "Privet";

const char kErrorCodeKey[] = "code";
const char kErrorMessageKey[] = "message";
const char kErrorDebugInfoKey[] = "debugInfo";

const char kSetupStartSsidKey[] = "ssid";
const char kSetupStartPassKey[] = "passphrase";
const char kSetupStartTicketIdKey[] = "ticketId";
const char kSetupStartUserKey[] = "user";

const char kFingerprintKey[] = "fingerprint";
const char kCommandsKey[] = "commands";
const char kCommandsIdKey[] = "id";

const char kInvalidParamValueFormat[] = "Invalid parameter: '%s'='%s'";

const int kAccesssTokenExpirationSeconds = 3600;

// Threshold to reduce probability of expiration because of clock difference
// between device and client. Value is just a guess.
const int kAccesssTokenExpirationThresholdSeconds = 300;

template <class Container>
std::unique_ptr<base::ListValue> ToValue(const Container& list) {
  std::unique_ptr<base::ListValue> value_list(new base::ListValue());
  for (const std::string& val : list)
    value_list->AppendString(val);
  return std::move(value_list);
}

template <typename T>
class EnumToStringMap {
 public:
  static std::string FindNameById(T id) {
    for (const Map& m : kMap) {
      if (m.id == id) {
        CHECK(m.name);
        return m.name;
      }
    }
    NOTREACHED();
    return std::string();
  }

  static bool FindIdByName(const std::string& name, T* id) {
    for (const Map& m : kMap) {
      if (m.name && m.name == name) {
        *id = m.id;
        return true;
      }
    }
    return false;
  }

 private:
  struct Map {
    const T id;
    const char* const name;
  };
  static const Map kMap[];
};

template <>
const EnumToStringMap<ConnectionState::Status>::Map
    EnumToStringMap<ConnectionState::Status>::kMap[] = {
        {ConnectionState::kDisabled, "disabled"},
        {ConnectionState::kUnconfigured, "unconfigured"},
        {ConnectionState::kConnecting, "connecting"},
        {ConnectionState::kOnline, "online"},
        {ConnectionState::kOffline, "offline"},
};

template <>
const EnumToStringMap<SetupState::Status>::Map
    EnumToStringMap<SetupState::Status>::kMap[] = {
        {SetupState::kNone, nullptr},
        {SetupState::kInProgress, "inProgress"},
        {SetupState::kSuccess, "success"},
};

template <>
const EnumToStringMap<WifiType>::Map EnumToStringMap<WifiType>::kMap[] = {
    {WifiType::kWifi24, "2.4GHz"},
    {WifiType::kWifi50, "5.0GHz"},
};

template <>
const EnumToStringMap<PairingType>::Map EnumToStringMap<PairingType>::kMap[] = {
    {PairingType::kPinCode, "pinCode"},
    {PairingType::kEmbeddedCode, "embeddedCode"},
    {PairingType::kUltrasound32, "ultrasound32"},
    {PairingType::kAudible32, "audible32"},
};

template <>
const EnumToStringMap<CryptoType>::Map EnumToStringMap<CryptoType>::kMap[] = {
    {CryptoType::kNone, "none"},
    {CryptoType::kSpake_p224, "p224_spake2"},
    {CryptoType::kSpake_p256, "p256_spake2"},
};

template <>
const EnumToStringMap<AuthScope>::Map EnumToStringMap<AuthScope>::kMap[] = {
    {AuthScope::kNone, nullptr},
    {AuthScope::kGuest, "guest"},
    {AuthScope::kViewer, "viewer"},
    {AuthScope::kUser, "user"},
    {AuthScope::kOwner, "owner"},
};

struct {
  const char* const reason;
  int code;
} kReasonToCode[] = {
    {errors::kInvalidClientCommitment, chromeos::http::status_code::Forbidden},
    {errors::kInvalidFormat, chromeos::http::status_code::BadRequest},
    {errors::kMissingAuthorization, chromeos::http::status_code::Denied},
    {errors::kInvalidAuthorization, chromeos::http::status_code::Denied},
    {errors::kInvalidAuthorizationScope,
     chromeos::http::status_code::Forbidden},
    {errors::kCommitmentMismatch, chromeos::http::status_code::Forbidden},
    {errors::kUnknownSession, chromeos::http::status_code::NotFound},
    {errors::kInvalidAuthCode, chromeos::http::status_code::Forbidden},
    {errors::kInvalidAuthMode, chromeos::http::status_code::BadRequest},
    {errors::kInvalidRequestedScope, chromeos::http::status_code::BadRequest},
    {errors::kAccessDenied, chromeos::http::status_code::Forbidden},
    {errors::kInvalidParams, chromeos::http::status_code::BadRequest},
    {errors::kSetupUnavailable, chromeos::http::status_code::BadRequest},
    {errors::kDeviceBusy, chromeos::http::status_code::ServiceUnavailable},
    {errors::kInvalidState, chromeos::http::status_code::InternalServerError},
    {errors::kNotFound, chromeos::http::status_code::NotFound},
    {errors::kNotImplemented, chromeos::http::status_code::NotSupported},
};

template <typename T>
std::string EnumToString(T id) {
  return EnumToStringMap<T>::FindNameById(id);
}

template <typename T>
bool StringToEnum(const std::string& name, T* id) {
  return EnumToStringMap<T>::FindIdByName(name, id);
}

AuthScope AuthScopeFromString(const std::string& scope, AuthScope auto_scope) {
  if (scope == kAuthScopeAutoValue)
    return auto_scope;
  AuthScope scope_id = AuthScope::kNone;
  StringToEnum(scope, &scope_id);
  return scope_id;
}

std::string GetAuthTokenFromAuthHeader(const std::string& auth_header) {
  std::string name;
  std::string value;
  chromeos::string_utils::SplitAtFirst(auth_header, " ", &name, &value);
  return value;
}

std::unique_ptr<base::DictionaryValue> ErrorInfoToJson(
    const chromeos::Error& error) {
  std::unique_ptr<base::DictionaryValue> output{new base::DictionaryValue};
  output->SetString(kErrorMessageKey, error.GetMessage());
  output->SetString(kErrorCodeKey, error.GetCode());
  return output;
}

// Creates JSON similar to GCD server error format.
std::unique_ptr<base::DictionaryValue> ErrorToJson(
    const chromeos::Error& error) {
  std::unique_ptr<base::DictionaryValue> output{ErrorInfoToJson(error)};

  // Optional debug information.
  std::unique_ptr<base::ListValue> errors{new base::ListValue};
  for (const chromeos::Error* it = &error; it; it = it->GetInnerError()) {
    std::unique_ptr<base::DictionaryValue> inner{ErrorInfoToJson(error)};
    tracked_objects::Location location{
        error.GetLocation().function_name.c_str(),
        error.GetLocation().file_name.c_str(),
        error.GetLocation().line_number,
        nullptr};
    inner->SetString(kErrorDebugInfoKey, location.ToString());
    errors->Append(inner.release());
  }
  output->Set(kErrorDebugInfoKey, errors.release());
  return output;
}

template <class T>
void SetState(const T& state, base::DictionaryValue* parent) {
  if (!state.error()) {
    parent->SetString(kStatusKey, EnumToString(state.status()));
    return;
  }
  parent->SetString(kStatusKey, kStatusErrorValue);
  parent->Set(kErrorKey, ErrorToJson(*state.error()).release());
}

void ReturnError(const chromeos::Error& error,
                 const PrivetHandler::RequestCallback& callback) {
  int code = chromeos::http::status_code::InternalServerError;
  for (const auto& it : kReasonToCode) {
    if (error.HasError(errors::kDomain, it.reason)) {
      code = it.code;
      break;
    }
  }
  std::unique_ptr<base::DictionaryValue> output{new base::DictionaryValue};
  output->Set(kErrorKey, ErrorToJson(error).release());
  callback.Run(code, *output);
}

void OnCommandRequestSucceeded(const PrivetHandler::RequestCallback& callback,
                               const base::DictionaryValue& output) {
  callback.Run(chromeos::http::status_code::Ok, output);
}

void OnCommandRequestFailed(const PrivetHandler::RequestCallback& callback,
                            chromeos::Error* error) {
  if (error->HasError("gcd", "unknown_command")) {
    chromeos::ErrorPtr new_error = error->Clone();
    chromeos::Error::AddTo(&new_error, FROM_HERE, errors::kDomain,
                           errors::kNotFound, "Unknown command ID");
    return ReturnError(*new_error, callback);
  }
  return ReturnError(*error, callback);
}

}  // namespace

PrivetHandler::PrivetHandler(CloudDelegate* cloud,
                             DeviceDelegate* device,
                             SecurityDelegate* security,
                             WifiDelegate* wifi,
                             IdentityDelegate* identity)
    : cloud_(cloud),
      device_(device),
      security_(security),
      wifi_(wifi),
      identity_(identity) {
  if (cloud_)
    cloud_observer_.Add(cloud_);

  AddHandler("/privet/info", &PrivetHandler::HandleInfo, AuthScope::kGuest);
  AddHandler("/privet/v3/pairing/start", &PrivetHandler::HandlePairingStart,
             AuthScope::kGuest);
  AddHandler("/privet/v3/pairing/confirm", &PrivetHandler::HandlePairingConfirm,
             AuthScope::kGuest);
  AddHandler("/privet/v3/pairing/cancel", &PrivetHandler::HandlePairingCancel,
             AuthScope::kGuest);
  AddHandler("/privet/v3/auth", &PrivetHandler::HandleAuth, AuthScope::kGuest);
  AddHandler("/privet/v3/setup/start", &PrivetHandler::HandleSetupStart,
             AuthScope::kOwner);
  AddHandler("/privet/v3/setup/status", &PrivetHandler::HandleSetupStatus,
             AuthScope::kOwner);
  if (cloud_) {
    AddHandler("/privet/v3/commandDefs", &PrivetHandler::HandleCommandDefs,
               AuthScope::kUser);
    AddHandler("/privet/v3/commands/execute",
               &PrivetHandler::HandleCommandsExecute, AuthScope::kUser);
    AddHandler("/privet/v3/commands/status",
               &PrivetHandler::HandleCommandsStatus, AuthScope::kUser);
    AddHandler("/privet/v3/commands/cancel",
               &PrivetHandler::HandleCommandsCancel, AuthScope::kUser);
    AddHandler("/privet/v3/commands/list", &PrivetHandler::HandleCommandsList,
               AuthScope::kUser);
  }
}

PrivetHandler::~PrivetHandler() {
}

void PrivetHandler::OnCommandDefsChanged() {
  ++command_defs_fingerprint_;
}

void PrivetHandler::HandleRequest(const std::string& api,
                                  const std::string& auth_header,
                                  const base::DictionaryValue* input,
                                  const RequestCallback& callback) {
  chromeos::ErrorPtr error;
  if (!input) {
    chromeos::Error::AddTo(&error, FROM_HERE, errors::kDomain,
                           errors::kInvalidFormat, "Malformed JSON");
    return ReturnError(*error, callback);
  }
  auto handler = handlers_.find(api);
  if (handler == handlers_.end()) {
    chromeos::Error::AddTo(&error, FROM_HERE, errors::kDomain,
                           errors::kNotFound, "Path not found");
    return ReturnError(*error, callback);
  }
  if (auth_header.empty()) {
    chromeos::Error::AddTo(&error, FROM_HERE, errors::kDomain,
                           errors::kMissingAuthorization,
                           "Authorization header must not be empty");
    return ReturnError(*error, callback);
  }
  std::string token = GetAuthTokenFromAuthHeader(auth_header);
  if (token.empty()) {
    chromeos::Error::AddToPrintf(
        &error, FROM_HERE, errors::kDomain, errors::kInvalidAuthorization,
        "Invalid authorization header: %s", auth_header.c_str());
    return ReturnError(*error, callback);
  }
  AuthScope scope = AuthScope::kNone;
  if (token == kAuthTypeAnonymousValue) {
    scope = AuthScope::kGuest;
  } else {
    base::Time time;
    scope = security_->ParseAccessToken(token, &time);
    time += base::TimeDelta::FromSeconds(kAccesssTokenExpirationSeconds);
    time +=
        base::TimeDelta::FromSeconds(kAccesssTokenExpirationThresholdSeconds);
    if (time < base::Time::Now())
      scope = AuthScope::kNone;
  }
  if (scope == AuthScope::kNone) {
    chromeos::Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                                 errors::kInvalidAuthorization,
                                 "Invalid access token: %s", token.c_str());
    return ReturnError(*error, callback);
  }
  if (handler->second.first > scope) {
    chromeos::Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                                 errors::kInvalidAuthorizationScope,
                                 "Scope '%s' does not allow '%s'",
                                 EnumToString(scope).c_str(), api.c_str());
    return ReturnError(*error, callback);
  }
  (this->*handler->second.second)(*input, callback);
}

void PrivetHandler::AddHandler(const std::string& path,
                               ApiHandler handler,
                               AuthScope scope) {
  CHECK(handlers_.emplace(path, std::make_pair(scope, handler)).second);
}

void PrivetHandler::HandleInfo(const base::DictionaryValue&,
                               const RequestCallback& callback) {
  base::DictionaryValue output;
  output.SetString(kInfoVersionKey, kInfoVersionValue);
  output.SetString(kInfoIdKey, identity_->GetId());
  output.SetString(kNameKey, device_->GetName());

  std::string description{device_->GetDescription()};
  if (!description.empty())
    output.SetString(kDescrptionKey, description);

  std::string location{device_->GetLocation()};
  if (!location.empty())
    output.SetString(kLocationKey, location);

  std::string dev_class{device_->GetClass()};
  CHECK_EQ(2u, dev_class.size());
  output.SetString(kInfoClassKey, dev_class);
  std::string model_id{device_->GetModelId()};
  CHECK_EQ(3u, model_id.size());
  output.SetString(kInfoModelIdKey, model_id);
  output.Set(kInfoServicesKey, ToValue(device_->GetServices()).release());

  output.Set(kInfoAuthenticationKey, CreateInfoAuthSection().release());

  output.Set(kInfoEndpointsKey, CreateEndpointsSection().release());

  if (wifi_)
    output.Set(kWifiKey, CreateWifiSection().release());

  if (cloud_)
    output.Set(kGcdKey, CreateGcdSection().release());

  output.SetInteger(kInfoUptimeKey, device_->GetUptime().InSeconds());

  std::unique_ptr<base::ListValue> apis(new base::ListValue());
  for (const auto& key_value : handlers_)
    apis->AppendString(key_value.first);
  output.Set(kInfoApiKey, apis.release());

  callback.Run(chromeos::http::status_code::Ok, output);
}

void PrivetHandler::HandlePairingStart(const base::DictionaryValue& input,
                                       const RequestCallback& callback) {
  chromeos::ErrorPtr error;

  std::string pairing_str;
  input.GetString(kPairingKey, &pairing_str);

  std::string crypto_str;
  input.GetString(kCryptoKey, &crypto_str);

  PairingType pairing;
  std::set<PairingType> modes = security_->GetPairingTypes();
  if (!StringToEnum(pairing_str, &pairing) || !ContainsKey(modes, pairing)) {
    chromeos::Error::AddToPrintf(
        &error, FROM_HERE, errors::kDomain, errors::kInvalidParams,
        kInvalidParamValueFormat, kPairingKey, pairing_str.c_str());
    return ReturnError(*error, callback);
  }

  CryptoType crypto;
  std::set<CryptoType> cryptos = security_->GetCryptoTypes();
  if (!StringToEnum(crypto_str, &crypto) || !ContainsKey(cryptos, crypto)) {
    chromeos::Error::AddToPrintf(
        &error, FROM_HERE, errors::kDomain, errors::kInvalidParams,
        kInvalidParamValueFormat, kCryptoKey, crypto_str.c_str());
    return ReturnError(*error, callback);
  }

  std::string id;
  std::string commitment;
  if (!security_->StartPairing(pairing, crypto, &id, &commitment, &error))
    return ReturnError(*error, callback);

  base::DictionaryValue output;
  output.SetString(kPairingSessionIdKey, id);
  output.SetString(kPairingDeviceCommitmentKey, commitment);
  callback.Run(chromeos::http::status_code::Ok, output);
}

void PrivetHandler::HandlePairingConfirm(const base::DictionaryValue& input,
                                         const RequestCallback& callback) {
  std::string id;
  input.GetString(kPairingSessionIdKey, &id);

  std::string commitment;
  input.GetString(kPairingClientCommitmentKey, &commitment);

  std::string fingerprint;
  std::string signature;
  chromeos::ErrorPtr error;
  if (!security_->ConfirmPairing(id, commitment, &fingerprint, &signature,
                                 &error)) {
    return ReturnError(*error, callback);
  }

  base::DictionaryValue output;
  output.SetString(kPairingFingerprintKey, fingerprint);
  output.SetString(kPairingSignatureKey, signature);
  callback.Run(chromeos::http::status_code::Ok, output);
}

void PrivetHandler::HandlePairingCancel(const base::DictionaryValue& input,
                                        const RequestCallback& callback) {
  std::string id;
  input.GetString(kPairingSessionIdKey, &id);

  chromeos::ErrorPtr error;
  if (!security_->CancelPairing(id, &error))
    return ReturnError(*error, callback);

  base::DictionaryValue output;
  callback.Run(chromeos::http::status_code::Ok, output);
}

void PrivetHandler::HandleAuth(const base::DictionaryValue& input,
                               const RequestCallback& callback) {
  chromeos::ErrorPtr error;

  std::string auth_code_type;
  input.GetString(kAuthModeKey, &auth_code_type);

  std::string auth_code;
  input.GetString(kAuthCodeKey, &auth_code);

  AuthScope max_auth_scope = AuthScope::kNone;
  if (auth_code_type == kAuthTypeAnonymousValue) {
    max_auth_scope = AuthScope::kGuest;
  } else if (auth_code_type == kAuthTypePairingValue) {
    if (!security_->IsValidPairingCode(auth_code)) {
      chromeos::Error::AddToPrintf(
          &error, FROM_HERE, errors::kDomain, errors::kInvalidAuthCode,
          kInvalidParamValueFormat, kAuthCodeKey, auth_code.c_str());
      return ReturnError(*error, callback);
    }
    max_auth_scope = AuthScope::kOwner;
  } else {
    chromeos::Error::AddToPrintf(
        &error, FROM_HERE, errors::kDomain, errors::kInvalidAuthMode,
        kInvalidParamValueFormat, kAuthModeKey, auth_code_type.c_str());
    return ReturnError(*error, callback);
  }

  std::string requested_scope;
  input.GetString(kAuthRequestedScopeKey, &requested_scope);

  AuthScope requested_auth_scope =
      AuthScopeFromString(requested_scope, max_auth_scope);
  if (requested_auth_scope == AuthScope::kNone) {
    chromeos::Error::AddToPrintf(
        &error, FROM_HERE, errors::kDomain, errors::kInvalidRequestedScope,
        kInvalidParamValueFormat, kAuthRequestedScopeKey,
        requested_scope.c_str());
    return ReturnError(*error, callback);
  }

  if (requested_auth_scope > max_auth_scope) {
    chromeos::Error::AddToPrintf(
        &error, FROM_HERE, errors::kDomain, errors::kAccessDenied,
        "Scope '%s' is not allowed for '%s'",
        EnumToString(requested_auth_scope).c_str(), auth_code.c_str());
    return ReturnError(*error, callback);
  }

  base::DictionaryValue output;
  output.SetString(
      kAuthAccessTokenKey,
      security_->CreateAccessToken(requested_auth_scope, base::Time::Now()));
  output.SetString(kAuthTokenTypeKey, kAuthorizationHeaderPrefix);
  output.SetInteger(kAuthExpiresInKey, kAccesssTokenExpirationSeconds);
  output.SetString(kAuthScopeKey, EnumToString(requested_auth_scope));
  callback.Run(chromeos::http::status_code::Ok, output);
}

void PrivetHandler::HandleSetupStart(const base::DictionaryValue& input,
                                     const RequestCallback& callback) {
  std::string param;
  if (input.GetString(kNameKey, &param))
    device_->SetName(param);
  if (input.GetString(kDescrptionKey, &param))
    device_->SetDescription(param);
  if (input.GetString(kLocationKey, &param))
    device_->SetLocation(param);

  std::string ssid;
  std::string passphrase;
  std::string ticket;
  std::string user;

  chromeos::ErrorPtr error;

  const base::DictionaryValue* wifi = nullptr;
  if (input.GetDictionary(kWifiKey, &wifi)) {
    if (!wifi_ || wifi_->GetTypes().empty()) {
      chromeos::Error::AddTo(&error, FROM_HERE, errors::kDomain,
                             errors::kSetupUnavailable,
                             "WiFi setup unavailible");
      return ReturnError(*error, callback);
    }
    wifi->GetString(kSetupStartSsidKey, &ssid);
    if (ssid.empty()) {
      chromeos::Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                                   errors::kInvalidParams,
                                   kInvalidParamValueFormat, kWifiKey, "");
      return ReturnError(*error, callback);
    }
    wifi->GetString(kSetupStartPassKey, &passphrase);
  }

  const base::DictionaryValue* registration = nullptr;
  if (input.GetDictionary(kGcdKey, &registration)) {
    if (!cloud_) {
      chromeos::Error::AddTo(&error, FROM_HERE, errors::kDomain,
                             errors::kSetupUnavailable,
                             "GCD setup unavailible");
      return ReturnError(*error, callback);
    }
    registration->GetString(kSetupStartTicketIdKey, &ticket);
    if (ticket.empty()) {
      chromeos::Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                                   errors::kInvalidParams,
                                   kInvalidParamValueFormat, kGcdKey, "");
      return ReturnError(*error, callback);
    }
    registration->GetString(kSetupStartUserKey, &user);
  }

  if (!ssid.empty() && !wifi_->ConfigureCredentials(ssid, passphrase, &error))
    return ReturnError(*error, callback);

  if (!ticket.empty() && !cloud_->Setup(ticket, user, &error))
    return ReturnError(*error, callback);

  return HandleSetupStatus(input, callback);
}

void PrivetHandler::HandleSetupStatus(const base::DictionaryValue& input,
                                      const RequestCallback& callback) {
  base::DictionaryValue output;

  if (cloud_) {
    const SetupState& state = cloud_->GetSetupState();
    if (!state.IsStatusEqual(SetupState::kNone)) {
      base::DictionaryValue* gcd = new base::DictionaryValue;
      output.Set(kGcdKey, gcd);
      SetState(state, gcd);
      if (state.IsStatusEqual(SetupState::kSuccess))
        gcd->SetString(kInfoIdKey, cloud_->GetCloudId());
    }
  }

  if (wifi_) {
    const SetupState& state = wifi_->GetSetupState();
    if (!state.IsStatusEqual(SetupState::kNone)) {
      base::DictionaryValue* wifi = new base::DictionaryValue;
      output.Set(kWifiKey, wifi);
      SetState(state, wifi);
      if (state.IsStatusEqual(SetupState::kSuccess))
        wifi->SetString(kInfoWifiSsidKey, wifi_->GetCurrentlyConnectedSsid());
    }
  }

  callback.Run(chromeos::http::status_code::Ok, output);
}

void PrivetHandler::HandleCommandDefs(const base::DictionaryValue& input,
                                      const RequestCallback& callback) {
  base::DictionaryValue output;
  base::DictionaryValue* defs = cloud_->GetCommandDef().DeepCopy();
  output.Set(kCommandsKey, defs);
  output.SetString(kFingerprintKey,
                   base::IntToString(command_defs_fingerprint_));

  callback.Run(chromeos::http::status_code::Ok, output);
}

void PrivetHandler::HandleCommandsExecute(const base::DictionaryValue& input,
                                          const RequestCallback& callback) {
  cloud_->AddCommand(input, base::Bind(&OnCommandRequestSucceeded, callback),
                     base::Bind(&OnCommandRequestFailed, callback));
}

void PrivetHandler::HandleCommandsStatus(const base::DictionaryValue& input,
                                         const RequestCallback& callback) {
  std::string id;
  if (!input.GetString(kCommandsIdKey, &id)) {
    chromeos::ErrorPtr error;
    chromeos::Error::AddToPrintf(
        &error, FROM_HERE, errors::kDomain, errors::kInvalidParams,
        kInvalidParamValueFormat, kCommandsIdKey, id.c_str());
    return ReturnError(*error, callback);
  }
  cloud_->GetCommand(id, base::Bind(&OnCommandRequestSucceeded, callback),
                     base::Bind(&OnCommandRequestFailed, callback));
}

void PrivetHandler::HandleCommandsList(const base::DictionaryValue& input,
                                       const RequestCallback& callback) {
  cloud_->ListCommands(base::Bind(&OnCommandRequestSucceeded, callback),
                       base::Bind(&OnCommandRequestFailed, callback));
}

void PrivetHandler::HandleCommandsCancel(const base::DictionaryValue& input,
                                         const RequestCallback& callback) {
  std::string id;
  if (!input.GetString(kCommandsIdKey, &id)) {
    chromeos::ErrorPtr error;
    chromeos::Error::AddToPrintf(
        &error, FROM_HERE, errors::kDomain, errors::kInvalidParams,
        kInvalidParamValueFormat, kCommandsIdKey, id.c_str());
    return ReturnError(*error, callback);
  }
  cloud_->CancelCommand(id, base::Bind(&OnCommandRequestSucceeded, callback),
                        base::Bind(&OnCommandRequestFailed, callback));
}

std::unique_ptr<base::DictionaryValue> PrivetHandler::CreateEndpointsSection()
    const {
  std::unique_ptr<base::DictionaryValue> endpoints(new base::DictionaryValue());
  auto http_endpoint = device_->GetHttpEnpoint();
  endpoints->SetInteger(kInfoEndpointsHttpPortKey, http_endpoint.first);
  endpoints->SetInteger(kInfoEndpointsHttpUpdatePortKey, http_endpoint.second);

  auto https_endpoint = device_->GetHttpsEnpoint();
  endpoints->SetInteger(kInfoEndpointsHttpsPortKey, https_endpoint.first);
  endpoints->SetInteger(kInfoEndpointsHttpsUpdatePortKey,
                        https_endpoint.second);

  return std::move(endpoints);
}

std::unique_ptr<base::DictionaryValue> PrivetHandler::CreateInfoAuthSection()
    const {
  std::unique_ptr<base::DictionaryValue> auth(new base::DictionaryValue());

  std::unique_ptr<base::ListValue> pairing_types(new base::ListValue());
  for (PairingType type : security_->GetPairingTypes())
    pairing_types->AppendString(EnumToString(type));
  auth->Set(kPairingKey, pairing_types.release());

  std::unique_ptr<base::ListValue> auth_types(new base::ListValue());
  auth_types->AppendString(kAuthTypeAnonymousValue);
  auth_types->AppendString(kAuthTypePairingValue);
  if (cloud_ &&
      cloud_->GetConnectionState().IsStatusEqual(ConnectionState::kOnline)) {
    auth_types->AppendString(kAuthTypeCloudValue);
  }
  auth->Set(kAuthModeKey, auth_types.release());

  std::unique_ptr<base::ListValue> crypto_types(new base::ListValue());
  for (CryptoType type : security_->GetCryptoTypes())
    crypto_types->AppendString(EnumToString(type));
  auth->Set(kCryptoKey, crypto_types.release());

  return std::move(auth);
}

std::unique_ptr<base::DictionaryValue> PrivetHandler::CreateWifiSection()
    const {
  std::unique_ptr<base::DictionaryValue> wifi(new base::DictionaryValue());

  std::unique_ptr<base::ListValue> capabilities(new base::ListValue());
  for (WifiType type : wifi_->GetTypes())
    capabilities->AppendString(EnumToString(type));
  wifi->Set(kInfoWifiCapabilitiesKey, capabilities.release());

  wifi->SetString(kInfoWifiSsidKey, wifi_->GetCurrentlyConnectedSsid());

  std::string hosted_ssid = wifi_->GetHostedSsid();
  const ConnectionState& state = wifi_->GetConnectionState();
  if (!hosted_ssid.empty()) {
    DCHECK(!state.IsStatusEqual(ConnectionState::kDisabled));
    DCHECK(!state.IsStatusEqual(ConnectionState::kOnline));
    wifi->SetString(kInfoWifiHostedSsidKey, hosted_ssid);
  }
  SetState(state, wifi.get());
  return std::move(wifi);
}

std::unique_ptr<base::DictionaryValue> PrivetHandler::CreateGcdSection() const {
  std::unique_ptr<base::DictionaryValue> gcd(new base::DictionaryValue());
  gcd->SetString(kInfoIdKey, cloud_->GetCloudId());
  SetState(cloud_->GetConnectionState(), gcd.get());
  return std::move(gcd);
}

bool StringToPairingType(const std::string& mode, PairingType* id) {
  return StringToEnum(mode, id);
}

std::string PairingTypeToString(PairingType id) {
  return EnumToString(id);
}

}  // namespace privetd
