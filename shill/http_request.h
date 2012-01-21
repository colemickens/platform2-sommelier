// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_HTTP_REQUEST_
#define SHILL_HTTP_REQUEST_

#include <string>
#include <vector>

#include <base/callback_old.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/task.h>

#include "shill/byte_string.h"
#include "shill/refptr_types.h"
#include "shill/shill_time.h"

namespace shill {

class AsyncConnection;
class DNSClient;
class EventDispatcher;
class HTTPURL;
class InputData;
class IOHandler;
class IPAddress;
class Sockets;

// The HTTPRequest class implements facilities for performing
// a simple "GET" request and returning the contents via a
// callback.
class HTTPRequest {
 public:
  enum Result {
    kResultUnknown,
    kResultInProgress,
    kResultDNSFailure,
    kResultDNSTimeout,
    kResultConnectionFailure,
    kResultConnectionTimeout,
    kResultRequestFailure,
    kResultRequestTimeout,
    kResultResponseFailure,
    kResultResponseTimeout,
    kResultSuccess
  };

  HTTPRequest(ConnectionRefPtr connection,
              EventDispatcher *dispatcher,
              Sockets *sockets);
  virtual ~HTTPRequest();

  // Start an http GET request to the URL |url|.  Whenever any data is
  // read from the server, |read_event_callback| is called with the number
  // of bytes read.  This callback could be called more than once as data
  // arrives.  All callbacks can access the data as it is read in by
  // using the response_data() getter below.
  //
  // When the transaction completes, |result_callback| will be called with
  // the final status from the transaction.  |result_callback| will not be
  // called if either the Start() call fails or if Stop() is called before
  // the transaction completes.
  //
  // This (Start) function returns a failure result if the request
  // failed during initialization, or kResultInProgress if the request
  // has started successfully and is now in progress.
  Result Start(const HTTPURL &url,
               Callback1<int>::Type *read_event_callback,
               Callback1<Result>::Type *result_callback);

  // Stop the current HTTPRequest.  No callback is called as a side
  // effect of this function.
  void Stop();

  const ByteString &response_data() const { return response_data_; }

 private:
  friend class HTTPRequestTest;

  // Time to wait for connection to remote server.
  static const int kConnectTimeoutSeconds;
  // Time to wait for DNS server.
  static const int kDNSTimeoutSeconds;
  // Time to wait for any input from server.
  static const int kInputTimeoutSeconds;

  static const char kHTTPRequestTemplate[];

  bool ConnectServer(const IPAddress &address, int port);
  void GetDNSResult(bool result);
  void OnConnectCompletion(bool success, int fd);
  void ReadFromServer(InputData *data);
  void SendStatus(Result result);
  void StartIdleTimeout(int timeout_seconds, Result timeout_result);
  void TimeoutTask();
  void WriteToServer(int fd);

  ConnectionRefPtr connection_;
  EventDispatcher *dispatcher_;
  Sockets *sockets_;

  scoped_ptr<Callback2<bool, int>::Type> connect_completion_callback_;
  scoped_ptr<Callback1<bool>::Type> dns_client_callback_;
  scoped_ptr<Callback1<InputData *>::Type> read_server_callback_;
  scoped_ptr<Callback1<int>::Type> write_server_callback_;
  Callback1<Result>::Type *result_callback_;
  Callback1<int>::Type *read_event_callback_;
  ScopedRunnableMethodFactory<HTTPRequest> task_factory_;
  CancelableTask *idle_timeout_;
  scoped_ptr<IOHandler> read_server_handler_;
  scoped_ptr<IOHandler> write_server_handler_;
  scoped_ptr<DNSClient> dns_client_;
  scoped_ptr<AsyncConnection> server_async_connection_;
  std::string server_hostname_;
  int server_port_;
  int server_socket_;
  Result timeout_result_;
  ByteString request_data_;
  ByteString response_data_;
  bool is_running_;

  DISALLOW_COPY_AND_ASSIGN(HTTPRequest);
};

}  // namespace shill

#endif  // SHILL_HTTP_REQUEST_
