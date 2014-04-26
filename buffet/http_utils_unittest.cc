// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "buffet/bind_lambda.h"
#include "buffet/http_utils.h"
#include "buffet/http_transport_fake.h"
#include "buffet/mime_utils.h"
#include "buffet/url_utils.h"

using namespace chromeos;
using namespace chromeos::http;

static const char fake_url[] = "http://localhost";

TEST(HttpUtils, PostText) {
  std::string fake_data = "Some data";
  auto PostHandler = [fake_data](const fake::ServerRequest& request,
                                 fake::ServerResponse* response) {
    EXPECT_EQ(request_type::kPost, request.GetMethod());
    EXPECT_EQ(fake_data.size(),
              atoi(request.GetHeader(request_header::kContentLength).c_str()));
    EXPECT_EQ(mime::text::kPlain,
              request.GetHeader(request_header::kContentType));
    response->Reply(status_code::Ok, request.GetData(), mime::text::kPlain);
  };

  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(fake_url, request_type::kPost, base::Bind(PostHandler));

  auto response = http::PostText(fake_url, fake_data.c_str(),
                                 mime::text::kPlain, transport);
  EXPECT_TRUE(response->IsSuccessful());
  EXPECT_EQ(mime::text::kPlain, response->GetContentType());
  EXPECT_EQ(fake_data, response->GetDataAsString());
}

TEST(HttpUtils, Get) {
  auto GetHandler = [](const fake::ServerRequest& request,
                       fake::ServerResponse* response) {
    EXPECT_EQ(request_type::kGet, request.GetMethod());
    EXPECT_EQ("0", request.GetHeader(request_header::kContentLength));
    EXPECT_EQ("", request.GetHeader(request_header::kContentType));
    response->ReplyText(status_code::Ok, request.GetFormField("test"),
                       mime::text::kPlain);
  };

  std::shared_ptr<fake::Transport> transport(new fake::Transport);
  transport->AddHandler(fake_url, request_type::kGet, base::Bind(GetHandler));

  for (std::string data : {"blah", "some data", ""}) {
    std::string url = url::AppendQueryParam(fake_url, "test", data);
    EXPECT_EQ(data, http::GetAsString(url, transport));
  }
}
