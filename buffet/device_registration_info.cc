// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/device_registration_info.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/json/json_writer.h>
#include <base/message_loop/message_loop.h>
#include <base/values.h>
#include <chromeos/bind_lambda.h>
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
const char kName[]          = "name";
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

std::unique_ptr<base::DictionaryValue>
DeviceRegistrationInfo::BuildDeviceResource(chromeos::ErrorPtr* error) {
  std::unique_ptr<base::DictionaryValue> commands =
      command_manager_->GetCommandDictionary().GetCommandsAsJson(true, error);
  if (!commands)
    return nullptr;

  std::unique_ptr<base::DictionaryValue> state =
      state_manager_->GetStateValuesAsJson(error);
  if (!state)
    return nullptr;

  std::unique_ptr<base::DictionaryValue> resource{new base::DictionaryValue};
  if (!device_id_.empty())
    resource->SetString("id", device_id_);
  resource->SetString("deviceKind", device_kind_);
  resource->SetString("name", name_);
  if (!display_name_.empty())
    resource->SetString("displayName", display_name_);
  resource->SetString("channel.supportedType", "xmpp");
  resource->Set("commandDefs", commands.release());
  resource->Set("state", state.release());

  return resource;
}

std::unique_ptr<base::Value> DeviceRegistrationInfo::GetDeviceInfo(
    chromeos::ErrorPtr* error) {
  if (!CheckRegistration(error))
    return std::unique_ptr<base::Value>();

  // TODO(antonm): Switch to DoCloudRequest later.
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

std::string DeviceRegistrationInfo::RegisterDevice(
    const std::map<std::string, std::string>& params,
    chromeos::ErrorPtr* error) {
  GetParamValue(params, "ticket_id", &ticket_id_);
  GetParamValue(params, storage_keys::kClientId, &client_id_);
  GetParamValue(params, storage_keys::kClientSecret, &client_secret_);
  GetParamValue(params, storage_keys::kApiKey, &api_key_);
  GetParamValue(params, storage_keys::kDeviceKind, &device_kind_);
  GetParamValue(params, storage_keys::kName, &name_);
  GetParamValue(params, storage_keys::kDisplayName, &display_name_);
  GetParamValue(params, storage_keys::kOAuthURL, &oauth_url_);
  GetParamValue(params, storage_keys::kServiceURL, &service_url_);

  std::unique_ptr<base::DictionaryValue> device_draft =
      BuildDeviceResource(error);
  if (!device_draft)
    return std::string();

  base::DictionaryValue req_json;
  req_json.SetString("id", ticket_id_);
  req_json.SetString("oauthClientId", client_id_);
  req_json.Set("deviceDraft", device_draft.release());

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

  url = GetServiceURL("registrationTickets/" + ticket_id_ +
                      "/finalize?key=" + api_key_);
  response = chromeos::http::PostBinary(url, nullptr, 0, transport_, error);
  if (!response)
    return std::string();
  json_resp = chromeos::http::ParseJsonResponse(response.get(), nullptr, error);
  if (!json_resp)
    return std::string();
  if (!response->IsSuccessful()) {
    ParseGCDError(json_resp.get(), error);
    return std::string();
  }

  std::string auth_code;
  if (!json_resp->GetString("robotAccountEmail", &device_robot_account_) ||
      !json_resp->GetString("robotAccountAuthorizationCode", &auth_code) ||
      !json_resp->GetString("deviceDraft.id", &device_id_)) {
    chromeos::Error::AddTo(error, kErrorDomainGCD, "unexpected_response",
                           "Device account missing in response");
    return std::string();
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
    return std::string();

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
    return std::string();
  }

  access_token_expiration_ = base::Time::Now() +
                             base::TimeDelta::FromSeconds(expires_in);

  Save();
  return device_id_;
}

namespace {

template <class T>
void PostToCallback(base::Callback<void(const T&)> callback,
                    std::unique_ptr<T> value) {
  auto cb = [callback] (T* result) {
    callback.Run(*result);
  };
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(cb, base::Owned(value.release())));
}

// TODO(antonm): May belong to chromeos/http.

void SendRequestAsync(
    const std::string& method,
    const std::string& url,
    const std::string& data,
    const std::string& mime_type,
    const chromeos::http::HeaderList& headers,
    std::shared_ptr<chromeos::http::Transport> transport,
    int num_retries,
    base::Callback<void(const chromeos::http::Response&)> callback,
    base::Callback<void(const chromeos::Error&)> errorback) {
  chromeos::ErrorPtr error;

  auto on_retriable_failure = [&error, method, url, data, mime_type,
       headers, transport, num_retries, callback, errorback] () {
    if (num_retries > 0) {
      auto c = [method, url, data, mime_type, headers, transport,
            num_retries, callback, errorback] () {
        SendRequestAsync(method, url,
                         data, mime_type,
                         headers,
                         transport,
                         num_retries - 1,
                         callback, errorback);
      };
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(c));
    } else {
      PostToCallback(errorback, std::move(error));
    }
  };

  chromeos::http::Request request(url, method.c_str(), transport);
  request.AddHeaders(headers);
  if (!data.empty()) {
    request.SetContentType(mime_type.c_str());
    if (!request.AddRequestBody(data.c_str(), data.size(), &error)) {
      on_retriable_failure();
      return;
    }
  }

  std::unique_ptr<chromeos::http::Response> response{
    request.GetResponse(&error)};
  if (!response) {
    on_retriable_failure();
    return;
  }

  int status_code{response->GetStatusCode()};
  if (status_code >= chromeos::http::status_code::Continue &&
      status_code < chromeos::http::status_code::BadRequest) {
    PostToCallback(callback, std::move(response));
    return;
  }

  // TODO(antonm): Should add some useful information to error.
  LOG(WARNING) << "Request failed. Response code = " << status_code;

  if (status_code >= 500 && status_code < 600) {
    // Request was valid, but server failed, retry.
    // TODO(antonm): Implement exponential backoff.
    // TODO(antonm): Reconsider status codes, maybe only some require
    // retry.
    // TODO(antonm): Support Retry-After header.
    on_retriable_failure();
  } else {
    PostToCallback(errorback, std::move(error));
  }
}

}  // namespace

