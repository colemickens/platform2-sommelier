// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/webserver_client.h"

#include <vector>

#include <base/json/json_reader.h>
#include <base/logging.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>

using chromeos::dbus_utils::AsyncEventSequencer;
using libwebserv::ProtocolHandler;
using libwebserv::Request;
using libwebserv::Response;

namespace leaderd {

namespace {

const char kLeadershipScoreKey[] = "score";
const char kLeadershipGroupKey[] = "group";
const char kLeadershipIdKey[] = "uuid";
const char kLeadershipLeaderKey[] = "leader";

}  // namespace

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

  web_server_.GetDefaultHttpHandler()->AddHandlerCallback(
      "/privet/v3/leadership/challenge", chromeos::http::request_type::kPost,
      base::Bind(&WebServerClient::ChallengeRequestHandler,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebServerClient::ChallengeRequestHandler(scoped_ptr<Request> request,
                                              scoped_ptr<Response> response) {
  std::string data(request->GetData().begin(), request->GetData().end());
  VLOG(3) << "Input: " << data;

  std::unique_ptr<base::Value> value;
  const base::DictionaryValue* dictionary = nullptr;

  if (!data.empty()) {
    std::string content_type = chromeos::mime::RemoveParameters(
        request->GetFirstHeader(chromeos::http::request_header::kContentType));
    if (content_type == chromeos::mime::application::kJson) {
      value.reset(base::JSONReader::Read(data));
      if (value) value->GetAsDictionary(&dictionary);
    }
  }

  std::unique_ptr<base::DictionaryValue> output = ProcessChallenge(dictionary);
  if (output) {
    response->ReplyWithJson(chromeos::http::status_code::Ok, output.get());
  } else {
    response->ReplyWithError(chromeos::http::status_code::BadRequest, "");
  }
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
      input_dictionary->GetInteger(kLeadershipScoreKey, &score) &&
      input_dictionary->GetString(kLeadershipGroupKey, &group) &&
      input_dictionary->GetString(kLeadershipIdKey, &uuid) &&
      delegate_->HandleLeaderChallenge(
          uuid, group, score, &leader_uuid, &my_uuid)) {
    output.reset(new base::DictionaryValue());
    output->SetString(kLeadershipLeaderKey, leader_uuid);
    output->SetString(kLeadershipIdKey, my_uuid);
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
