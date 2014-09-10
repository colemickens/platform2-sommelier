// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/indented_text.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using std::string;
using std::vector;
using testing::ElementsAre;
using testing::Test;

namespace chromeos_dbus_bindings {

class IndentedTextTest : public Test {
 protected:
  size_t GetOffset() const { return text_.offset_; }
  const vector<size_t>& GetHistory() const { return text_.offset_history_; }
  IndentedText text_;
};

TEST_F(IndentedTextTest, Constructor) {
  EXPECT_EQ("", text_.GetContents());
  EXPECT_EQ(0, GetOffset());
  EXPECT_TRUE(GetHistory().empty());
}

TEST_F(IndentedTextTest, AddLine) {
  const char kTestString0[] = "test";
  text_.AddLine(kTestString0);
  EXPECT_EQ(string(kTestString0) + "\n", text_.GetContents());
  EXPECT_EQ(0, GetOffset());
  EXPECT_TRUE(GetHistory().empty());

  const char kTestString1[] = "me";
  text_.AddLine(kTestString1);
  EXPECT_EQ(string(kTestString0) + "\n" + kTestString1 + "\n",
            text_.GetContents());
  EXPECT_EQ(0, GetOffset());
  EXPECT_TRUE(GetHistory().empty());
}

TEST_F(IndentedTextTest, AddLineWithOffset) {
  const char kTestString[] = "test";
  const int kShift = 4;
  text_.AddLineWithOffset(kTestString, kShift);
  EXPECT_EQ(string(kShift, ' ') + kTestString + "\n", text_.GetContents());
}

TEST_F(IndentedTextTest, AddBlock) {
  IndentedText block0;
  const char kTestString[] = "test";
  block0.AddLineWithOffset(kTestString, 10);
  block0.AddLineWithOffset(kTestString, 20);
  IndentedText block1;
  block1.AddLineWithOffset(kTestString, 5);
  block1.AddLineWithOffset(kTestString, 15);
  text_.AddBlock(block0);
  text_.AddBlock(block1);
  EXPECT_EQ(block0.GetContents() + block1.GetContents(), text_.GetContents());
}

TEST_F(IndentedTextTest, AddBlockWithOffset) {
  const char kTestString[] = "test";
  IndentedText block;
  const int kOffset0 = 0;
  block.AddLineWithOffset(kTestString, kOffset0);
  const int kOffset1 = 4;
  block.AddLineWithOffset(kTestString, kOffset1);
  const int kOffset2 = 20;
  text_.AddBlockWithOffset(block, kOffset2);
  EXPECT_EQ(string(kOffset2 + kOffset0, ' ') + kTestString + "\n" +
            string(kOffset2 + kOffset1, ' ') + kTestString + "\n",
            text_.GetContents());
}

TEST_F(IndentedTextTest, PushPop) {
  const char kTestString[] = "test";
  text_.AddLine(kTestString);

  const int kShift0 = 2;
  text_.PushOffset(kShift0);
  EXPECT_EQ(2, GetOffset());
  EXPECT_THAT(GetHistory(), ElementsAre(kShift0));
  text_.AddLine(kTestString);

  const int kShift1 = 4;
  text_.PushOffset(kShift1);
  EXPECT_EQ(kShift0 + kShift1, GetOffset());
  EXPECT_THAT(GetHistory(), ElementsAre(kShift0, kShift1));
  text_.AddLine(kTestString);

  text_.PopOffset();
  text_.AddLine(kTestString);
  EXPECT_EQ(2, GetOffset());
  EXPECT_THAT(GetHistory(), ElementsAre(kShift0));

  text_.PopOffset();
  text_.AddLine(kTestString);
  EXPECT_EQ(0, GetOffset());
  EXPECT_TRUE(GetHistory().empty());

  EXPECT_EQ(string(kTestString) + "\n" +
            string(kShift0, ' ') + kTestString + "\n" +
            string(kShift0 + kShift1, ' ') + kTestString + "\n" +
            string(kShift0, ' ') + kTestString + "\n" +
            string(kTestString) + "\n",
            text_.GetContents());
}

TEST_F(IndentedTextTest, Reset) {
  text_.PushOffset(10);
  text_.AddLine("test");
  EXPECT_NE("", text_.GetContents());
  EXPECT_NE(0, GetOffset());
  EXPECT_FALSE(GetHistory().empty());
  text_.Reset();
  EXPECT_EQ("", text_.GetContents());
  EXPECT_EQ(0, GetOffset());
  EXPECT_TRUE(GetHistory().empty());
}

}  // namespace chromeos_dbus_bindings
