// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/privet_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <chromeos/strings/string_utils.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/security_delegate.h"
#include "privetd/wifi_delegate.h"

namespace privetd {

namespace {

const char kInfoApiPath[] = "/privet/info";
const char kAuthApiPath[] = "/privet/v3/auth";
const char kSetupStartApiPath[] = "/privet/v3/setup/start";
const char kSetupStatusApiPath[] = "/privet/v3/setup/status";

const char kInfoVersionKey[] = "version";
const char kInfoVersionValue[] = "3.0";

const char kNameKey[] = "name";
const char kDescrptionKey[] = "description";
const char kLocationKey[] = "location";

const char kRegistrationKey[] = "registration";
const char kWifiKey[] = "wifi";

const char kInfoIdKey[] = "id";
const char kInfoTypeKey[] = "type";

const char kInfoAuthenticationKey[] = "authentication";

const char kInfoCloudIdKey[] = "cloudId";

const char kInfoCloudConnectionKey[] = "cloudConnection";
const char kInfoCloudConnectingValue[] = "connecting";
const char kInfoCloudOnlineValue[] = "online";
const char kInfoCloudOfflineValue[] = "offline";
const char kInfoCloudUnconfiguredValue[] = "unconfigured";
const char kInfoCloudDisabledValue[] = "disabled";

const char kInfoWirelessKey[] = "capabilities.wireless";
const char kInfoWirelessWifi24Value[] = "wifi2.4";
const char kInfoWirelessWifi25Value[] = "wifi5.0";

const char kInfoPairingKey[] = "capabilities.pairing";
const char kInfoPairingPinCodeValue[] = "pinCode";
const char kInfoPairingEmbeddedCodeValue[] = "embeddedCode";

const char kInfoEndpointsKey[] = "endpoints";
const char kInfoEndpointsHttpPortKey[] = "httpPort";
const char kInfoEndpointsHttpUpdatePortKey[] = "httpUpdatesPort";
const char kInfoEndpointsHttpsPortKey[] = "httpsPort";
const char kInfoEndpointsHttpsUpdatePortKey[] = "httpsUpdatesPort";

const char kInfoWifiSsidKey[] = "wifiSSID";
const char kInfoUptimeKey[] = "uptime";
const char kInfoApiKey[] = "api";

const char kAuthCodeTypeKey[] = "authCodeType";
const char kAuthTypeAnonymousValue[] = "anonymous";
const char kAuthTypePairingValue[] = "pairing";
const char kAuthTypeCloudValue[] = "cloud";

const char kAuthCodeKey[] = "authCode";
const char kAuthRequestedScopeKey[] = "requestedScope";
const char kAuthScopeAutoValue[] = "auto";
const char kAuthScopeOwnerValue[] = "owner";
const char kAuthScopeUserValue[] = "user";
const char kAuthScopeViewerValue[] = "viewer";
const char kAuthScopeGuestValue[] = "guest";

const char kAuthAccessTokenKey[] = "accessToken";
const char kAuthTokenTypeKey[] = "tokenType";
const char kAuthExpiresInKey[] = "expiresIn";
const char kAuthScopeKey[] = "scope";

const char kAuthorizationHeaderPrefix[] = "Privet";
const char kErrorReasonKey[] = "reason";
const char kErrorMessageKey[] = "message";

const char kSetupStateRequiredKey[] = "required";
const char kSetupStateStatusKey[] = "status";
const char kSetupStateIdKey[] = "id";
const char kSetupStateSsidKey[] = "ssid";
const char kSetupStatusErrorPath[] = "error.reason";

const char kSetupStartSsidKey[] = "ssid";
const char kSetupStartPassKey[] = "passphrase";
const char kSetupStartTicketIdKey[] = "ticketID";
const char kSetupStartUserKey[] = "user";

const char kSetupStatusAvailableValue[] = "available";
const char kSetupStatusCompleteValue[] = "complete";
const char kSetupStatusInProgressValue[] = "inProgress";
const char kSetupStatusErrorValue[] = "error";

const char kSetupErrorInvalidTicketValue[] = "invalidTicket";
const char kSetupErrorServerErrorValue[] = "serverError";
const char kSetupErrorOfflineValue[] = "offline";
const char kSetupErrorDeviceConfigErrorValue[] = "deviceConfigError";
const char kSetupErrorUnknownSsidValue[] = "unknownSsid";
const char kSetupErrorInvalidPassphraseValue[] = "invalidPassphrase";

// Errors
const char kMissingAuthorization[] = "missingAuthorization";
const char kInvalidAuthorization[] = "invalidAuthorization";
const char kInvalidAuthorizationScope[] = "invalidAuthorizationScope";
const char kInvalidAuthCode[] = "invalidAuthCode";
const char kInvalidAuthCodeType[] = "invalidAuthCodeType";
const char kInvalidRequestedScope[] = "invalidRequestedScope";
const char kAccessDenied[] = "accessDenied";
const char kInvalidParams[] = "invalidParams";
const char kSetupUnavailable[] = "setupUnavailable";
const char kDeviceBusy[] = "deviceBusy";

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

std::string ConnectionTypeToString(CloudState state) {
  switch (state) {
    case CloudState::kConnecting:
      return kInfoCloudConnectingValue;
    case CloudState::kOnline:
      return kInfoCloudOnlineValue;
    case CloudState::kOffline:
      return kInfoCloudOfflineValue;
    case CloudState::kUnconfigured:
      return kInfoCloudUnconfiguredValue;
    case CloudState::kDisabled:
      return kInfoCloudDisabledValue;
  }
  NOTREACHED();
  return kInfoCloudUnconfiguredValue;
}

std::string WifiTypeToString(WifiType type) {
  switch (type) {
    case WifiType::kWifi24:
      return kInfoWirelessWifi24Value;
    case WifiType::kWifi50:
      return kInfoWirelessWifi25Value;
  }
  NOTREACHED();
  return std::string();
}

std::string PairingTypeToString(PairingType type) {
  switch (type) {
    case PairingType::kPinCode:
      return kInfoPairingPinCodeValue;
    case PairingType::kEmbeddedCode:
      return kInfoPairingEmbeddedCodeValue;
  }
  NOTREACHED();
  return std::string();
}

AuthScope AuthScopeFromString(const std::string& scope, AuthScope auto_scope) {
  if (scope == kAuthScopeAutoValue)
    return auto_scope;
  if (scope == kAuthScopeOwnerValue)
    return AuthScope::kOwner;
  if (scope == kAuthScopeUserValue)
    return AuthScope::kUser;
  if (scope == kAuthScopeViewerValue)
    return AuthScope::kViewer;
  if (scope == kAuthScopeGuestValue)
    return AuthScope::kGuest;
  return AuthScope::kNone;
}

std::string AuthScopeToString(AuthScope scope) {
  switch (scope) {
    case AuthScope::kOwner:
      return kAuthScopeOwnerValue;
    case AuthScope::kUser:
      return kAuthScopeUserValue;
    case AuthScope::kViewer:
      return kAuthScopeViewerValue;
    case AuthScope::kGuest:
      return kAuthScopeGuestValue;
    default:
      return std::string();
  }
}

std::string GetAuthTokenFromAuthHeader(const std::string& auth_header) {
  std::string name;
  std::string value;
  chromeos::string_utils::SplitAtFirst(auth_header, ' ', &name, &value);
  return value;
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
  handlers_[kAuthApiPath] = std::make_pair(
      AuthScope::kGuest,
      base::Bind(&PrivetHandler::HandleAuth, base::Unretained(this)));
  handlers_[kSetupStartApiPath] = std::make_pair(
      AuthScope::kOwner,
      base::Bind(&PrivetHandler::HandleSetupStart, base::Unretained(this)));
  handlers_[kSetupStatusApiPath] = std::make_pair(
      AuthScope::kViewer,
      base::Bind(&PrivetHandler::HandleSetupStatus, base::Unretained(this)));
}

PrivetHandler::~PrivetHandler() {
}

bool PrivetHandler::HandleRequest(const std::string& api,
                                  const std::string& auth_header,
                                  const base::DictionaryValue& input,
                                  const RequestCallback& callback) {
  auto handler = handlers_.find(api);
  if (handler == handlers_.end())
    return false;
  if (auth_header.empty())
    return ReturnError(kMissingAuthorization, callback);
  std::string token = GetAuthTokenFromAuthHeader(auth_header);
  if (token.empty())
    return ReturnError(kInvalidAuthorization, callback);
  AuthScope scope = AuthScope::kNone;
  if (token == kAuthTypeAnonymousValue) {
    scope = AuthScope::kGuest;
  } else {
    base::Time expiration =
        base::Time::Now() -
        base::TimeDelta::FromSeconds(kAccesssTokenExpirationSeconds +
                                     kAccesssTokenExpirationThresholdSeconds);
    scope = security_->GetScopeFromAccessToken(token, expiration);
  }
  if (scope == AuthScope::kNone)
    return ReturnError(kInvalidAuthorization, callback);
  if (handler->second.first > scope)
    return ReturnError(kInvalidAuthorizationScope, callback);
  return handler->second.second.Run(input, callback);
}

bool PrivetHandler::ReturnError(const std::string& error,
                                const RequestCallback& callback) const {
  return ReturnErrorWithMessage(error, std::string(), callback);
}

bool PrivetHandler::ReturnErrorWithMessage(
    const std::string& error,
    const std::string& message,
    const RequestCallback& callback) const {
  base::DictionaryValue output;
  output.SetString(kErrorReasonKey, error);
  if (!message.empty())
    output.SetString(kErrorMessageKey, message);
  callback.Run(output, false);
  return true;
}

bool PrivetHandler::HandleInfo(const base::DictionaryValue&,
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

  output.Set(kInfoTypeKey, ToValue(device_->GetTypes()).release());

  std::unique_ptr<base::ListValue> auth_types(new base::ListValue());
  auth_types->AppendString(kAuthTypeAnonymousValue);
  auth_types->AppendString(kAuthTypePairingValue);
  if (cloud_->GetConnectionState() == CloudState::kOnline)
    auth_types->AppendString(kAuthTypeCloudValue);
  output.Set(kInfoAuthenticationKey, auth_types.release());

  output.SetString(kInfoCloudIdKey, cloud_->GetCloudId());

  output.SetString(kInfoCloudConnectionKey,
                   ConnectionTypeToString(cloud_->GetConnectionState()));

  std::unique_ptr<base::ListValue> networking_types(new base::ListValue());
  for (WifiType type : wifi_->GetWifiTypes())
    networking_types->AppendString(WifiTypeToString(type));
  output.Set(kInfoWirelessKey, networking_types.release());

  std::unique_ptr<base::ListValue> pairing_types(new base::ListValue());
  for (PairingType type : security_->GetPairingTypes())
    pairing_types->AppendString(PairingTypeToString(type));
  output.Set(kInfoPairingKey, pairing_types.release());

  base::DictionaryValue* endpoints = new base::DictionaryValue();
  output.Set(kInfoEndpointsKey, endpoints);
  std::pair<int, int> http_endpoint = device_->GetHttpEnpoint();
  if (http_endpoint.first > 0) {
    endpoints->SetInteger(kInfoEndpointsHttpPortKey, http_endpoint.first);
    if (http_endpoint.second > 0) {
      endpoints->SetInteger(kInfoEndpointsHttpUpdatePortKey,
                            http_endpoint.second);
    }
  }

  std::pair<int, int> https_endpoint = device_->GetHttpsEnpoint();
  if (https_endpoint.first > 0) {
    endpoints->SetInteger(kInfoEndpointsHttpsPortKey, https_endpoint.first);
    if (https_endpoint.second > 0) {
      endpoints->SetInteger(kInfoEndpointsHttpsUpdatePortKey,
                            https_endpoint.second);
    }
  }

  std::string ssid = wifi_->GetWifiSsid();
  output.SetString(kInfoWifiSsidKey, ssid);
  output.SetInteger(kInfoUptimeKey, device_->GetUptime().InSeconds());

  std::unique_ptr<base::ListValue> apis(new base::ListValue());
  for (const auto& key_value : handlers_)
    apis->AppendString(key_value.first);
  output.Set(kInfoApiKey, apis.release());

  callback.Run(output, true);
  return true;
}

bool PrivetHandler::HandleAuth(const base::DictionaryValue& input,
                               const RequestCallback& callback) {
  std::string auth_code_type;
  input.GetString(kAuthCodeTypeKey, &auth_code_type);

  std::string auth_code;
  input.GetString(kAuthCodeKey, &auth_code);

  AuthScope max_auth_scope = AuthScope::kNone;
  if (auth_code_type == kAuthTypeAnonymousValue) {
    // TODO(vitalybuka): Change to kAnonymous when pairing is implemented.
    max_auth_scope = AuthScope::kOwner;
  } else if (auth_code_type == kAuthTypePairingValue) {
    if (!security_->IsValidPairingCode(auth_code))
      return ReturnError(kInvalidAuthCode, callback);
    max_auth_scope = AuthScope::kOwner;
  } else {
    return ReturnError(kInvalidAuthCodeType, callback);
  }

  std::string requested_scope;
  input.GetString(kAuthRequestedScopeKey, &requested_scope);

  AuthScope requested_auth_scope =
      AuthScopeFromString(requested_scope, max_auth_scope);
  if (requested_auth_scope == AuthScope::kNone)
    return ReturnError(kInvalidRequestedScope, callback);

  if (requested_auth_scope > max_auth_scope)
    return ReturnError(kAccessDenied, callback);

  base::DictionaryValue output;
  output.SetString(kAuthAccessTokenKey,
                   security_->CreateAccessToken(requested_auth_scope));
  output.SetString(kAuthTokenTypeKey, kAuthorizationHeaderPrefix);
  output.SetInteger(kAuthExpiresInKey, kAccesssTokenExpirationSeconds);
  output.SetString(kAuthScopeKey, AuthScopeToString(requested_auth_scope));
  callback.Run(output, true);
  return true;
}

bool PrivetHandler::HandleSetupStart(const base::DictionaryValue& input,
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
    if (wifi_->GetWifiTypes().empty())
      return ReturnError(kSetupUnavailable, callback);
    wifi->GetString(kSetupStartSsidKey, &ssid);
    if (ssid.empty())
      return ReturnError(kInvalidParams, callback);
    wifi->GetString(kSetupStartPassKey, &passphrase);
  }

