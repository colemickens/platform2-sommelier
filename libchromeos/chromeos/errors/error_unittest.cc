// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <gtest/gtest.h>

#include <chromeos/errors/error.h>

using chromeos::Error;

namespace {

chromeos::ErrorPtr GenerateNetworkError() {
  return Error::Create("network", "not_found", "Resource not found");
}

chromeos::ErrorPtr GenerateHttpError() {
  auto inner = GenerateNetworkError();
  return Error::Create("HTTP", "404", "Not found", std::move(inner));
}

}  // namespace

TEST(Error, Single) {
  auto err = GenerateNetworkError();
  EXPECT_EQ("network", err->GetDomain());
  EXPECT_EQ("not_found", err->GetCode());
  EXPECT_EQ("Resource not found", err->GetMessage());
  EXPECT_EQ(nullptr, err->GetInnerError());
  EXPECT_TRUE(err->HasDomain("network"));
  EXPECT_FALSE(err->HasDomain("HTTP"));
  EXPECT_FALSE(err->HasDomain("foo"));
  EXPECT_TRUE(err->HasError("network", "not_found"));
  EXPECT_FALSE(err->HasError("network", "404"));
  EXPECT_FALSE(err->HasError("HTTP", "404"));
  EXPECT_FALSE(err->HasError("HTTP", "not_found"));
  EXPECT_FALSE(err->HasError("foo", "bar"));
}

TEST(Error, Nested) {
  auto err = GenerateHttpError();
  EXPECT_EQ("HTTP", err->GetDomain());
  EXPECT_EQ("404", err->GetCode());
  EXPECT_EQ("Not found", err->GetMessage());
  EXPECT_NE(nullptr, err->GetInnerError());
  EXPECT_EQ("network", err->GetInnerError()->GetDomain());
  EXPECT_TRUE(err->HasDomain("network"));
  EXPECT_TRUE(err->HasDomain("HTTP"));
  EXPECT_FALSE(err->HasDomain("foo"));
  EXPECT_TRUE(err->HasError("network", "not_found"));
  EXPECT_FALSE(err->HasError("network", "404"));
  EXPECT_TRUE(err->HasError("HTTP", "404"));
  EXPECT_FALSE(err->HasError("HTTP", "not_found"));
  EXPECT_FALSE(err->HasError("foo", "bar"));
}
