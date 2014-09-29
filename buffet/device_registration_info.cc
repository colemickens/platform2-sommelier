// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/device_registration_info.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/json/json_writer.h>
#include <base/values.h>
#include <chromeos/data_encoding.h>
#include <chromeos/http/http_utils.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>
#include <chromeos/url_utils.h>

#include "buffet/commands/command_definition.h"
#include "buffet/commands/command_manager.h"
#include "buffet/device_registration_storage_keys.h"
#include "buffet/states/state_manager.h"
#include "buffet/storage_impls.h"
#include "buffet/utils.h"

const char buffet::kErrorDomainOAuth2[] = "oauth2";
const char buffet::kErrorDomainGCD[] = "gcd";
const char buffet::kErrorDomainGCDServer[] = "gcd_server";

namespace buffet {
namespace storage_keys {

// Persistent keys
const char kClientId[]      = "client_id";
const char kClientSecret[]  = "client_secret";
const char kApiKey[]        = "api_key";
const char kRefreshToken[]  = "refresh_token";
const char kDeviceId[]      = "device_id";
const char kOAuthURL[]      = "oauth_url";
const char kServiceURL[]    = "service_url";
const char kRobotAccount[]  = "robot_account";
// Transient keys
const char kDeviceKind[]    = "device_kind";
const char kSystemName[]    = "system_name";
const char kDisplayName[]   = "display_name";

}  // namespace storage_keys
}  // namespace buffet

namespace {

const base::FilePath::CharType kDeviceInfoFilePath[] =
    FILE_PATH_LITERAL("/var/lib/buffet/device_reg_info");

bool GetParamValue(
    const std::map<std::string, std::string>& params,
    const std::string& param_name,
    std::string* param_value) {
  auto p = params.find(param_name);
  if (p == params.end())
    return false;

  *param_value = p->second;
  return true;
}

std::pair<std::string, std::string> BuildAuthHeader(
    const std::string& access_token_type,
    const std::string& access_token) {
  std::string authorization =
      chromeos::string_utils::Join(' ', access_token_type, access_token);
  return {chromeos::http::request_header::kAuthorization, authorization};
}

std::unique_ptr<base::DictionaryValue> ParseOAuthResponse(
    const chromeos::http::Response* response, chromeos::ErrorPtr* error) {
  int code = 0;
  auto resp = chromeos::http::ParseJsonResponse(response, &code, error);
  if (resp && code >= chromeos::http::status_code::BadRequest) {
    if (error) {
      std::string error_code, error_message;
      if (resp->GetString("error", &error_code) &&
          resp->GetString("error_description", &error_message)) {
        chromeos::Error::AddTo(error, buffet::kErrorDomainOAuth2, error_code,
                               error_message);
      } else {
        chromeos::Error::AddTo(error, buffet::kErrorDomainOAuth2,
                               "unexpected_response", "Unexpected OAuth error");
      }
    }
    return std::unique_ptr<base::DictionaryValue>();
  }
  return resp;
}

inline void SetUnexpectedError(chromeos::ErrorPtr* error) {
  chromeos::Error::AddTo(error, buffet::kErrorDomainGCD, "unexpected_response",
                         "Unexpected GCD error");
}

void ParseGCDError(const base::DictionaryValue* json,
                   chromeos::ErrorPtr* error) {
  if (!error)
    return;

  const base::Value* list_value = nullptr;
  const base::ListValue* error_list = nullptr;
  if (!json->Get("error.errors", &list_value) ||
      !list_value->GetAsList(&error_list)) {
    SetUnexpectedError(error);
    return;
  }

  for (size_t i = 0; i < error_list->GetSize(); i++) {
    const base::Value* error_value = nullptr;
    const base::DictionaryValue* error_object = nullptr;
    if (!error_list->Get(i, &error_value) ||
        !error_value->GetAsDictionary(&error_object)) {
      SetUnexpectedError(error);
      continue;
    }
    std::string error_code, error_message;
    if (error_object->GetString("reason", &error_code) &&
        error_object->GetString("message", &error_message)) {
      chromeos::Error::AddTo(error, buffet::kErrorDomainGCDServer,
                             error_code, error_message);
    } else {
      SetUnexpectedError(error);
    }
  }
}

std::string BuildURL(const std::string& url,
                     const std::vector<std::string>& subpaths,
                     const chromeos::data_encoding::WebParamList& params) {
  std::string result = chromeos::url::CombineMultiple(url, subpaths);
  return chromeos::url::AppendQueryParams(result, params);
}

}  // anonymous namespace