void DeviceRegistrationInfo::DoCloudRequest(
    const char* method,
    const std::string& url,
    const base::DictionaryValue* body,
    CloudRequestCallback callback,
    CloudRequestErroback errorback) {
  // TODO(antonm): Add reauthorisation on access token expiration (do not
  // forget about 5xx when fetching new access token).
  // TODO(antonm): Add support for device removal.

  std::string data;
  if (body)
    base::JSONWriter::Write(body, &data);

  const std::string mime_type{chromeos::mime::AppendParameter(
      chromeos::mime::application::kJson,
      chromeos::mime::parameters::kCharset,
      "utf-8")};

  auto request_cb = [callback, errorback] (
      const chromeos::http::Response& response) {
    chromeos::ErrorPtr error;

    std::unique_ptr<base::DictionaryValue> json_resp{
        chromeos::http::ParseJsonResponse(&response, nullptr, &error)};
    if (!json_resp) {
      PostToCallback(errorback, std::move(error));
      return;
    }

    PostToCallback(callback, std::move(json_resp));
  };

  SendRequestAsync(method, url,
                   data, mime_type,
                   {GetAuthorizationHeader()},
                   transport_,
                   7,
                   base::Bind(request_cb), errorback);
}

void DeviceRegistrationInfo::StartDevice(chromeos::ErrorPtr* error) {
  if (!CheckRegistration(error))
    return;

  std::unique_ptr<base::DictionaryValue> device_resource =
      BuildDeviceResource(error);
  if (!device_resource)
    return;

  auto std_errorback = base::Bind([](const chromeos::Error& error) {});

  const std::string device_url{GetDeviceURL()};
  auto update_device_resource = [device_url, std_errorback]
      (DeviceRegistrationInfo* self, base::DictionaryValue* device_resource,
       CloudRequestCallback callback) {
    self->DoCloudRequest(
        chromeos::http::request_type::kPut,
        device_url,
        device_resource,
        // TODO(antonm): Failure to update device resource probably deserves
        // some additional actions.
        callback, std_errorback);
  };

  const std::string command_queue_url{
    GetServiceURL("commands/queue", {{"deviceId", device_id_}})};
  auto fetch_commands_cb = [command_queue_url, std_errorback]
      (DeviceRegistrationInfo* self,
       CloudRequestCallback callback, const base::DictionaryValue&) {
    self->DoCloudRequest(chromeos::http::request_type::kGet,
                         command_queue_url,
                         nullptr,
                         callback, std_errorback);
  };

  auto abort_commands_cb = [] (const base::DictionaryValue& json) {
    const base::ListValue* commands{nullptr};
    if (json.GetList("commands", &commands)) {
      const size_t size{commands->GetSize()};
      for (size_t i = 0; i < size; ++i) {
        const base::DictionaryValue* command{nullptr};
        if (!commands->GetDictionary(i, &command)) {
          LOG(WARNING) << "No command resource at " << i;
          continue;
        }
        std::string command_state;
        if (!command->GetString("state", &command_state)) {
          LOG(WARNING) << "Command with no state at " << i;
          continue;
        }
        if (command_state != "error" &&
            command_state != "inProgress" &&
            command_state != "paused") {
          // It's not a limbo command, ignore.
          continue;
        }
        std::string command_id;
        if (!command->GetString("id", &command_id)) {
          LOG(WARNING) << "Command with no ID at " << i;
          continue;
        }
        // TODO(antonm): Really abort the command.
      }
    }
  };

  base::Bind(update_device_resource,
             base::Unretained(this),
             base::Owned(device_resource.release()),
             base::Bind(fetch_commands_cb,
                        base::Unretained(this),
                        base::Bind(abort_commands_cb))).Run();


  // TODO(antonm): Implement the rest of startup sequence:
  //   * Poll for commands to run
  //   * Schedule periodic polling
}

}  // namespace buffet
