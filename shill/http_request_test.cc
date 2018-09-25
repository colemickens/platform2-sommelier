// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/http_request.h"

#include <netinet/in.h>

#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <gtest/gtest.h>

#include "shill/http_url.h"
#include "shill/mock_async_connection.h"
#include "shill/mock_connection.h"
#include "shill/mock_control.h"
#include "shill/mock_device_info.h"
#include "shill/mock_dns_client.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/net/ip_address.h"
#include "shill/net/mock_sockets.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnNew;
using ::testing::ReturnRef;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Test;

namespace shill {

namespace {
const char kTextSiteName[] = "www.chromium.org";
const char kTextURL[] = "http://www.chromium.org/path/to/resource";
const char kNumericURL[] = "http://10.1.1.1";
const char kPath[] = "/path/to/resource";
const char kInterfaceName[] = "int0";
const char kDNSServer0[] = "8.8.8.8";
const char kDNSServer1[] = "8.8.4.4";
const char* const kDNSServers[] = {kDNSServer0, kDNSServer1};
const char kServerAddress[] = "10.1.1.1";
const int kServerFD = 10203;
const int kServerPort = 80;
}  // namespace

MATCHER_P(IsIPAddress, address, "") {
  IPAddress ip_address(IPAddress::kFamilyIPv4);
  EXPECT_TRUE(ip_address.SetAddressFromString(address));
  return ip_address.Equals(arg);
}

MATCHER_P(ByteStringMatches, byte_string, "") {
  return byte_string.Equals(arg);
}

MATCHER_P(CallbackEq, callback, "") {
  return arg.Equals(callback);
}

static int BreakConnection() {
  errno = EPIPE;
  return -1;
}

class HttpRequestTest : public Test {
 public:
  HttpRequestTest()
      : interface_name_(kInterfaceName),
        server_async_connection_(new StrictMock<MockAsyncConnection>()),
        dns_servers_(kDNSServers, kDNSServers + 2),
        dns_client_(new StrictMock<MockDnsClient>()),
        device_info_(
            new NiceMock<MockDeviceInfo>(&control_, nullptr, nullptr, nullptr)),
        connection_(new StrictMock<MockConnection>(device_info_.get())) {}

 protected:
  class CallbackTarget {
   public:
    CallbackTarget()
        : read_event_callback_(
              Bind(&CallbackTarget::ReadEventCallTarget, Unretained(this))),
          result_callback_(
              Bind(&CallbackTarget::ResultCallTarget, Unretained(this))) {}

    MOCK_METHOD1(ReadEventCallTarget, void(const ByteString& response_data));
    MOCK_METHOD2(ResultCallTarget, void(HttpRequest::Result result,
                                        const ByteString& response_data));
    const Callback<void(const ByteString&)>& read_event_callback() {
      return read_event_callback_;
    }
    const Callback<void(HttpRequest::Result,
                        const ByteString&)>& result_callback() {
      return result_callback_;
    }

   private:
    Callback<void(const ByteString&)> read_event_callback_;
    Callback<void(HttpRequest::Result, const ByteString&)> result_callback_;
  };

  void SetUp() override {
    EXPECT_CALL(*connection_.get(), IsIPv6())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*connection_.get(), interface_name())
        .WillRepeatedly(ReturnRef(interface_name_));
    EXPECT_CALL(*connection_.get(), dns_servers())
        .WillRepeatedly(ReturnRef(dns_servers_));

    request_.reset(new HttpRequest(connection_, &dispatcher_, &sockets_));
    // Passes ownership.
    request_->dns_client_.reset(dns_client_);
    // Passes ownership.
    request_->server_async_connection_.reset(server_async_connection_);
  }
  void TearDown() override {
    if (request_->is_running_) {
      ExpectStop();

      // Subtle: Make sure the finalization of the request happens while our
      // expectations are still active.
      request_.reset();
    }
  }
  size_t FindInRequestData(const string& find_string) {
    string request_string(
        reinterpret_cast<char*>(request_->request_data_.GetData()),
        request_->request_data_.GetLength());
    return request_string.find(find_string);
  }
  // Accessors
  const ByteString& GetRequestData() {
    return request_->request_data_;
  }
  HttpRequest* request() { return request_.get(); }
  MockSockets& sockets() { return sockets_; }