namespace buffet {

DeviceRegistrationInfo::DeviceRegistrationInfo(
    const std::shared_ptr<CommandManager>& command_manager,
    const std::shared_ptr<const StateManager>& state_manager)
    : DeviceRegistrationInfo(
        command_manager,
        state_manager,
        chromeos::http::Transport::CreateDefault(),
        // TODO(avakulenko): Figure out security implications of storing
        // this data unencrypted.
        std::make_shared<FileStorage>(base::FilePath{kDeviceInfoFilePath})) {
}

DeviceRegistrationInfo::DeviceRegistrationInfo(
    const std::shared_ptr<CommandManager>& command_manager,
    const std::shared_ptr<const StateManager>& state_manager,
    const std::shared_ptr<chromeos::http::Transport>& transport,
    const std::shared_ptr<StorageInterface>& storage)
    : transport_{transport},
      storage_{storage},
      command_manager_{command_manager},
      state_manager_{state_manager} {
}

std::pair<std::string, std::string>
    DeviceRegistrationInfo::GetAuthorizationHeader() const {
  return BuildAuthHeader("Bearer", access_token_);
}

std::string DeviceRegistrationInfo::GetServiceURL(
    const std::string& subpath,
    const chromeos::data_encoding::WebParamList& params) const {
  return BuildURL(service_url_, {subpath}, params);
}

std::string DeviceRegistrationInfo::GetDeviceURL(
    const std::string& subpath,
    const chromeos::data_encoding::WebParamList& params) const {
  CHECK(!device_id_.empty()) << "Must have a valid device ID";
  return BuildURL(service_url_, {"devices", device_id_, subpath}, params);
}

std::string DeviceRegistrationInfo::GetOAuthURL(
    const std::string& subpath,
    const chromeos::data_encoding::WebParamList& params) const {
  return BuildURL(oauth_url_, {subpath}, params);
}

std::string DeviceRegistrationInfo::GetDeviceId(chromeos::ErrorPtr* error) {
  return CheckRegistration(error) ? device_id_ : std::string();
}

bool DeviceRegistrationInfo::Load() {
  auto value = storage_->Load();
  const base::DictionaryValue* dict = nullptr;
  if (!value || !value->GetAsDictionary(&dict))
    return false;

  // Get the values into temp variables first to make sure we can get
  // all the data correctly before changing the state of this object.
  std::string client_id;
  if (!dict->GetString(storage_keys::kClientId, &client_id))
    return false;
  std::string client_secret;
  if (!dict->GetString(storage_keys::kClientSecret, &client_secret))
    return false;
  std::string api_key;
  if (!dict->GetString(storage_keys::kApiKey, &api_key))
    return false;
  std::string refresh_token;
  if (!dict->GetString(storage_keys::kRefreshToken, &refresh_token))
    return false;
  std::string device_id;
  if (!dict->GetString(storage_keys::kDeviceId, &device_id))
    return false;
  std::string oauth_url;
  if (!dict->GetString(storage_keys::kOAuthURL, &oauth_url))
    return false;
  std::string service_url;
  if (!dict->GetString(storage_keys::kServiceURL, &service_url))
    return false;
  std::string device_robot_account;
  if (!dict->GetString(storage_keys::kRobotAccount, &device_robot_account))
    return false;

  client_id_            = client_id;
  client_secret_        = client_secret;
  api_key_              = api_key;
  refresh_token_        = refresh_token;
  device_id_            = device_id;
  oauth_url_            = oauth_url;
  service_url_          = service_url;
  device_robot_account_ = device_robot_account;
  return true;
}

bool DeviceRegistrationInfo::Save() const {
  base::DictionaryValue dict;
  dict.SetString(storage_keys::kClientId,     client_id_);
  dict.SetString(storage_keys::kClientSecret, client_secret_);
  dict.SetString(storage_keys::kApiKey,       api_key_);
  dict.SetString(storage_keys::kRefreshToken, refresh_token_);
  dict.SetString(storage_keys::kDeviceId,     device_id_);
  dict.SetString(storage_keys::kOAuthURL,     oauth_url_);
  dict.SetString(storage_keys::kServiceURL,   service_url_);
  dict.SetString(storage_keys::kRobotAccount, device_robot_account_);
  return storage_->Save(&dict);
}

bool DeviceRegistrationInfo::CheckRegistration(chromeos::ErrorPtr* error) {
  LOG(INFO) << "Checking device registration record.";
  if (refresh_token_.empty() ||
      device_id_.empty() ||
      device_robot_account_.empty()) {
    LOG(INFO) << "No valid device registration record found.";
    chromeos::Error::AddTo(error, kErrorDomainGCD, "device_not_registered",
                           "No valid device registration record found");
    return false;
  }

  LOG(INFO) << "Device registration record found.";
  return ValidateAndRefreshAccessToken(error);
}

bool DeviceRegistrationInfo::ValidateAndRefreshAccessToken(
    chromeos::ErrorPtr* error) {
  LOG(INFO) << "Checking access token expiration.";
  if (!access_token_.empty() &&
      !access_token_expiration_.is_null() &&
      access_token_expiration_ > base::Time::Now()) {
    LOG(INFO) << "Access token is still valid.";
    return true;
  }

  auto response = chromeos::http::PostFormData(GetOAuthURL("token"), {
    {"refresh_token", refresh_token_},
    {"client_id", client_id_},
    {"client_secret", client_secret_},
    {"grant_type", "refresh_token"},
  }, transport_, error);
  if (!response)
    return false;

  auto json = ParseOAuthResponse(response.get(), error);
  if (!json)
    return false;

  int expires_in = 0;
  if (!json->GetString("access_token", &access_token_) ||
      !json->GetInteger("expires_in", &expires_in) ||
      access_token_.empty() ||
      expires_in <= 0) {
    LOG(ERROR) << "Access token unavailable.";
    chromeos::Error::AddTo(error, kErrorDomainOAuth2,
                           "unexpected_server_response",
                           "Access token unavailable");
    return false;
  }

  access_token_expiration_ = base::Time::Now() +
                             base::TimeDelta::FromSeconds(expires_in);

  LOG(INFO) << "Access token is refreshed for additional " << expires_in
            << " seconds.";
  return true;
}

std::unique_ptr<base::Value> DeviceRegistrationInfo::GetDeviceInfo(
    chromeos::ErrorPtr* error) {
  if (!CheckRegistration(error))
    return std::unique_ptr<base::Value>();

  auto response = chromeos::http::Get(
      GetDeviceURL(), {GetAuthorizationHeader()}, transport_, error);
  int status_code = 0;
  std::unique_ptr<base::DictionaryValue> json =
      chromeos::http::ParseJsonResponse(response.get(), &status_code, error);
  if (json) {
    if (status_code >= chromeos::http::status_code::BadRequest) {
      LOG(WARNING) << "Failed to retrieve the device info. Response code = "
                   << status_code;
      ParseGCDError(json.get(), error);
      return std::unique_ptr<base::Value>();
    }
  }
  return std::unique_ptr<base::Value>(json.release());
}

bool CheckParam(const std::string& param_name,
                const std::string& param_value,
                chromeos::ErrorPtr* error) {
  if (!param_value.empty())
    return true;

  chromeos::Error::AddToPrintf(error, kErrorDomainBuffet, "missing_parameter",
                               "Parameter %s not specified",
                               param_name.c_str());
  return false;
}

std::string DeviceRegistrationInfo::StartRegistration(
    const std::map<std::string, std::string>& params,
    chromeos::ErrorPtr* error) {
  GetParamValue(params, "ticket_id", &ticket_id_);
  GetParamValue(params, storage_keys::kClientId, &client_id_);
  GetParamValue(params, storage_keys::kClientSecret, &client_secret_);
  GetParamValue(params, storage_keys::kApiKey, &api_key_);
  GetParamValue(params, storage_keys::kDeviceKind, &device_kind_);
  GetParamValue(params, storage_keys::kSystemName, &system_name_);
  GetParamValue(params, storage_keys::kDisplayName, &display_name_);
  GetParamValue(params, storage_keys::kOAuthURL, &oauth_url_);
  GetParamValue(params, storage_keys::kServiceURL, &service_url_);

  std::unique_ptr<base::DictionaryValue> commands =
      command_manager_->GetCommandDictionary().GetCommandsAsJson(true, error);
  if (!commands)
    return std::string();

  std::unique_ptr<base::DictionaryValue> state =
      state_manager_->GetStateValuesAsJson(error);
  if (!state)
    return std::string();

  base::DictionaryValue req_json;
  req_json.SetString("id", ticket_id_);
  req_json.SetString("oauthClientId", client_id_);
  req_json.SetString("deviceDraft.deviceKind", device_kind_);
  req_json.SetString("deviceDraft.systemName", system_name_);
  if (!display_name_.empty())
    req_json.SetString("deviceDraft.displayName", display_name_);
  req_json.SetString("deviceDraft.channel.supportedType", "xmpp");
  req_json.Set("deviceDraft.commandDefs", commands.release());
  req_json.Set("deviceDraft.state", state.release());

  auto url = GetServiceURL("registrationTickets/" + ticket_id_,
                           {{"key", api_key_}});
  std::unique_ptr<chromeos::http::Response> response =
      chromeos::http::PatchJson(url, &req_json, transport_, error);
  auto json_resp = chromeos::http::ParseJsonResponse(response.get(), nullptr,
                                                     error);
  if (!json_resp)
    return std::string();
  if (!response->IsSuccessful())
    return std::string();

  std::string auth_url = GetOAuthURL("auth", {
    {"scope", "https://www.googleapis.com/auth/clouddevices"},
    {"redirect_uri", "urn:ietf:wg:oauth:2.0:oob"},
    {"response_type", "code"},
    {"client_id", client_id_}
  });

  base::DictionaryValue json;
  json.SetString("ticket_id", ticket_id_);
  json.SetString("auth_url", auth_url);

  std::string ret;
  base::JSONWriter::Write(&json, &ret);
  return ret;
}

bool DeviceRegistrationInfo::FinishRegistration(chromeos::ErrorPtr* error) {
  if (ticket_id_.empty()) {
    LOG(ERROR) << "Finish registration without ticket ID";
    chromeos::Error::AddTo(error, kErrorDomainBuffet,
                           "registration_not_started",
                           "Device registration not started");
    return false;
  }

  std::string url = GetServiceURL("registrationTickets/" + ticket_id_ +
                                  "/finalize?key=" + api_key_);
  std::unique_ptr<chromeos::http::Response> response =
      chromeos::http::PostBinary(url, nullptr, 0, transport_, error);
  if (!response)
    return false;
  auto json_resp = chromeos::http::ParseJsonResponse(response.get(), nullptr,
                                                     error);
  if (!json_resp)
    return false;
  if (!response->IsSuccessful()) {
    ParseGCDError(json_resp.get(), error);
    return false;
  }

  std::string auth_code;
  if (!json_resp->GetString("robotAccountEmail", &device_robot_account_) ||
      !json_resp->GetString("robotAccountAuthorizationCode", &auth_code) ||
      !json_resp->GetString("deviceDraft.id", &device_id_)) {
    chromeos::Error::AddTo(error, kErrorDomainGCD, "unexpected_response",
                           "Device account missing in response");
    return false;
  }

  // Now get access_token and refresh_token
  response = chromeos::http::PostFormData(GetOAuthURL("token"), {
    {"code", auth_code},
    {"client_id", client_id_},
    {"client_secret", client_secret_},
    {"redirect_uri", "oob"},
    {"scope", "https://www.googleapis.com/auth/clouddevices"},
    {"grant_type", "authorization_code"}
  }, transport_, error);
  if (!response)
    return false;

  json_resp = ParseOAuthResponse(response.get(), error);
  int expires_in = 0;
  if (!json_resp ||
      !json_resp->GetString("access_token", &access_token_) ||
      !json_resp->GetString("refresh_token", &refresh_token_) ||
      !json_resp->GetInteger("expires_in", &expires_in) ||
      access_token_.empty() ||
      refresh_token_.empty() ||
      expires_in <= 0) {
    chromeos::Error::AddTo(error, kErrorDomainGCD, "unexpected_response",
                           "Device access_token missing in response");
    return false;
  }

  access_token_expiration_ = base::Time::Now() +
                             base::TimeDelta::FromSeconds(expires_in);

  Save();
  return true;
}

}  // namespace buffet
