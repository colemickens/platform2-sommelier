// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/privet_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/values.h>
#include <chromeos/http/http_request.h>
#include <chromeos/strings/string_utils.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/security_delegate.h"
#include "privetd/wifi_delegate.h"

namespace privetd {

namespace {

const char kInfoApiPath[] = "/privet/info";
const char kPairingStartApiPath[] = "/privet/v3/pairing/start";
const char kPairingConfirmApiPath[] = "/privet/v3/pairing/confirm";
const char kAuthApiPath[] = "/privet/v3/auth";
const char kSetupStartApiPath[] = "/privet/v3/setup/start";
const char kSetupStatusApiPath[] = "/privet/v3/setup/status";

const char kInfoVersionKey[] = "version";
const char kInfoVersionValue[] = "3.0";

const char kNameKey[] = "name";
const char kDescrptionKey[] = "description";
const char kLocationKey[] = "location";

const char kGcdKey[] = "gcd";
const char kWifiKey[] = "wifi";
const char kRequiredKey[] = "required";
const char kStatusKey[] = "status";
const char kErrorKey[] = "error";
const char kCryptoKey[] = "crypto";

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
const char kErrorReasonKey[] = "reason";
const char kErrorMessageKey[] = "message";

const char kSetupStartSsidKey[] = "ssid";
const char kSetupStartPassKey[] = "passphrase";
const char kSetupStartTicketIdKey[] = "ticketId";
const char kSetupStartUserKey[] = "user";

const int kAccesssTokenExpirationSeconds = 3600;

// Threshold to reduce probability of expiration because of clock difference
// between device and client. Value is just a guess.
const int kAccesssTokenExpirationThresholdSeconds = 300;

std::unique_ptr<base::ListValue> ToValue(const std::vector<std::string> list) {
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
        {ConnectionState::kError, "error"},
};

template <>
const EnumToStringMap<SetupState::Status>::Map
    EnumToStringMap<SetupState::Status>::kMap[] = {
        {SetupState::kNone, nullptr},
        {SetupState::kInProgress, "inProgress"},
        {SetupState::kSuccess, "success"},
        {SetupState::kError, "error"},
};

template <>
const EnumToStringMap<Error>::Map EnumToStringMap<Error>::kMap[] = {
    {Error::kNone, nullptr},
    {Error::kInvalidFormat, "invalidFormat"},
    {Error::kMissingAuthorization, "missingAuthorization"},
    {Error::kInvalidAuthorization, "invalidAuthorization"},
    {Error::kInvalidAuthorizationScope, "invalidAuthorizationScope"},
    {Error::kCommitmentMismatch, "commitmentMismatch"},
    {Error::kUnknownSession, "unknownSession"},
    {Error::kInvalidAuthCode, "invalidAuthCode"},
    {Error::kInvalidAuthMode, "invalidAuthMode"},
    {Error::kInvalidRequestedScope, "invalidRequestedScope"},
    {Error::kAccessDenied, "accessDenied"},
    {Error::kInvalidParams, "invalidParams"},
    {Error::kSetupUnavailable, "setupUnavailable"},
    {Error::kDeviceBusy, "deviceBusy"},
    {Error::kInvalidTicket, "invalidTicket"},
    {Error::kServerError, "serverError"},
    {Error::kDeviceConfigError, "deviceConfigError"},
    {Error::kInvalidSsid, "invalidSsid"},
    {Error::kInvalidPassphrase, "invalidPassphrase"},
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
    {PairingType::kUltrasoundDsssBroadcaster, "ultrasoundDsssBroadcaster"},
    {PairingType::kAudibleDtmfBroadcaster, "audibleDtmfBroadcaster"},
};

template <>
const EnumToStringMap<CryptoType>::Map EnumToStringMap<CryptoType>::kMap[] = {
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
  chromeos::string_utils::SplitAtFirst(auth_header, ' ', &name, &value);
  return value;
}

std::unique_ptr<base::DictionaryValue> CreateError(Error error,
                                                   const std::string& message) {
  if (error == Error::kNone)
    return nullptr;
  std::unique_ptr<base::DictionaryValue> output(new base::DictionaryValue);
  output->SetString(kErrorReasonKey, EnumToString(error));
  if (!message.empty())
    output->SetString(kErrorMessageKey, message);
  return output;
}

template <class T>
void SetState(const T& state, base::DictionaryValue* parent) {
  parent->SetString(kStatusKey, EnumToString(state.status));
  if (state.error == Error::kNone)
    return;
  parent->Set(kErrorKey,
              CreateError(state.error, state.error_message).release());
}

void ReturnErrorWithMessage(Error error,
                            const std::string& message,
                            const PrivetHandler::RequestCallback& callback) {
  std::unique_ptr<base::DictionaryValue> output = CreateError(error, message);
  callback.Run(chromeos::http::status_code::BadRequest, *output);
}

void ReturnError(Error error, const PrivetHandler::RequestCallback& callback) {
  ReturnErrorWithMessage(error, std::string(), callback);
}

void ReturnNotFound(const PrivetHandler::RequestCallback& callback) {
  callback.Run(chromeos::http::status_code::NotFound, base::DictionaryValue());
}

}  // namespace

PrivetHandler::PrivetHandler(CloudDelegate* cloud,
                             DeviceDelegate* device,
                             SecurityDelegate* security,
                             WifiDelegate* wifi)
    : cloud_(cloud), device_(device), security_(security), wifi_(wifi) {
  handlers_[kInfoApiPath] = std::make_pair(
      AuthScope::kGuest,
      base::Bind(&PrivetHandler::HandleInfo, base::Unretained(this)));
  handlers_[kPairingStartApiPath] = std::make_pair(
      AuthScope::kGuest,
      base::Bind(&PrivetHandler::HandlePairingStart, base::Unretained(this)));
  handlers_[kPairingConfirmApiPath] = std::make_pair(
      AuthScope::kGuest,
      base::Bind(&PrivetHandler::HandlePairingConfirm, base::Unretained(this)));
  handlers_[kAuthApiPath] = std::make_pair(
      AuthScope::kGuest,
      base::Bind(&PrivetHandler::HandleAuth, base::Unretained(this)));
  handlers_[kSetupStartApiPath] = std::make_pair(
      AuthScope::kOwner,
      base::Bind(&PrivetHandler::HandleSetupStart, base::Unretained(this)));
  handlers_[kSetupStatusApiPath] = std::make_pair(
      AuthScope::kOwner,
      base::Bind(&PrivetHandler::HandleSetupStatus, base::Unretained(this)));
}

PrivetHandler::~PrivetHandler() {
}

void PrivetHandler::HandleRequest(const std::string& api,
                                  const std::string& auth_header,
                                  const base::DictionaryValue* input,
                                  const RequestCallback& callback) {
  if (!input)
    return ReturnError(Error::kInvalidFormat, callback);
  auto handler = handlers_.find(api);
  if (handler == handlers_.end())
    return ReturnNotFound(callback);
  if (auth_header.empty())
    return ReturnError(Error::kMissingAuthorization, callback);
  std::string token = GetAuthTokenFromAuthHeader(auth_header);
  if (token.empty())
    return ReturnError(Error::kInvalidAuthorization, callback);
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
  if (scope == AuthScope::kNone)
    return ReturnError(Error::kInvalidAuthorization, callback);
  if (handler->second.first > scope)
    return ReturnError(Error::kInvalidAuthorizationScope, callback);
  handler->second.second.Run(*input, callback);
}

void PrivetHandler::HandleInfo(const base::DictionaryValue&,
                               const RequestCallback& callback) {
  base::DictionaryValue output;
  output.SetString(kInfoVersionKey, kInfoVersionValue);
  output.SetString(kInfoIdKey, device_->GetId());
  output.SetString(kNameKey, device_->GetName());

  std::string description = device_->GetDescription();
  if (!description.empty())
    output.SetString(kDescrptionKey, description);

  std::string location = device_->GetLocation();
  if (!location.empty())
    output.SetString(kLocationKey, location);

  output.SetString(kInfoClassKey, device_->GetClass());
  output.SetString(kInfoModelIdKey, device_->GetModelId());
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
  std::string pairing_str;
  input.GetString(kPairingKey, &pairing_str);

  std::string crypto_str;
  input.GetString(kCryptoKey, &crypto_str);

  PairingType pairing;
  std::vector<PairingType> modes = security_->GetPairingTypes();
  if (!StringToEnum(pairing_str, &pairing) ||
      std::find(modes.begin(), modes.end(), pairing) == modes.end()) {
    return ReturnError(Error::kInvalidParams, callback);
  }

  CryptoType crypto;
  std::vector<CryptoType> cryptos = security_->GetCryptoTypes();
  if (!StringToEnum(crypto_str, &crypto) ||
      std::find(cryptos.begin(), cryptos.end(), crypto) == cryptos.end()) {
    return ReturnError(Error::kInvalidParams, callback);
  }

  std::string id;
  std::string commitment;
  Error error = security_->StartPairing(pairing, crypto, &id, &commitment);
  if (error != Error::kNone)
    return ReturnError(error, callback);

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
  Error error =
      security_->ConfirmPairing(id, commitment, &fingerprint, &signature);
  if (error != Error::kNone)
    return ReturnError(error, callback);

  base::DictionaryValue output;
  output.SetString(kPairingFingerprintKey, fingerprint);
  output.SetString(kPairingSignatureKey, signature);
  callback.Run(chromeos::http::status_code::Ok, output);
}

void PrivetHandler::HandleAuth(const base::DictionaryValue& input,
                               const RequestCallback& callback) {
  std::string auth_code_type;
  input.GetString(kAuthModeKey, &auth_code_type);

  std::string auth_code;
  input.GetString(kAuthCodeKey, &auth_code);

  AuthScope max_auth_scope = AuthScope::kNone;
  if (auth_code_type == kAuthTypeAnonymousValue) {
    max_auth_scope = AuthScope::kGuest;
  } else if (auth_code_type == kAuthTypePairingValue) {
    if (!security_->IsValidPairingCode(auth_code))
      return ReturnError(Error::kInvalidAuthCode, callback);
    max_auth_scope = AuthScope::kOwner;
  } else {
    return ReturnError(Error::kInvalidAuthMode, callback);
  }

  std::string requested_scope;
  input.GetString(kAuthRequestedScopeKey, &requested_scope);

  AuthScope requested_auth_scope =
      AuthScopeFromString(requested_scope, max_auth_scope);
  if (requested_auth_scope == AuthScope::kNone)
    return ReturnError(Error::kInvalidRequestedScope, callback);

  if (requested_auth_scope > max_auth_scope)
    return ReturnError(Error::kAccessDenied, callback);

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

  const base::DictionaryValue* wifi = nullptr;
  if (input.GetDictionary(kWifiKey, &wifi)) {
    if (!wifi_ || wifi_->GetTypes().empty())
      return ReturnError(Error::kSetupUnavailable, callback);
    wifi->GetString(kSetupStartSsidKey, &ssid);
    if (ssid.empty())
      return ReturnError(Error::kInvalidParams, callback);
    wifi->GetString(kSetupStartPassKey, &passphrase);
  }

  const base::DictionaryValue* registration = nullptr;
  if (input.GetDictionary(kGcdKey, &registration)) {
    if (!cloud_)
      return ReturnError(Error::kSetupUnavailable, callback);
    registration->GetString(kSetupStartTicketIdKey, &ticket);
    if (ticket.empty())
      return ReturnError(Error::kInvalidParams, callback);
    registration->GetString(kSetupStartUserKey, &user);
  }

  if (!ssid.empty() && !wifi_->ConfigureCredentials(ssid, passphrase))
    return ReturnError(Error::kDeviceBusy, callback);

  if (!ticket.empty() && !cloud_->Setup(ticket, user))
    return ReturnError(Error::kDeviceBusy, callback);

  return HandleSetupStatus(input, callback);
}

void PrivetHandler::HandleSetupStatus(const base::DictionaryValue& input,
                                      const RequestCallback& callback) {
  base::DictionaryValue output;

  if (cloud_) {
    SetupState state = cloud_->GetSetupState();
    if (state.status != SetupState::kNone) {
      base::DictionaryValue* gcd = new base::DictionaryValue;
      output.Set(kGcdKey, gcd);
      SetState(state, gcd);
      if (state.status == SetupState::kSuccess)
        gcd->SetString(kInfoIdKey, cloud_->GetCloudId());
    }
  }

  if (wifi_) {
    SetupState state = wifi_->GetSetupState();
    if (state.status != SetupState::kNone) {
      base::DictionaryValue* wifi = new base::DictionaryValue;
      output.Set(kWifiKey, wifi);
      SetState(state, wifi);
      if (state.status == SetupState::kSuccess)
        wifi->SetString(kInfoWifiSsidKey, wifi_->GetCurrentlyConnectedSsid());
    }
  }

  callback.Run(chromeos::http::status_code::Ok, output);
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
  if (cloud_ && cloud_->GetConnectionState().status == ConnectionState::kOnline)
    auth_types->AppendString(kAuthTypeCloudValue);
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

  wifi->SetBoolean(kRequiredKey, wifi_->IsRequired());

  std::unique_ptr<base::ListValue> capabilities(new base::ListValue());
  for (WifiType type : wifi_->GetTypes())
    capabilities->AppendString(EnumToString(type));
  wifi->Set(kInfoWifiCapabilitiesKey, capabilities.release());

  wifi->SetString(kInfoWifiSsidKey, wifi_->GetCurrentlyConnectedSsid());

  std::string hosted_ssid = wifi_->GetHostedSsid();
  ConnectionState state = wifi_->GetConnectionState();
  if (!hosted_ssid.empty()) {
    DCHECK(state.status != ConnectionState::kDisabled);
    DCHECK(state.status != ConnectionState::kOnline);
    wifi->SetString(kInfoWifiHostedSsidKey, hosted_ssid);
  }
  SetState(state, wifi.get());
  return std::move(wifi);
}

std::unique_ptr<base::DictionaryValue> PrivetHandler::CreateGcdSection() const {
  std::unique_ptr<base::DictionaryValue> gcd(new base::DictionaryValue());
  gcd->SetBoolean(kRequiredKey, cloud_->IsRequired());
  gcd->SetString(kInfoIdKey, cloud_->GetCloudId());
  SetState(cloud_->GetConnectionState(), gcd.get());
  return std::move(gcd);
}

}  // namespace privetd
