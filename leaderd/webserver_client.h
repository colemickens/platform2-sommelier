// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_WEBSERVER_CLIENT_H_
#define LEADERD_WEBSERVER_CLIENT_H_

#include <string>

#include <base/callback.h>
#include <base/values.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <dbus/bus.h>
#include <libwebserv/protocol_handler.h>
#include <libwebserv/request.h>
#include <libwebserv/response.h>
#include <libwebserv/server.h>

namespace leaderd {

class Manager;

class WebServerClient {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void SetWebServerPort(uint16_t port) = 0;
    virtual bool HandleLeaderChallenge(const std::string& group_id,
                                       const std::string& challenger_id,
                                       int32_t challenger_score,
                                       std::string* leader_id,
                                       std::string* responder_id) = 0;
    virtual bool HandleLeaderAnnouncement(const std::string& group_id,
                                          const std::string& leader_id,
                                          int32_t leader_score) = 0;
  };

  WebServerClient(Delegate* delegate,
                  const std::string& web_handler_name);
  virtual ~WebServerClient() = default;

  void RegisterAsync(const scoped_refptr<dbus::Bus>& bus,
                     const std::string& leaderd_service_name,
                     chromeos::dbus_utils::AsyncEventSequencer* sequencer);

 private:
  friend class WebServerClientTest;
  std::unique_ptr<base::Value> GetBody(scoped_ptr<libwebserv::Request> request);
  void ChallengeRequestHandler(scoped_ptr<libwebserv::Request> request,
                               scoped_ptr<libwebserv::Response> response);
  void AnnouncementRequestHandler(scoped_ptr<libwebserv::Request> request,
                                  scoped_ptr<libwebserv::Response> response);
  std::unique_ptr<base::DictionaryValue> ProcessChallenge(
      const base::DictionaryValue* input);
  bool ProcessAnnouncement(const base::DictionaryValue* input);
  void OnProtocolHandlerConnected(
      libwebserv::ProtocolHandler* protocol_handler);
  void OnProtocolHandlerDisconnected(
      libwebserv::ProtocolHandler* protocol_handler);

  Delegate* delegate_;
  const std::string protocol_handler_name_;
  libwebserv::Server web_server_;
  base::WeakPtrFactory<WebServerClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebServerClient);
};

}  // namespace leaderd

#endif  // LEADERD_WEBSERVER_CLIENT_H_
