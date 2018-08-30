// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/uri.h"

#include <gtest/gtest.h>

namespace cros_disks {

TEST(UriTest, IsUri) {
  EXPECT_TRUE(Uri::IsUri("foo://path"));
  EXPECT_TRUE(Uri::IsUri("foo.bar://path"));
  EXPECT_TRUE(Uri::IsUri("foo-bar://path"));
  EXPECT_TRUE(Uri::IsUri("foo+bar://path"));
  EXPECT_TRUE(Uri::IsUri("foo://"));

  EXPECT_FALSE(Uri::IsUri("foo:/path"));
  EXPECT_FALSE(Uri::IsUri("foo//path"));
  EXPECT_FALSE(Uri::IsUri("foo/path"));
  EXPECT_FALSE(Uri::IsUri("://path"));

  EXPECT_FALSE(Uri::IsUri("foo_bar://path"));
  EXPECT_FALSE(Uri::IsUri("foo=bar://path"));
  EXPECT_FALSE(Uri::IsUri("foo@bar://path"));
  EXPECT_FALSE(Uri::IsUri(".bar://path"));
  EXPECT_FALSE(Uri::IsUri("-bar://path"));
  EXPECT_FALSE(Uri::IsUri("+bar://path"));
}

TEST(UriTest, Parse) {
  Uri uri = Uri::Parse("foo://path");
  EXPECT_EQ("foo", uri.scheme());
  EXPECT_EQ("path", uri.path());
  EXPECT_EQ("foo://path", uri.value());

  uri = Uri::Parse("foo.bar-baz+boo://correct:horse@battery:staple/etc/passwd");
  EXPECT_EQ("foo.bar-baz+boo", uri.scheme());
  EXPECT_EQ("correct:horse@battery:staple/etc/passwd", uri.path());
  EXPECT_EQ("foo.bar-baz+boo://correct:horse@battery:staple/etc/passwd",
            uri.value());
}

TEST(UriTest, ParseInvalid) {
  EXPECT_DEATH(Uri::Parse("foo:/path"), ".*");
  EXPECT_DEATH(Uri::Parse("foo//path"), ".*");
  EXPECT_DEATH(Uri::Parse("foo/path"), ".*");
  EXPECT_DEATH(Uri::Parse("://path"), ".*");

  EXPECT_DEATH(Uri::Parse("foo_bar://path"), ".*");
  EXPECT_DEATH(Uri::Parse("foo=bar://path"), ".*");
  EXPECT_DEATH(Uri::Parse("foo@bar://path"), ".*");
  EXPECT_DEATH(Uri::Parse(".bar://path"), ".*");
  EXPECT_DEATH(Uri::Parse("-bar://path"), ".*");
  EXPECT_DEATH(Uri::Parse("+bar://path"), ".*");
}

}  // namespace cros_disks
