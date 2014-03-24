// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/http_request.h"

#include "buffet/http_transport_curl.h"
#include "buffet/mime_utils.h"

using namespace chromeos;
using namespace chromeos::http;

// request_type
const char request_type::kOptions[]               = "OPTIONS";
const char request_type::kGet[]                   = "GET";
const char request_type::kHead[]                  = "HEAD";
const char request_type::kPost[]                  = "POST";
const char request_type::kPut[]                   = "PUT";
const char request_type::kPatch[]                 = "PATCH";
const char request_type::kDelete[]                = "DELETE";
const char request_type::kTrace[]                 = "TRACE";
const char request_type::kConnect[]               = "CONNECT";
const char request_type::kCopy[]                  = "COPY";
const char request_type::kMove[]                  = "MOVE";

// request_header
const char request_header::kAccept[]              = "Accept";
const char request_header::kAcceptCharset[]       = "Accept-Charset";
const char request_header::kAcceptEncoding[]      = "Accept-Encoding";
const char request_header::kAcceptLanguage[]      = "Accept-Language";
const char request_header::kAllow[]               = "Allow";
const char request_header::kAuthorization[]       = "Authorization";
const char request_header::kCacheControl[]        = "Cache-Control";
const char request_header::kConnection[]          = "Connection";
const char request_header::kContentEncoding[]     = "Content-Encoding";
const char request_header::kContentLanguage[]     = "Content-Language";
const char request_header::kContentLength[]       = "Content-Length";
const char request_header::kContentLocation[]     = "Content-Location";
const char request_header::kContentMd5[]          = "Content-MD5";
const char request_header::kContentRange[]        = "Content-Range";
const char request_header::kContentType[]         = "Content-Type";
const char request_header::kCookie[]              = "Cookie";
const char request_header::kDate[]                = "Date";
const char request_header::kExpect[]              = "Expect";
const char request_header::kExpires[]             = "Expires";
const char request_header::kFrom[]                = "From";
const char request_header::kHost[]                = "Host";
const char request_header::kIfMatch[]             = "If-Match";
const char request_header::kIfModifiedSince[]     = "If-Modified-Since";
const char request_header::kIfNoneMatch[]         = "If-None-Match";
const char request_header::kIfRange[]             = "If-Range";
const char request_header::kIfUnmodifiedSince[]   = "If-Unmodified-Since";
const char request_header::kLastModified[]        = "Last-Modified";
const char request_header::kMaxForwards[]         = "Max-Forwards";
const char request_header::kPragma[]              = "Pragma";
const char request_header::kProxyAuthorization[]  = "Proxy-Authorization";
const char request_header::kRange[]               = "Range";
const char request_header::kReferer[]             = "Referer";
const char request_header::kTE[]                  = "TE";
const char request_header::kTrailer[]             = "Trailer";
const char request_header::kTransferEncoding[]    = "Transfer-Encoding";
const char request_header::kUpgrade[]             = "Upgrade";
const char request_header::kUserAgent[]           = "User-Agent";
const char request_header::kVia[]                 = "Via";
const char request_header::kWarning[]             = "Warning";

// response_header
const char response_header::kAcceptRanges[]       = "Accept-Ranges";
const char response_header::kAge[]                = "Age";
const char response_header::kAllow[]              = "Allow";
const char response_header::kCacheControl[]       = "Cache-Control";
const char response_header::kConnection[]         = "Connection";
const char response_header::kContentEncoding[]    = "Content-Encoding";
const char response_header::kContentLanguage[]    = "Content-Language";
const char response_header::kContentLength[]      = "Content-Length";
const char response_header::kContentLocation[]    = "Content-Location";
const char response_header::kContentMd5[]         = "Content-MD5";
const char response_header::kContentRange[]       = "Content-Range";
const char response_header::kContentType[]        = "Content-Type";
const char response_header::kDate[]               = "Date";
const char response_header::kETag[]               = "ETag";
const char response_header::kExpires[]            = "Expires";
const char response_header::kLastModified[]       = "Last-Modified";
const char response_header::kLocation[]           = "Location";
const char response_header::kPragma[]             = "Pragma";
const char response_header::kProxyAuthenticate[]  = "Proxy-Authenticate";
const char response_header::kRetryAfter[]         = "Retry-After";
const char response_header::kServer[]             = "Server";
const char response_header::kSetCookie[]          = "Set-Cookie";
const char response_header::kTrailer[]            = "Trailer";
const char response_header::kTransferEncoding[]   = "Transfer-Encoding";
const char response_header::kUpgrade[]            = "Upgrade";
const char response_header::kVary[]               = "Vary";
const char response_header::kVia[]                = "Via";
const char response_header::kWarning[]            = "Warning";
const char response_header::kWwwAuthenticate[]    = "WWW-Authenticate";

