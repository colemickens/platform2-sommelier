// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/examples/ubuntu/event_http_server.h"

#include <vector>

#include <base/bind.h>
#include <base/time/time.h>
#include <event2/bufferevent_ssl.h>
#include <openssl/err.h>

namespace weave {
namespace examples {

namespace {

std::string GetSslError() {
  char error[1000] = {};
  ERR_error_string_n(ERR_get_error(), error, sizeof(error));
  return error;
}

bufferevent* BuffetEventCallback(event_base* base, void* arg) {
  SSL_CTX* ctx = static_cast<SSL_CTX*>(arg);
  return bufferevent_openssl_socket_new(
      base, -1, SSL_new(ctx), BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
}

}  // namespace

class HttpServerImpl::RequestImpl : public Request {
 public:
  explicit RequestImpl(evhttp_request* req) : path_{evhttp_request_uri(req)} {
    path_ = path_.substr(0, path_.find("?"));
    path_ = path_.substr(0, path_.find("#"));
    req_.reset(req);

    data_.resize(evbuffer_get_length(req_->input_buffer));
    evbuffer_remove(req_->input_buffer, data_.data(), data_.size());
  }

  ~RequestImpl() {}

  const std::string& GetPath() const override { return path_; }
  std::string GetFirstHeader(const std::string& name) const override {
    const char* header = evhttp_find_header(req_->input_headers, name.c_str());
    if (!header)
      return {};
    return header;
  }
  const std::vector<uint8_t>& GetData() const override { return data_; }

  evhttp_request* ReleaseHandler() { return req_.release(); }

 private:
  std::vector<uint8_t> data_;
  std::unique_ptr<evhttp_request, decltype(&evhttp_cancel_request)> req_{
      nullptr, &evhttp_cancel_request};
  std::string path_;
};

HttpServerImpl::HttpServerImpl(event_base* base) : base_{base} {
  SSL_load_error_strings();
  SSL_library_init();

  ctx_.reset(SSL_CTX_new(TLSv1_2_server_method()));
  SSL_CTX_set_options(ctx_.get(), SSL_OP_SINGLE_DH_USE |
                                      SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_SSLv2);

  ec_key_.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(ec_key_) << GetSslError();
  CHECK_EQ(1, SSL_CTX_set_tmp_ecdh(ctx_.get(), ec_key_.get())) << GetSslError();

  GenerateX509();
  CHECK_EQ(1, SSL_CTX_use_PrivateKey(ctx_.get(), pkey_.get())) << GetSslError();
  CHECK_EQ(1, SSL_CTX_use_certificate(ctx_.get(), x509_.get()))
      << GetSslError();

  CHECK_EQ(1, SSL_CTX_check_private_key(ctx_.get())) << GetSslError();

  httpd_.reset(evhttp_new(base_));
  CHECK(httpd_);
  httpsd_.reset(evhttp_new(base_));
  CHECK(httpsd_);

  evhttp_set_bevcb(httpsd_.get(), BuffetEventCallback, ctx_.get());
  evhttp_set_gencb(httpd_.get(), ProcessRequestCallback, this);
  evhttp_set_gencb(httpsd_.get(), ProcessRequestCallback, this);

  CHECK_EQ(0, evhttp_bind_socket(httpd_.get(), "0.0.0.0", GetHttpPort()));
  CHECK_EQ(0, evhttp_bind_socket(httpsd_.get(), "0.0.0.0", GetHttpsPort()));
}

void HttpServerImpl::GenerateX509() {
  x509_.reset(X509_new());
  CHECK(x509_) << GetSslError();

  X509_set_version(x509_.get(), 2);

  X509_gmtime_adj(X509_get_notBefore(x509_.get()), 0);
  X509_gmtime_adj(X509_get_notAfter(x509_.get()),
                  base::TimeDelta::FromDays(365).InSeconds());

  pkey_.reset(EVP_PKEY_new());
  CHECK(pkey_) << GetSslError();
  std::unique_ptr<BIGNUM, decltype(&BN_free)> big_num(BN_new(), &BN_free);
  CHECK(BN_set_word(big_num.get(), 65537)) << GetSslError();
  auto rsa = RSA_new();
  RSA_generate_key_ex(rsa, 2048, big_num.get(), nullptr);
  CHECK(EVP_PKEY_assign_RSA(pkey_.get(), rsa)) << GetSslError();

  X509_set_pubkey(x509_.get(), pkey_.get());

  CHECK(X509_sign(x509_.get(), pkey_.get(), EVP_sha256())) << GetSslError();

  cert_fingerprint_.resize(EVP_MD_size(EVP_sha256()));
  uint32_t len = 0;
  CHECK(X509_digest(x509_.get(), EVP_sha256(), cert_fingerprint_.data(), &len));
  CHECK_EQ(len, cert_fingerprint_.size());
}

void HttpServerImpl::ProcessRequestCallback(evhttp_request* req, void* arg) {
  static_cast<HttpServerImpl*>(arg)->ProcessRequest(req);
}

void HttpServerImpl::NotFound(evhttp_request* req) {
  std::unique_ptr<evbuffer, decltype(&evbuffer_free)> buf{evbuffer_new(),
                                                          &evbuffer_free};
  evbuffer_add_printf(buf.get(), "404 Not Found: %s\n",
                      evhttp_request_uri(req));
  evhttp_send_reply(req, 404, "Not Found", buf.get());
}

void HttpServerImpl::ProcessRequest(evhttp_request* req) {
  std::string path = evhttp_request_uri(req);
  for (auto i = handlers_.rbegin(); i != handlers_.rend(); ++i) {
    if (path.compare(0, i->first.size(), i->first) == 0) {
      auto request = std::make_shared<RequestImpl>(req);
      i->second.Run(*request,
                    base::Bind(&HttpServerImpl::ProcessReply,
                               weak_ptr_factory_.GetWeakPtr(), request));
      return;
    }
  }
  NotFound(req);
}
void HttpServerImpl::ProcessReply(std::shared_ptr<RequestImpl> request,
                                  int status_code,
                                  const std::string& data,
                                  const std::string& mime_type) {
  std::unique_ptr<evbuffer, decltype(&evbuffer_free)> buf{evbuffer_new(),
                                                          &evbuffer_free};
  evbuffer_add(buf.get(), data.data(), data.size());
  evhttp_request* req = request->ReleaseHandler();
  evhttp_add_header(req->output_headers, "Content-Type", mime_type.c_str());
  evhttp_send_reply(req, status_code, "None", buf.get());
}

void HttpServerImpl::AddOnStateChangedCallback(
    const OnStateChangedCallback& callback) {
  callback.Run(*this);
}

void HttpServerImpl::AddRequestHandler(const std::string& path_prefix,
                                       const OnRequestCallback& callback) {
  handlers_.emplace(path_prefix, callback);
}

uint16_t HttpServerImpl::GetHttpPort() const {
  return 7780;
}

uint16_t HttpServerImpl::GetHttpsPort() const {
  return 7781;
}

const std::vector<uint8_t>& HttpServerImpl::GetHttpsCertificateFingerprint()
    const {
  return cert_fingerprint_;
}

}  // namespace examples
}  // namespace weave