  // Expectations
  void ExpectReset() {
    EXPECT_EQ(connection_.get(), request_->connection_.get());
    EXPECT_EQ(&dispatcher_, request_->dispatcher_);
    EXPECT_EQ(&sockets_, request_->sockets_);
    EXPECT_TRUE(request_->result_callback_.is_null());
    EXPECT_TRUE(request_->read_event_callback_.is_null());
    EXPECT_FALSE(request_->connect_completion_callback_.is_null());
    EXPECT_FALSE(request_->dns_client_callback_.is_null());
    EXPECT_FALSE(request_->read_server_callback_.is_null());
    EXPECT_FALSE(request_->write_server_callback_.is_null());
    EXPECT_FALSE(request_->read_server_handler_.get());
    EXPECT_FALSE(request_->write_server_handler_.get());
    EXPECT_EQ(dns_client_, request_->dns_client_.get());
    EXPECT_EQ(server_async_connection_,
              request_->server_async_connection_.get());
    EXPECT_TRUE(request_->server_hostname_.empty());
    EXPECT_EQ(-1, request_->server_port_);
    EXPECT_EQ(-1, request_->server_socket_);
    EXPECT_EQ(HttpRequest::kResultUnknown, request_->timeout_result_);
    EXPECT_TRUE(request_->request_data_.IsEmpty());
    EXPECT_TRUE(request_->response_data_.IsEmpty());
    EXPECT_FALSE(request_->is_running_);
  }
  void ExpectStop() {
    if (request_->server_socket_ != -1) {
      EXPECT_CALL(sockets(), Close(kServerFD))
          .WillOnce(Return(0));
    }
    EXPECT_CALL(*dns_client_, Stop())
        .Times(AtLeast(1));
    EXPECT_CALL(*server_async_connection_, Stop())
        .Times(AtLeast(1));
    EXPECT_CALL(*connection_.get(), ReleaseRouting());
  }
  void ExpectSetTimeout(int timeout) {
    EXPECT_CALL(dispatcher_, PostDelayedTask(_, _, timeout * 1000));
  }
  void ExpectSetConnectTimeout() {
    ExpectSetTimeout(HttpRequest::kConnectTimeoutSeconds);
  }
  void ExpectSetInputTimeout() {
    ExpectSetTimeout(HttpRequest::kInputTimeoutSeconds);
  }
  void ExpectInResponse(const string& expected_response_data) {
    string response_string(
        reinterpret_cast<char*>(request_->response_data_.GetData()),
        request_->response_data_.GetLength());
    EXPECT_NE(string::npos, response_string.find(expected_response_data));
  }
  void ExpectDNSRequest(const string& host, bool return_value) {
    EXPECT_CALL(*dns_client_, Start(StrEq(host), _))
        .WillOnce(Return(return_value));
  }
  void ExpectAsyncConnect(const string& address, int port,
                          bool return_value) {
    EXPECT_CALL(*server_async_connection_, Start(IsIPAddress(address), port))
        .WillOnce(Return(return_value));
    if (return_value) {
      ExpectSetConnectTimeout();
    }
  }
  void  InvokeSyncConnect(const IPAddress& /*address*/, int /*port*/) {
    CallConnectCompletion(true, kServerFD);
  }
  void CallConnectCompletion(bool success, int fd) {
    request_->OnConnectCompletion(success, fd);
  }
  void ExpectSyncConnect(const string& address, int port) {
    EXPECT_CALL(*server_async_connection_, Start(IsIPAddress(address), port))
        .WillOnce(DoAll(Invoke(this, &HttpRequestTest::InvokeSyncConnect),
                        Return(true)));
  }
  void ExpectConnectFailure() {
    EXPECT_CALL(*server_async_connection_, Start(_, _))
        .WillOnce(Return(false));
  }
  void ExpectMonitorServerInput() {
    EXPECT_CALL(dispatcher_,
                CreateInputHandler(kServerFD,
                                   CallbackEq(request_->read_server_callback_),
                                   _))
        .WillOnce(ReturnNew<IOHandler>());
    ExpectSetInputTimeout();
  }
  void ExpectMonitorServerOutput() {
    EXPECT_CALL(dispatcher_,
                CreateReadyHandler(
                    kServerFD, IOHandler::kModeOutput,
                    CallbackEq(request_->write_server_callback_)))
        .WillOnce(ReturnNew<IOHandler>());
    ExpectSetInputTimeout();
  }
  void ExpectRouteRequest() {
    EXPECT_CALL(*connection_.get(), RequestRouting());
  }
  void ExpectRouteRelease() {
    EXPECT_CALL(*connection_.get(), ReleaseRouting());
  }
  void ExpectResultCallback(HttpRequest::Result result) {
    EXPECT_CALL(target_, ResultCallTarget(result, _));
  }
  void InvokeResultVerify(HttpRequest::Result result,
                          const ByteString& response_data) {
    EXPECT_EQ(HttpRequest::kResultSuccess, result);
    EXPECT_TRUE(expected_response_.Equals(response_data));
  }
  void ExpectResultCallbackWithResponse(const string& response) {
    expected_response_ = ByteString(response, false);
    EXPECT_CALL(target_, ResultCallTarget(HttpRequest::kResultSuccess, _))
        .WillOnce(Invoke(this, &HttpRequestTest::InvokeResultVerify));
  }
  void ExpectReadEventCallback(const string& response) {
    ByteString response_data(response, false);
    EXPECT_CALL(target_, ReadEventCallTarget(ByteStringMatches(response_data)));
  }
  void GetDNSResultFailure(const string& error_msg) {
    Error error(Error::kOperationFailed, error_msg);
    IPAddress address(IPAddress::kFamilyUnknown);
    request_->GetDNSResult(error, address);
  }
  void GetDNSResultSuccess(const IPAddress& address) {
    Error error;
    request_->GetDNSResult(error, address);
  }
  void OnConnectCompletion(bool result, int sockfd) {
    request_->OnConnectCompletion(result, sockfd);
  }
  void ReadFromServer(const string& data) {
    const unsigned char* ptr =
        reinterpret_cast<const unsigned char*>(data.c_str());
    vector<unsigned char> data_writable(ptr, ptr + data.length());
    InputData server_data(data_writable.data(), data_writable.size());
    request_->ReadFromServer(&server_data);
  }
  void ReadFromServerBadData(InputData* data) {
    CHECK_LT(data->len, 0);
    request_->ReadFromServer(data);
  }
  void WriteToServer(int fd) {
    request_->WriteToServer(fd);
  }
  HttpRequest::Result StartRequest(const string& url) {
    HttpUrl http_url;
    EXPECT_TRUE(http_url.ParseFromString(url));
    return request_->Start(http_url,
                           target_.read_event_callback(),
                           target_.result_callback());
  }
  void SetupConnectWithURL(const string& url, const string& expected_hostname) {
    ExpectRouteRequest();
    ExpectDNSRequest(expected_hostname, true);
    EXPECT_EQ(HttpRequest::kResultInProgress, StartRequest(url));
    IPAddress addr(IPAddress::kFamilyIPv4);
    EXPECT_TRUE(addr.SetAddressFromString(kServerAddress));
    GetDNSResultSuccess(addr);
  }
  void SetupConnect() {
    SetupConnectWithURL(kTextURL, kTextSiteName);
  }
  void SetupConnectAsync() {
    ExpectAsyncConnect(kServerAddress, kServerPort, true);
    SetupConnect();
  }
  void SetupConnectComplete() {
    SetupConnectAsync();
    ExpectMonitorServerOutput();
    OnConnectCompletion(true, kServerFD);
  }
  void CallTimeoutTask() {
    request_->TimeoutTask();
  }
  void CallServerErrorCallback() {
    request_->OnServerReadError(string());
  }