  const base::DictionaryValue* registration = nullptr;
  if (input.GetDictionary(kRegistrationKey, &registration)) {
    if (cloud_->GetRegistrationState() != RegistrationState::kAvalible)
      return ReturnError(kSetupUnavailable, callback);
    registration->GetString(kSetupStartTicketIdKey, &ticket);
    if (ticket.empty())
      return ReturnError(kInvalidParams, callback);
    registration->GetString(kSetupStartUserKey, &user);
  }

  if (!ssid.empty() && !wifi_->SetupWifi(ssid, passphrase))
    return ReturnError(kDeviceBusy, callback);

  if (!ticket.empty() && !cloud_->RegisterDevice(ticket, user))
    return ReturnError(kDeviceBusy, callback);

  return HandleSetupStatus(input, callback);
}

bool PrivetHandler::HandleSetupStatus(const base::DictionaryValue& input,
                                      const RequestCallback& callback) {
  base::DictionaryValue output;

  if (cloud_->GetConnectionState() != CloudState::kDisabled) {
    base::DictionaryValue* registration = new base::DictionaryValue;
    output.Set(kRegistrationKey, registration);

    registration->SetBoolean(kSetupStateRequiredKey,
                             cloud_->IsRegistrationRequired());

    std::string status = kSetupStatusErrorValue;
    std::string error;
    switch (cloud_->GetRegistrationState()) {
      case RegistrationState::kAvalible:
        status = kSetupStatusAvailableValue;
        break;
      case RegistrationState::kCompleted:
        status = kSetupStatusCompleteValue;
        registration->SetString(kSetupStateIdKey, cloud_->GetCloudId());
        break;
      case RegistrationState::kInProgress:
        status = kSetupStatusInProgressValue;
        break;
      case RegistrationState::kInvalidTicket:
        error = kSetupErrorInvalidTicketValue;
        break;
      case RegistrationState::kServerError:
        error = kSetupErrorServerErrorValue;
        break;
      case RegistrationState::kOffline:
        error = kSetupErrorOfflineValue;
        break;
      case RegistrationState::kDeviceConfigError:
        error = kSetupErrorDeviceConfigErrorValue;
        break;
    }
    registration->SetString(kSetupStateStatusKey, status);
    if (!error.empty())
      registration->SetString(kSetupStatusErrorPath, error);
  }

  if (!wifi_->GetWifiTypes().empty()) {
    base::DictionaryValue* wifi = new base::DictionaryValue();
    output.Set(kWifiKey, wifi);
    wifi->SetBoolean(kSetupStateRequiredKey, wifi_->IsWifiRequired());

    std::string status = kSetupStatusErrorValue;
    std::string error;
    switch (wifi_->GetWifiSetupState()) {
      case WifiSetupState::kAvalible:
        status = kSetupStatusAvailableValue;
        break;
      case WifiSetupState::kCompleted:
        status = kSetupStatusCompleteValue;
        wifi->SetString(kSetupStateSsidKey, wifi_->GetWifiSsid());
        break;
      case WifiSetupState::kInProgress:
        status = kSetupStatusInProgressValue;
        break;
      case WifiSetupState::kInvalidSsid:
        error = kSetupErrorUnknownSsidValue;
        break;
      case WifiSetupState::kInvalidPassword:
        error = kSetupErrorInvalidPassphraseValue;
        break;
    }
    wifi->SetString(kSetupStateStatusKey, status);
    if (!error.empty())
      wifi->SetString(kSetupStatusErrorPath, error);
  }
  callback.Run(output, true);
  return true;
}

}  // namespace privetd