//**************************************************************************
//********************** Request Class **********************
//**************************************************************************
Request::Request(std::string const& url, char const* method) :
  transport_(new curl::Transport(url, method)) {
}

Request::Request(std::string const& url) :
  transport_(new curl::Transport(url, nullptr)) {
}

Request::Request(std::shared_ptr<TransportInterface> transport) :
  transport_(transport) {
}

void Request::AddRange(int64_t bytes) {
  if (transport_)
    transport_->AddRange(bytes);
}

void Request::AddRange(uint64_t from_byte, uint64_t to_byte) {
  if (transport_)
    transport_->AddRange(from_byte, to_byte);
}

std::unique_ptr<Response> Request::GetResponse() {
  if (transport_) {
    if (transport_->GetStage() == TransportInterface::Stage::initialized) {
      if(transport_->Perform())
        return std::unique_ptr<Response>(new Response(transport_));
    } else if (transport_->GetStage() ==
               TransportInterface::Stage::response_received) {
      return std::unique_ptr<Response>(new Response(transport_));
    }
  }
  return std::unique_ptr<Response>();
}

void Request::SetAccept(char const* accept_mime_types) {
  if (transport_)
    transport_->SetAccept(accept_mime_types);
}

std::string Request::GetAccept() const {
  return transport_ ? transport_->GetAccept() : std::string();
}

std::string Request::GetRequestURL() const {
  return transport_ ? transport_->GetRequestURL() : std::string();
}

void Request::SetContentType(char const* contentType) {
  if (transport_)
    transport_->SetContentType(contentType);
}

std::string Request::GetContentType() const {
  return transport_ ? transport_->GetContentType() : std::string();
}

void Request::AddHeader(char const* header, char const* value) {
  if (transport_)
    transport_->AddHeader(header, value);
}

bool Request::AddRequestBody(void const* data, size_t size) {
  return transport_ && transport_->AddRequestBody(data, size);
}

void Request::SetMethod(char const* method) {
  if (transport_)
    transport_->SetMethod(method);
}

std::string Request::GetMethod() const {
  return transport_ ? transport_->GetMethod() : std::string();
}

void Request::SetReferer(char const* referer) {
  if (transport_)
    transport_->SetReferer(referer);
}

std::string Request::GetReferer() const {
  return transport_ ? transport_->GetReferer() : std::string();
}

void Request::SetUserAgent(char const* user_agent) {
  if (transport_)
    transport_->SetUserAgent(user_agent);
}

std::string Request::GetUserAgent() const {
  return transport_ ? transport_->GetUserAgent() : std::string();
}

std::string Request::GetErrorMessage() const {
  if (transport_ &&
      transport_->GetStage() == TransportInterface::Stage::failed) {
    return transport_->GetErrorMessage();
  }

  return std::string();
}

//**************************************************************************
//********************** Response Class **********************
//**************************************************************************
Response::Response(std::shared_ptr<TransportInterface> transport) :
    transport_(transport) {
}

bool Response::IsSuccessful() const {
  if (transport_ &&
      transport_->GetStage() == TransportInterface::Stage::response_received) {
    int code = GetStatusCode();
    return code >= status_code::Continue && code < status_code::BadRequest;
  }
  return false;
}

int Response::GetStatusCode() const {
  if (!transport_)
    return -1;

  return transport_->GetResponseStatusCode();
}

std::string Response::GetStatusText() const {
  if (!transport_)
    return std::string();

  return transport_->GetResponseStatusText();
}

std::string Response::GetContentType() const {
  return GetHeader(response_header::kContentType);
}

std::vector<unsigned char> Response::GetData() const {
  if (transport_)
    return transport_->GetResponseData();

  return std::vector<unsigned char>();
}

std::string Response::GetDataAsString() const {
  if (transport_) {
    auto data = transport_->GetResponseData();
    char const* data_buf = reinterpret_cast<char const*>(data.data());
    return std::string(data_buf, data_buf + data.size());
  }

  return std::string();
}

std::string Response::GetHeader(char const* header_name) const {
  if (transport_)
    return transport_->GetResponseHeader(header_name);

  return std::string();
}

