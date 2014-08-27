// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/values.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/http/http_transport_fake.h>
#include <chromeos/http/http_utils.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>
#include <chromeos/url_utils.h>
#include <gtest/gtest.h>

namespace chromeos {
namespace http {

static const char kFakeUrl[] = "http://localhost";
static const char kEchoUrl[] = "http://localhost/echo";
static const char kMethodEchoUrl[] = "http://localhost/echo/method";

///////////////////// Generic helper request handlers /////////////////////////
// Returns the request data back with the same content type.
static void EchoDataHandler(const fake::ServerRequest& request,
                            fake::ServerResponse* response) {
  response->Reply(status_code::Ok, request.GetData(),
                  request.GetHeader(request_header::kContentType).c_str());
}

// Returns the request method as a plain text response.
static void EchoMethodHandler(const fake::ServerRequest& request,
                              fake::ServerResponse* response) {
  response->ReplyText(status_code::Ok, request.GetMethod(),
                      chromeos::mime::text::kPlain);
}

///////////////////////////////////////////////////////////////////////////////
TEST(HttpUtils, SendRequest_BinaryData) {
  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kEchoUrl, request_type::kPost,
                        base::Bind(EchoDataHandler));

  // Test binary data round-tripping.
  std::vector<unsigned char> custom_data{0xFF, 0x00, 0x80, 0x40, 0xC0, 0x7F};
  auto response = http::SendRequest(request_type::kPost, kEchoUrl,
                                    custom_data.data(), custom_data.size(),
                                    chromeos::mime::application::kOctet_stream,
                                    HeaderList(), transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::application::kOctet_stream,
            response->GetContentType());
  EXPECT_EQ(custom_data.size(), response->GetData().size());
  EXPECT_EQ(custom_data, response->GetData());
}

TEST(HttpUtils, SendRequest_Post) {
  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kMethodEchoUrl, "*", base::Bind(EchoMethodHandler));

  // Test binary data round-tripping.
  std::vector<unsigned char> custom_data{0xFF, 0x00, 0x80, 0x40, 0xC0, 0x7F};

  // Check the correct HTTP method used.
  auto response = http::SendRequest(request_type::kPost, kMethodEchoUrl,
                                    custom_data.data(), custom_data.size(),
                                    chromeos::mime::application::kOctet_stream,
                                    HeaderList(), transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::text::kPlain, response->GetContentType());
  EXPECT_EQ(request_type::kPost, response->GetDataAsString());
}

TEST(HttpUtils, SendRequest_Get) {
  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kMethodEchoUrl, "*", base::Bind(EchoMethodHandler));

  auto response = http::SendRequest(request_type::kGet, kMethodEchoUrl,
                                    nullptr, 0, nullptr,
                                    HeaderList(), transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::text::kPlain, response->GetContentType());
  EXPECT_EQ(request_type::kGet, response->GetDataAsString());
}

TEST(HttpUtils, SendRequest_Put) {
  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kMethodEchoUrl, "*", base::Bind(EchoMethodHandler));

  auto response = http::SendRequest(request_type::kPut, kMethodEchoUrl,
                                    nullptr, 0, nullptr,
                                    HeaderList(), transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::text::kPlain, response->GetContentType());
  EXPECT_EQ(request_type::kPut, response->GetDataAsString());
}

TEST(HttpUtils, SendRequest_NotFound) {
  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  // Test failed response (URL not found).
  auto response = http::SendRequest(request_type::kGet, "http://blah.com",
                                    nullptr, 0, nullptr,
                                    HeaderList(), transport, nullptr);
  EXPECT_FALSE(response->IsSuccessful());
  EXPECT_EQ(status_code::NotFound, response->GetStatusCode());
}

