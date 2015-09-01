// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_EXAMPLES_UBUNTU_EVENT_HTTP_SERVER_H_
#define LIBWEAVE_EXAMPLES_UBUNTU_EVENT_HTTP_SERVER_H_

#include <event2/http.h>
#include <evhttp.h>
#include <openssl/ssl.h>

#include <map>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <weave/http_server.h>

namespace weave {
namespace examples {

// HTTP/HTTPS server implemented with libevent.
class HttpServerImpl : public HttpServer {
 public:
  class RequestImpl;

  explicit HttpServerImpl(event_base* base);

  void AddOnStateChangedCallback(
      const OnStateChangedCallback& callback) override;
  void AddRequestHandler(const std::string& path_prefix,
                         const OnRequestCallback& callback) override;
  uint16_t GetHttpPort() const override;
  uint16_t GetHttpsPort() const override;
  const std::vector<uint8_t>& GetHttpsCertificateFingerprint() const override;

 private:
  void GenerateX509();
  static void ProcessRequestCallback(evhttp_request* req, void* arg);
  void ProcessRequest(evhttp_request* req);
  void ProcessReply(std::shared_ptr<RequestImpl> request,
                    int status_code,
                    const std::string& data,
                    const std::string& mime_type);
  void NotFound(evhttp_request* req);

  std::map<std::string, OnRequestCallback> handlers_;

  std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)> ec_key_{nullptr,
                                                          &EC_KEY_free};

  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey_{nullptr,
                                                            &EVP_PKEY_free};

  std::unique_ptr<X509, decltype(&X509_free)> x509_{nullptr, &X509_free};

  std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> ctx_{nullptr,
                                                         &SSL_CTX_free};
  std::vector<uint8_t> cert_fingerprint_;
  event_base* base_{nullptr};
  std::unique_ptr<evhttp, decltype(&evhttp_free)> httpd_{nullptr, &evhttp_free};
  std::unique_ptr<evhttp, decltype(&evhttp_free)> httpsd_{nullptr,
                                                          &evhttp_free};
  base::WeakPtrFactory<HttpServerImpl> weak_ptr_factory_{this};
};

}  // namespace examples
}  // namespace weave

#endif  // LIBWEAVE_EXAMPLES_UBUNTU_EVENT_HTTP_SERVER_H_
