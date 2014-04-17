// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_HTTP_REQUEST_H_
#define BUFFET_HTTP_REQUEST_H_

#include <vector>
#include <memory>
#include <string>
#include <base/basictypes.h>

#include "buffet/transport_interface.h"

namespace chromeos {
namespace http {

// HTTP request verbs
namespace request_type {
  extern const char kOptions[];
  extern const char kGet[];
  extern const char kHead[];
  extern const char kPost[];
  extern const char kPut[];
  extern const char kPatch[];  // Not a standard HTTP/1.1 request method
  extern const char kDelete[];
  extern const char kTrace[];
  extern const char kConnect[];
  extern const char kCopy[];   // Not a standard HTTP/1.1 request method
  extern const char kMove[];   // Not a standard HTTP/1.1 request method
} // namespace request_type

// HTTP request header names
namespace request_header {
  extern const char kAccept[];
  extern const char kAcceptCharset[];
  extern const char kAcceptEncoding[];
  extern const char kAcceptLanguage[];
  extern const char kAllow[];
  extern const char kAuthorization[];
  extern const char kCacheControl[];
  extern const char kConnection[];
  extern const char kContentEncoding[];
  extern const char kContentLanguage[];
  extern const char kContentLength[];
  extern const char kContentLocation[];
  extern const char kContentMd5[];
  extern const char kContentRange[];
  extern const char kContentType[];
  extern const char kCookie[];
  extern const char kDate[];
  extern const char kExpect[];
  extern const char kExpires[];
  extern const char kFrom[];
  extern const char kHost[];
  extern const char kIfMatch[];
  extern const char kIfModifiedSince[];
  extern const char kIfNoneMatch[];
  extern const char kIfRange[];
  extern const char kIfUnmodifiedSince[];
  extern const char kLastModified[];
  extern const char kMaxForwards[];
  extern const char kPragma[];
  extern const char kProxyAuthorization[];
  extern const char kRange[];
  extern const char kReferer[];
  extern const char kTE[];
  extern const char kTrailer[];
  extern const char kTransferEncoding[];
  extern const char kUpgrade[];
  extern const char kUserAgent[];
  extern const char kVia[];
  extern const char kWarning[];
} // namespace request_header

// HTTP response header names
namespace response_header {
  extern const char kAcceptRanges[];
  extern const char kAge[];
  extern const char kAllow[];
  extern const char kCacheControl[];
  extern const char kConnection[];
  extern const char kContentEncoding[];
  extern const char kContentLanguage[];
  extern const char kContentLength[];
  extern const char kContentLocation[];
  extern const char kContentMd5[];
  extern const char kContentRange[];
  extern const char kContentType[];
  extern const char kDate[];
  extern const char kETag[];
  extern const char kExpires[];
  extern const char kLastModified[];
  extern const char kLocation[];
  extern const char kPragma[];
  extern const char kProxyAuthenticate[];
  extern const char kRetryAfter[];
  extern const char kServer[];
  extern const char kSetCookie[];
  extern const char kTrailer[];
  extern const char kTransferEncoding[];
  extern const char kUpgrade[];
  extern const char kVary[];
  extern const char kVia[];
  extern const char kWarning[];
  extern const char kWwwAuthenticate[];
} // namespace response_header

// HTTP request status (error) codes
namespace status_code {
  // OK to continue with request
  static const int Continue = 100;
  // Server has switched protocols in upgrade header
  static const int SwitchProtocols = 101;

  // Request completed
  static const int Ok = 200;
  // Object created, reason = new URI
  static const int Created = 201;
  // Async completion (TBS)
  static const int Accepted = 202;
  // Partial completion
  static const int Partial = 203;
  // No info to return
  static const int NoContent = 204;
  // Request completed, but clear form
  static const int ResetContent = 205;
  // Partial GET furfilled
  static const int PartialContent = 206;

  // Server couldn't decide what to return
  static const int Ambiguous = 300;
  // Object permanently moved
  static const int Moved = 301;
  // Object temporarily moved
  static const int Redirect = 302;
  // Redirection w/ new access method
  static const int RedirectMethod = 303;
  // If-Modified-Since was not modified
  static const int NotModified = 304;
  // Redirection to proxy, location header specifies proxy to use
  static const int UseProxy = 305;
  // HTTP/1.1: keep same verb
  static const int RedirectKeepVerb = 307;

  // Invalid syntax
  static const int BadRequest = 400;
  // Access denied
  static const int Denied = 401;
  // Payment required
  static const int PaymentRequired = 402;
  // Request forbidden
  static const int Forbidden = 403;
  // Object not found
  static const int NotFound = 404;
  // Method is not allowed
  static const int BadMethod = 405;
  // No response acceptable to client found
  static const int NoneAcceptable = 406;
  // Proxy authentication required
  static const int ProxyAuthRequired = 407;
  // Server timed out waiting for request
  static const int RequestTimeout = 408;
  // User should resubmit with more info
  static const int Conflict = 409;
  // The resource is no longer available
  static const int Gone = 410;
  // The server refused to accept request w/o a length
  static const int LengthRequired = 411;
  // Precondition given in request failed
  static const int PrecondionFailed = 412;
  // Request entity was too large
  static const int RequestTooLarge = 413;
  // Request URI too long
  static const int UriTooLong = 414;
  // Unsupported media type
  static const int UnsupportedMedia = 415;
  // Retry after doing the appropriate action.
  static const int RetryWith = 449;