TEST(HttpUtils, SendRequest_Headers) {
  std::shared_ptr<fake::Transport> transport(new fake::Transport);

  static const char json_echo_url[] = "http://localhost/echo/json";
  auto JsonEchoHandler = [](const fake::ServerRequest& request,
                            fake::ServerResponse* response) {
    base::DictionaryValue json;
    json.SetString("method", request.GetMethod());
    json.SetString("data", request.GetDataAsString());
    for (const auto& pair : request.GetHeaders()) {
      json.SetString("header." + pair.first, pair.second);
    }
    response->ReplyJson(status_code::Ok, &json);
  };
  transport->AddHandler(json_echo_url, "*",
                        base::Bind(JsonEchoHandler));
  auto response = http::SendRequest(
      request_type::kPost, json_echo_url, "abcd", 4,
      chromeos::mime::application::kOctet_stream, {
        {request_header::kCookie, "flavor=vanilla"},
        {request_header::kIfMatch, "*"},
      }, transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::application::kJson,
            chromeos::mime::RemoveParameters(response->GetContentType()));
  auto json = ParseJsonResponse(response.get(), nullptr, nullptr);
  std::string value;
  EXPECT_TRUE(json->GetString("method", &value));
  EXPECT_EQ(request_type::kPost, value);
  EXPECT_TRUE(json->GetString("data", &value));
  EXPECT_EQ("abcd", value);
  EXPECT_TRUE(json->GetString("header.Cookie", &value));
  EXPECT_EQ("flavor=vanilla", value);
  EXPECT_TRUE(json->GetString("header.Content-Type", &value));
  EXPECT_EQ(chromeos::mime::application::kOctet_stream, value);
  EXPECT_TRUE(json->GetString("header.Content-Length", &value));
  EXPECT_EQ("4", value);
  EXPECT_TRUE(json->GetString("header.If-Match", &value));
  EXPECT_EQ("*", value);
}

TEST(HttpUtils, Get) {
  // Sends back the "?test=..." portion of URL.
  // So if we do GET "http://localhost?test=blah", this handler responds
  // with "blah" as text/plain.
  auto GetHandler = [](const fake::ServerRequest& request,
                       fake::ServerResponse* response) {
    EXPECT_EQ(request_type::kGet, request.GetMethod());
    EXPECT_EQ("0", request.GetHeader(request_header::kContentLength));
    EXPECT_EQ("", request.GetHeader(request_header::kContentType));
    response->ReplyText(status_code::Ok, request.GetFormField("test"),
                        chromeos::mime::text::kPlain);
  };

  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kFakeUrl, request_type::kGet, base::Bind(GetHandler));
  transport->AddHandler(kMethodEchoUrl, "*", base::Bind(EchoMethodHandler));

  // Make sure Get/GetAsString actually do the GET request
  auto response = http::Get(kMethodEchoUrl, transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::text::kPlain, response->GetContentType());
  EXPECT_EQ(request_type::kGet, response->GetDataAsString());
  EXPECT_EQ(request_type::kGet,
            http::GetAsString(kMethodEchoUrl, transport, nullptr));

  for (std::string data : {"blah", "some data", ""}) {
    std::string url = chromeos::url::AppendQueryParam(kFakeUrl, "test", data);
    EXPECT_EQ(data, http::GetAsString(url, transport, nullptr));
  }
}

TEST(HttpUtils, Head) {
  auto HeadHandler = [](const fake::ServerRequest& request,
                        fake::ServerResponse* response) {
    EXPECT_EQ(request_type::kHead, request.GetMethod());
    EXPECT_EQ("0", request.GetHeader(request_header::kContentLength));
    EXPECT_EQ("", request.GetHeader(request_header::kContentType));
    response->ReplyText(status_code::Ok, "blah",
                        chromeos::mime::text::kPlain);
  };

  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kFakeUrl, request_type::kHead, base::Bind(HeadHandler));

  auto response = http::Head(kFakeUrl, transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::text::kPlain, response->GetContentType());
  EXPECT_EQ("", response->GetDataAsString());  // Must not have actual body.
  EXPECT_EQ("4", response->GetHeader(request_header::kContentLength));
}

TEST(HttpUtils, PostBinary) {
  auto Handler = [](const fake::ServerRequest& request,
                    fake::ServerResponse* response) {
    EXPECT_EQ(request_type::kPost, request.GetMethod());
    EXPECT_EQ("256", request.GetHeader(request_header::kContentLength));
    EXPECT_EQ(chromeos::mime::application::kOctet_stream,
              request.GetHeader(request_header::kContentType));
    const auto& data = request.GetData();
    EXPECT_EQ(256, data.size());

    // Sum up all the bytes.
    int sum = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(32640, sum);  // sum(i, i => [0, 255]) = 32640.
    response->ReplyText(status_code::Ok, "", chromeos::mime::text::kPlain);
  };

  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kFakeUrl, request_type::kPost, base::Bind(Handler));

  /// Fill the data buffer with bytes from 0x00 to 0xFF.
  std::vector<unsigned char> data(256);
  std::iota(data.begin(), data.end(), 0);

  auto response = http::PostBinary(kFakeUrl, data.data(), data.size(),
                                   transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
}