 private:
  const string interface_name_;
  // Owned by the HttpRequest, but tracked here for EXPECT().
  StrictMock<MockAsyncConnection>* server_async_connection_;
  vector<string> dns_servers_;
  // Owned by the HttpRequest, but tracked here for EXPECT().
  StrictMock<MockDnsClient>* dns_client_;
  StrictMock<MockEventDispatcher> dispatcher_;
  MockControl control_;
  std::unique_ptr<MockDeviceInfo> device_info_;
  scoped_refptr<MockConnection> connection_;
  std::unique_ptr<HttpRequest> request_;
  StrictMock<MockSockets> sockets_;
  StrictMock<CallbackTarget> target_;
  ByteString expected_response_;
};

TEST_F(HttpRequestTest, Constructor) {
  ExpectReset();
}


TEST_F(HttpRequestTest, FailConnectNumericSynchronous) {
  ExpectRouteRequest();
  ExpectConnectFailure();
  ExpectStop();
  EXPECT_EQ(HttpRequest::kResultConnectionFailure, StartRequest(kNumericURL));
  ExpectReset();
}

TEST_F(HttpRequestTest, FailConnectNumericAsynchronous) {
  ExpectRouteRequest();
  ExpectAsyncConnect(kServerAddress, HttpUrl::kDefaultHttpPort, true);
  EXPECT_EQ(HttpRequest::kResultInProgress, StartRequest(kNumericURL));
  ExpectResultCallback(HttpRequest::kResultConnectionFailure);
  ExpectStop();
  CallConnectCompletion(false, -1);
  ExpectReset();
}