  // Internal server error
  static const int InternalServerError = 500;
  // Request not supported
  static const int NotSupported = 501;
  // Error response received from gateway
  static const int BadGateway = 502;
  // Temporarily overloaded
  static const int ServiceUnavailable = 503;
  // Timed out waiting for gateway
  static const int GatewayTimeout = 504;
  // HTTP version not supported
  static const int VersionNotSupported = 505;
} // namespace status_code

class Response; // Just a forward-declarartion

///////////////////////////////////////////////////////////////////////////////
// Request class is the main object used to set up and initiate an HTTP
// communication session. It is used to specify the HTTP request method,
// request URL and many optional parameters (such as HTTP headers, user agent,
// referer URL and so on.
//
// Once everything is setup, GetResponse() method is used to send the request
// and obtain the server response. The returned Response onject can be
// used to inspect the response code, HTTP headers and/or response body.
///////////////////////////////////////////////////////////////////////////////
class Request {
 public:
  // The main constructor. |url| specifies the remote host address/path
  // to send the request to. Optional |method| is the HTTP request verb. If
  // omitted, "GET" is used.
  // Uses the default libcurl-based implementation of TransportInterface
  Request(const std::string& url, const char* method);
  Request(const std::string& url);

  // Custom constructor that allows non-default implementations
  // of TransportInterface to be used.
  Request(std::shared_ptr<TransportInterface> transport);

  // Gets/Sets "Accept:" header value. The default value is "*/*" if not set.
  void SetAccept(const char* accept_mime_types);
  std::string GetAccept() const;

  // Gets/Sets "Content-Type:" header value
  void SetContentType(const char* content_type);
  std::string GetContentType() const;

  // Adds additional HTTP request header
  void AddHeader(const char* header, const char* value);
  void AddHeaders(const HeaderList& headers);

  // Removes HTTP request header
  void RemoveHeader(const char* header);

  // Adds a request body. This is not to be used with GET method
  bool AddRequestBody(const void* data, size_t size);

  // Makes a request for a subrange of data. Specifies a partial range with
  // either from beginning of the data to the specified offset (if |bytes| is
  // negative) or from the specified offset to the end of data (if |bytes| is
  // positive).
  // All individual ranges will be sent as part of "Range:" HTTP request header.
  void AddRange(int64_t bytes);

  // Makes a request for a subrange of data. Specifies a full range with
  // start and end bytes from the beginning of the requested data.
  // All individual ranges will be sent as part of "Range:" HTTP request header.
  void AddRange(uint64_t from_byte, uint64_t to_byte);

  // Gets/Sets an HTTP request verb to be used with request
  void SetMethod(const char* method);
  std::string GetMethod() const;

  // Returns the request URL
  std::string GetRequestURL() const;

  // Gets/Sets a request referer URL (sent as "Referer:" request header).
  void SetReferer(const char* referer);
  std::string GetReferer() const;

  // Gets/Sets a user agent string (sent as "User-Agent:" request header).
  void SetUserAgent(const char* user_agent);
  std::string GetUserAgent() const;

  // Sends the request to the server and returns the response object.
  // In case the server couldn't be reached for whatever reason, returns
  // empty unique_ptr (null). Calling GetErrorMessage() provides additional
  // information in such as case.
  std::unique_ptr<Response> GetResponse();

  // If the request failed before reaching the server, returns additional
  // information about the error occurred.
  std::string GetErrorMessage() const;

 private:
  std::shared_ptr<TransportInterface> transport_;
  DISALLOW_COPY_AND_ASSIGN(Request);
};

///////////////////////////////////////////////////////////////////////////////
// Response class is returned from Request::GetResponse() and is a way
// to get to response status, error codes, response HTTP headers and response
// data (body) if available.
///////////////////////////////////////////////////////////////////////////////
class Response {
 public:
  Response(std::shared_ptr<TransportInterface> transport);

  // Returns true if server returned a success code (status code below 400).
  bool IsSuccessful() const;

  // Returns the HTTP status code (e.g. 200 for success)
  int GetStatusCode() const;

  // Returns the status text (e.g. for error 403 it could be "NOT AUTHORIZED").
  std::string GetStatusText() const;

  // Returns the content type of the response data.
  std::string GetContentType() const;

  // Returns response data as a byte array
  std::vector<unsigned char> GetData() const;

  // Returns response data as a string
  std::string GetDataAsString() const;

  // Returns a value of a given response HTTP header.
  std::string GetHeader(const char* header_name) const;

 private:
  std::shared_ptr<TransportInterface> transport_;
  DISALLOW_COPY_AND_ASSIGN(Response);
};

} // namespace http
} // namespace chromeos

#endif // BUFFET_HTTP_REQUEST_H_