TEST(HttpUtils, PostText) {
  std::string fake_data = "Some data";
  auto PostHandler = [fake_data](const fake::ServerRequest& request,
                                 fake::ServerResponse* response) {
    EXPECT_EQ(request_type::kPost, request.GetMethod());
    EXPECT_EQ(fake_data.size(),
              std::stoul(request.GetHeader(request_header::kContentLength)));
    EXPECT_EQ(chromeos::mime::text::kPlain,
              request.GetHeader(request_header::kContentType));
    response->ReplyText(status_code::Ok, request.GetDataAsString(),
                       chromeos::mime::text::kPlain);
  };

  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kFakeUrl, request_type::kPost, base::Bind(PostHandler));

  auto response = http::PostText(kFakeUrl, fake_data.c_str(),
                                 chromeos::mime::text::kPlain,
                                 transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::text::kPlain, response->GetContentType());
  EXPECT_EQ(fake_data, response->GetDataAsString());
}

TEST(HttpUtils, PostFormData) {
  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kFakeUrl, request_type::kPost,
                        base::Bind(EchoDataHandler));

  auto response = http::PostFormData(kFakeUrl, {
                      {"key", "value"},
                      {"field", "field value"},
                  }, transport, nullptr);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(chromeos::mime::application::kWwwFormUrlEncoded,
            response->GetContentType());
  EXPECT_EQ("key=value&field=field+value", response->GetDataAsString());
}

TEST(HttpUtils, PostPatchJson) {
  auto JsonHandler = [](const fake::ServerRequest& request,
                        fake::ServerResponse* response) {
    auto mime_type = chromeos::mime::RemoveParameters(
        request.GetHeader(request_header::kContentType));
    EXPECT_EQ(chromeos::mime::application::kJson, mime_type);
    response->ReplyJson(status_code::Ok, {
      {"method", request.GetMethod()},
      {"data", request.GetDataAsString()},
    });
  };
  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kFakeUrl, "*", base::Bind(JsonHandler));

  base::DictionaryValue json;
  json.SetString("key1", "val1");
  json.SetString("key2", "val2");
  std::string value;

  // Test POST
  auto response = http::PostJson(kFakeUrl, &json, transport, nullptr);
  auto resp_json = http::ParseJsonResponse(response.get(), nullptr, nullptr);
  EXPECT_NE(nullptr, resp_json.get());
  EXPECT_TRUE(resp_json->GetString("method", &value));
  EXPECT_EQ(request_type::kPost, value);
  EXPECT_TRUE(resp_json->GetString("data", &value));
  EXPECT_EQ("{\"key1\":\"val1\",\"key2\":\"val2\"}", value);

  // Test PATCH
  response = http::PatchJson(kFakeUrl, &json, transport, nullptr);
  resp_json = http::ParseJsonResponse(response.get(), nullptr, nullptr);
  EXPECT_NE(nullptr, resp_json.get());
  EXPECT_TRUE(resp_json->GetString("method", &value));
  EXPECT_EQ(request_type::kPatch, value);
  EXPECT_TRUE(resp_json->GetString("data", &value));
  EXPECT_EQ("{\"key1\":\"val1\",\"key2\":\"val2\"}", value);
}

TEST(HttpUtils, ParseJsonResponse) {
  auto JsonHandler = [](const fake::ServerRequest& request,
                        fake::ServerResponse* response) {
    int status_code = std::stoi(request.GetFormField("code"));
    response->ReplyJson(status_code, {{"data", request.GetFormField("value")}});
  };
  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(kFakeUrl, request_type::kPost, base::Bind(JsonHandler));

  // Test valid JSON responses (with success or error codes).
  for (auto item : {"200;data", "400;wrong", "500;Internal Server error"}) {
    auto pair = chromeos::string_utils::SplitAtFirst(item, ';');
    auto response = http::PostFormData(kFakeUrl, {
                      {"code", pair.first},
                      {"value", pair.second},
                    }, transport, nullptr);
    int code = 0;
    auto json = http::ParseJsonResponse(response.get(), &code, nullptr);
    EXPECT_NE(nullptr, json.get());
    std::string value;
    EXPECT_TRUE(json->GetString("data", &value));
    EXPECT_EQ(pair.first, chromeos::string_utils::ToString(code));
    EXPECT_EQ(pair.second, value);
  }

  // Test invalid (non-JSON) response.
  auto response = http::Get("http://bad.url", transport, nullptr);
  EXPECT_EQ(status_code::NotFound, response->GetStatusCode());
  EXPECT_EQ(chromeos::mime::text::kHtml, response->GetContentType());
  int code = 0;
  auto json = http::ParseJsonResponse(response.get(), &code, nullptr);
  EXPECT_EQ(nullptr, json.get());
  EXPECT_EQ(status_code::NotFound, code);
}

}  // namespace http
}  // namespace chromeos