TEST_F(HttpRequestTest, FailConnectNumericTimeout) {
  ExpectRouteRequest();
  ExpectAsyncConnect(kServerAddress, HttpUrl::kDefaultHttpPort, true);
  EXPECT_EQ(HttpRequest::kResultInProgress, StartRequest(kNumericURL));
  ExpectResultCallback(HttpRequest::kResultConnectionTimeout);
  ExpectStop();
  CallTimeoutTask();
  ExpectReset();
}

TEST_F(HttpRequestTest, SyncConnectNumeric) {
  ExpectRouteRequest();
  ExpectSyncConnect(kServerAddress, HttpUrl::kDefaultHttpPort);
  ExpectMonitorServerOutput();
  EXPECT_EQ(HttpRequest::kResultInProgress, StartRequest(kNumericURL));
}

TEST_F(HttpRequestTest, FailDNSStart) {
  ExpectRouteRequest();
  ExpectDNSRequest(kTextSiteName, false);
  ExpectStop();
  EXPECT_EQ(HttpRequest::kResultDNSFailure, StartRequest(kTextURL));
  ExpectReset();
}

TEST_F(HttpRequestTest, FailDNSFailure) {
  ExpectRouteRequest();
  ExpectDNSRequest(kTextSiteName, true);
  EXPECT_EQ(HttpRequest::kResultInProgress, StartRequest(kTextURL));
  ExpectResultCallback(HttpRequest::kResultDNSFailure);
  ExpectStop();
  GetDNSResultFailure(DnsClient::kErrorNoData);
  ExpectReset();
}

TEST_F(HttpRequestTest, FailDNSTimeout) {
  ExpectRouteRequest();
  ExpectDNSRequest(kTextSiteName, true);
  EXPECT_EQ(HttpRequest::kResultInProgress, StartRequest(kTextURL));
  ExpectResultCallback(HttpRequest::kResultDNSTimeout);
  ExpectStop();
  const string error(DnsClient::kErrorTimedOut);
  GetDNSResultFailure(error);
  ExpectReset();
}

