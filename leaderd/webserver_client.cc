// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/webserver_client.h"

#include <vector>

#include <base/json/json_reader.h>
#include <base/logging.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>

using base::Value;
using chromeos::dbus_utils::AsyncEventSequencer;
using libwebserv::ProtocolHandler;
using libwebserv::Request;
using libwebserv::Response;

namespace leaderd {
namespace http_api {

const char kDiscoverGroupKey[] = "group";
const char kDiscoverLeaderKey[] = "leader";

const char kChallengeScoreKey[] = "score";
const char kChallengeGroupKey[] = "group";
const char kChallengeIdKey[] = "uuid";
const char kChallengeLeaderKey[] = "leader";

const char kAnnounceGroupKey[] = "group";
const char kAnnounceLeaderIdKey[] = "my_uuid";
const char kAnnounceScoreKey[] = "score";

}  // namespace http_api

WebServerClient::WebServerClient(Delegate* delegate,
                                 const std::string& web_handler_name)
    : delegate_(delegate),
      protocol_handler_name_{web_handler_name} {}

void WebServerClient::RegisterAsync(const scoped_refptr<dbus::Bus>& bus,
                                    const std::string& leaderd_service_name,
                                    AsyncEventSequencer* sequencer) {
  web_server_.OnProtocolHandlerConnected(
      base::Bind(&WebServerClient::OnProtocolHandlerConnected,
                 weak_ptr_factory_.GetWeakPtr()));
  web_server_.OnProtocolHandlerDisconnected(
      base::Bind(&WebServerClient::OnProtocolHandlerDisconnected,
                 weak_ptr_factory_.GetWeakPtr()));

  web_server_.Connect(
      bus, leaderd_service_name,
      sequencer->GetHandler("Server::Connect failed.", true),
      base::Bind(&base::DoNothing), base::Bind(&base::DoNothing));

  // Wanderers will try to discover the leader.
  web_server_.GetDefaultHttpHandler()->AddHandlerCallback(
      "/privet/v3/leadership/discover", chromeos::http::request_type::kPost,
      base::Bind(&WebServerClient::RequestHandler,
                 weak_ptr_factory_.GetWeakPtr(),
                 QueryType::DISCOVER));
  // Followers will try to challenge the leader.
  web_server_.GetDefaultHttpHandler()->AddHandlerCallback(
      "/privet/v3/leadership/challenge", chromeos::http::request_type::kPost,
      base::Bind(&WebServerClient::RequestHandler,
                 weak_ptr_factory_.GetWeakPtr(),
                 QueryType::CHALLENGE));
  // Leaders announce their presence.
  web_server_.GetDefaultHttpHandler()->AddHandlerCallback(
      "/privet/v3/leadership/announce", chromeos::http::request_type::kPost,
      base::Bind(&WebServerClient::RequestHandler,
                 weak_ptr_factory_.GetWeakPtr(),
                 QueryType::ANNOUNCE));
}

namespace {

std::unique_ptr<Value> GetBody(std::unique_ptr<Request> request) {
  std::string data(request->GetData().begin(), request->GetData().end());
  VLOG(3) << "Input: " << data;

  std::unique_ptr<Value> value;

  if (!data.empty()) {
    std::string content_type = chromeos::mime::RemoveParameters(
        request->GetFirstHeader(chromeos::http::request_header::kContentType));
    if (content_type == chromeos::mime::application::kJson) {
      value.reset(base::JSONReader::Read(data));
    }
  }
  return value;
}

}  // namespace

void WebServerClient::RequestHandler(QueryType query_type,
                                     std::unique_ptr<Request> request,
                                     std::unique_ptr<Response> response) {
  std::unique_ptr<Value> value{GetBody(std::move(request))};
  const base::DictionaryValue* dictionary = nullptr;
  if (value) value->GetAsDictionary(&dictionary);
  std::unique_ptr<base::DictionaryValue> output;
  switch (query_type) {
    case QueryType::DISCOVER:
      output = ProcessDiscover(dictionary);
      break;
    case QueryType::CHALLENGE:
      output = ProcessChallenge(dictionary);
      break;
    case QueryType::ANNOUNCE:
      output = ProcessAnnouncement(dictionary);
      break;
  }
  if (output) {
    response->ReplyWithJson(chromeos::http::status_code::Ok, output.get());
  } else {
    response->ReplyWithError(chromeos::http::status_code::BadRequest,
                             std::string{});
  }
}

std::unique_ptr<base::DictionaryValue> WebServerClient::ProcessDiscover(
    const base::DictionaryValue* input_dictionary) {
  std::unique_ptr<base::DictionaryValue> output;
  std::string group;
  std::string leader_uuid;
  if (input_dictionary &&
      input_dictionary->GetString(http_api::kDiscoverGroupKey, &group) &&
      input_dictionary->size() == 1 &&
      delegate_->HandleLeaderDiscover(
          group, &leader_uuid)) {
    output.reset(new base::DictionaryValue());
    output->SetString(http_api::kDiscoverLeaderKey, leader_uuid);
  }
  return output;
}

std::unique_ptr<base::DictionaryValue> WebServerClient::ProcessChallenge(
    const base::DictionaryValue* input_dictionary) {
  std::unique_ptr<base::DictionaryValue> output;
  int score;
  std::string group;
  std::string uuid;
  std::string my_uuid;
  std::string leader_uuid;
  if (input_dictionary &&
      input_dictionary->GetInteger(http_api::kChallengeScoreKey, &score) &&
      input_dictionary->GetString(http_api::kChallengeGroupKey, &group) &&
      input_dictionary->GetString(http_api::kChallengeIdKey, &uuid) &&
      input_dictionary->size() == 3 &&
      delegate_->HandleLeaderChallenge(
          group, uuid, score, &leader_uuid, &my_uuid)) {
    output.reset(new base::DictionaryValue());
    output->SetString(http_api::kChallengeLeaderKey, leader_uuid);
    output->SetString(http_api::kChallengeIdKey, my_uuid);
  }
  return output;
}

std::unique_ptr<base::DictionaryValue> WebServerClient::ProcessAnnouncement(
    const base::DictionaryValue* input_dictionary) {
  std::unique_ptr<base::DictionaryValue> output;
  std::string group_id;
  std::string leader_id;
  int32_t score;
  if (input_dictionary &&
      input_dictionary->GetString(http_api::kAnnounceGroupKey, &group_id) &&
      input_dictionary->GetString(http_api::kAnnounceLeaderIdKey, &leader_id) &&
      input_dictionary->GetInteger(http_api::kAnnounceScoreKey, &score) &&
      input_dictionary->size() == 3 &&
      delegate_->HandleLeaderAnnouncement(
          group_id, leader_id, score)) {
    output.reset(new base::DictionaryValue());
  }
  return output;
}

void WebServerClient::OnProtocolHandlerConnected(
    ProtocolHandler* protocol_handler) {
  if (protocol_handler->GetName() == protocol_handler_name_)
    delegate_->SetWebServerPort(*protocol_handler->GetPorts().begin());
}

void WebServerClient::OnProtocolHandlerDisconnected(
    ProtocolHandler* protocol_handler) {
  if (protocol_handler->GetName() == protocol_handler_name_)
    delegate_->SetWebServerPort(0);
}

}  // namespace leaderd