TEST_F(HttpRequestTest, FailConnectText) {
  ExpectConnectFailure();
  ExpectResultCallback(HttpRequest::kResultConnectionFailure);
  ExpectStop();
  SetupConnect();
  ExpectReset();
}

TEST_F(HttpRequestTest, ConnectComplete) {
  SetupConnectComplete();
}

TEST_F(HttpRequestTest, RequestTimeout) {
  SetupConnectComplete();
  ExpectResultCallback(HttpRequest::kResultRequestTimeout);
  ExpectStop();
  CallTimeoutTask();
}

TEST_F(HttpRequestTest, RequestData) {
  SetupConnectComplete();
  EXPECT_EQ(0, FindInRequestData(string("GET ") + kPath));
  EXPECT_NE(string::npos,
            FindInRequestData(string("\r\nHost: ") + kTextSiteName));
  ByteString request_data = GetRequestData();
  EXPECT_CALL(sockets(), Send(kServerFD, _, request_data.GetLength(), _))
      .WillOnce(Return(request_data.GetLength() - 1));
  ExpectSetInputTimeout();
  WriteToServer(kServerFD);
  EXPECT_CALL(sockets(), Send(kServerFD, _, 1, _))
      .WillOnce(Return(1));
  ExpectMonitorServerInput();
  WriteToServer(kServerFD);
}

TEST_F(HttpRequestTest, ResponseTimeout) {
  SetupConnectComplete();
  ByteString request_data = GetRequestData();
  EXPECT_CALL(sockets(), Send(kServerFD, _, request_data.GetLength(), _))
      .WillOnce(Return(request_data.GetLength()));
  ExpectMonitorServerInput();
  WriteToServer(kServerFD);
  ExpectResultCallback(HttpRequest::kResultResponseTimeout);
  ExpectStop();
  CallTimeoutTask();
}

TEST_F(HttpRequestTest, ResponseInputError) {
  SetupConnectComplete();
  ByteString request_data = GetRequestData();
  EXPECT_CALL(sockets(), Send(kServerFD, _, request_data.GetLength(), _))
      .WillOnce(Return(request_data.GetLength()));
  ExpectMonitorServerInput();
  WriteToServer(kServerFD);
  ExpectResultCallback(HttpRequest::kResultResponseFailure);
  ExpectStop();
  CallServerErrorCallback();
}

TEST_F(HttpRequestTest, ResponseData) {
  SetupConnectComplete();
  const string response0("hello");
  ExpectReadEventCallback(response0);
  ExpectSetInputTimeout();
  ReadFromServer(response0);
  ExpectInResponse(response0);

  const string response1(" to you");
  ExpectReadEventCallback(response0 + response1);
  ExpectSetInputTimeout();
  ReadFromServer(response1);
  ExpectInResponse(response1);

  ExpectResultCallbackWithResponse(response0 + response1);
  ExpectStop();
  ReadFromServer("");
  ExpectReset();
}

TEST_F(HttpRequestTest, ResponseBadData) {
  // Test that ReadFromServer works with corrupt input / length
  SetupConnectComplete();
  const string response2("test no crash");
  const unsigned char* ptr =
      reinterpret_cast<const unsigned char*>(response2.c_str());
  vector<unsigned char> data_writable(ptr, ptr + response2.length());
  // Deliberately use negative size
  InputData server_data(data_writable.data(), 0 - data_writable.size());
  ExpectResultCallback(HttpRequest::kResultResponseFailure);
  ExpectStop();
  ReadFromServerBadData(&server_data);
}

TEST_F(HttpRequestTest, ResponseBrokenConnection) {
  SetupConnectComplete();
  ByteString request_data = GetRequestData();
  EXPECT_CALL(sockets(), Send(kServerFD, _, request_data.GetLength(), _))
      .WillOnce(WithoutArgs(Invoke(BreakConnection)));
  ExpectResultCallback(HttpRequest::kResultRequestFailure);
  ExpectStop();
  WriteToServer(kServerFD);
}

}  // namespace shill
