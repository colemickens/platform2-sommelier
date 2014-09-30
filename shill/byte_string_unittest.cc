// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <endian.h>

#include <gtest/gtest.h>

#include <string>

#include "shill/byte_string.h"

using testing::Test;
using std::string;

namespace shill {

namespace {
const unsigned char kTest1[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
const char kTest1HexString[] = "00010203040506070809";
const char kTest1HexSubstring[] = "0203040506070809";
const char kTest1HexSubstringReordered[] = "0504030209080706";
const unsigned char kTest2[] = { 1, 2, 3, 0xa };
const char kTest2HexString[] = "0102030A";
const unsigned int kTest2Uint32 = 0x0102030a;
const unsigned char kTest3[] = { 0, 0, 0, 0 };
const char kTest4[] = "Hello world";
const unsigned char kTest5[] = { 1, 2, 3 };
}  // namespace

class ByteStringTest : public Test {
 public:
  bool IsCPUSameAsNetOrder() {
    const uint32_t kTestValue = 0x12345678;
    return htonl(kTestValue) == kTestValue;
  }

  void CalculateBitwiseAndResult(ByteString *bs,
                                 ByteString *mask,
                                 ByteString *expected_result,
                                 size_t count) {
    ASSERT_NE(nullptr, bs);
    ASSERT_NE(nullptr, mask);
    ASSERT_NE(nullptr, expected_result);

    for (size_t i = 0; i < count; i++) {
      EXPECT_FALSE(bs->BitwiseAnd(*mask));
      unsigned char val = count - i;
      mask->Append(ByteString(&val, 1));
      val &= bs->GetConstData()[i];
      expected_result->Append(ByteString(&val, 1));
    }
  }

  void CalculateBitwiseOrResult(ByteString *bs,
                                ByteString *merge,
                                ByteString *expected_result,
                                size_t count) {
    ASSERT_NE(nullptr, bs);
    ASSERT_NE(nullptr, merge);
    ASSERT_NE(nullptr, expected_result);

    for (size_t i = 0; i < count; i++) {
      EXPECT_FALSE(bs->BitwiseOr(*merge));
      unsigned char val = sizeof(kTest1) - i;
      merge->Append(ByteString(&val, 1));
      val |= bs->GetConstData()[i];
      expected_result->Append(ByteString(&val, 1));
    }
  }
};

TEST_F(ByteStringTest, Empty) {
  uint32_t val;

  ByteString bs1(0);
  EXPECT_TRUE(bs1.IsEmpty());
  EXPECT_EQ(0, bs1.GetLength());
  EXPECT_EQ(nullptr, bs1.GetData());
  EXPECT_FALSE(bs1.ConvertToNetUInt32(&val));
  EXPECT_TRUE(bs1.IsZero());
}

TEST_F(ByteStringTest, NonEmpty) {
  ByteString bs1(kTest1, sizeof(kTest1));
  uint32_t val;

  EXPECT_FALSE(bs1.IsEmpty());
  ASSERT_NE(nullptr, bs1.GetData());
  EXPECT_EQ(sizeof(kTest1), bs1.GetLength());
  for (unsigned int i = 0; i < sizeof(kTest1); i++) {
    EXPECT_EQ(bs1.GetData()[i], kTest1[i]);
  }
  EXPECT_FALSE(bs1.ConvertToNetUInt32(&val));
  EXPECT_FALSE(bs1.IsZero());

  // Build a ByteString (different to bs1), verify that the new ByteString
  // looks as expected, verify that it's different to bs1.
  ByteString bs2(kTest2, sizeof(kTest2));
  ASSERT_NE(nullptr, bs2.GetData());
  EXPECT_EQ(sizeof(kTest2), bs2.GetLength());
  for (unsigned int i = 0; i < sizeof(kTest2); i++) {
    EXPECT_EQ(bs2.GetData()[i], kTest2[i]);
  }
  EXPECT_FALSE(bs2.IsZero());
  EXPECT_FALSE(bs2.Equals(bs1));

  // Build _another_ ByteString (different to bs1 and bs2), verify that the
  // new ByteString looks as expected, verify that it's different to bs1 and
  // bs2.
  ByteString bs3(kTest3, sizeof(kTest3));
  ASSERT_NE(nullptr, bs3.GetData());
  EXPECT_EQ(sizeof(kTest3), bs3.GetLength());
  for (unsigned int i = 0; i < sizeof(kTest3); i++) {
    EXPECT_EQ(bs3.GetData()[i], kTest3[i]);
  }
  EXPECT_TRUE(bs3.IsZero());
  EXPECT_FALSE(bs2.Equals(bs1));
  EXPECT_FALSE(bs3.Equals(bs1));

  // Check two equal ByteStrings.
  ByteString bs6(kTest1, sizeof(kTest1));
  EXPECT_TRUE(bs6.Equals(bs1));
}

TEST_F(ByteStringTest, CopyTerminator) {
  ByteString bs4(string(kTest4), false);
  EXPECT_EQ(strlen(kTest4), bs4.GetLength());
  EXPECT_EQ(0, memcmp(kTest4, bs4.GetData(), bs4.GetLength()));

  ByteString bs5(string(kTest4), true);
  EXPECT_EQ(strlen(kTest4) + 1, bs5.GetLength());
  EXPECT_EQ(0, memcmp(kTest4, bs5.GetData(), bs5.GetLength()));
}

TEST_F(ByteStringTest, SubString) {
  ByteString bs1(kTest1, sizeof(kTest1));
  ByteString fragment(kTest1 + 3, 4);
  EXPECT_TRUE(fragment.Equals(bs1.GetSubstring(3, 4)));
  const int kMargin = sizeof(kTest1) - 3;
  ByteString end_fragment(kTest1 + kMargin, sizeof(kTest1) - kMargin);
  EXPECT_TRUE(end_fragment.Equals(bs1.GetSubstring(kMargin, sizeof(kTest1))));

  // Verify that the ByteString correctly handles accessing a substring
  // outside the range of the ByteString.
  const size_t kBogusOffset = 10;
  EXPECT_TRUE(bs1.GetSubstring(sizeof(kTest1), kBogusOffset).IsEmpty());
}

TEST_F(ByteStringTest, UInt32) {
  ByteString bs1 = ByteString::CreateFromNetUInt32(kTest2Uint32);
  uint32_t val;

  EXPECT_EQ(4, bs1.GetLength());
  ASSERT_NE(nullptr, bs1.GetData());
  EXPECT_TRUE(bs1.ConvertToNetUInt32(&val));
  EXPECT_EQ(kTest2Uint32, val);
  EXPECT_FALSE(bs1.IsZero());

  ByteString bs2(kTest2, sizeof(kTest2));
  EXPECT_TRUE(bs1.Equals(bs2));
  EXPECT_TRUE(bs2.ConvertToNetUInt32(&val));
  EXPECT_EQ(kTest2Uint32, val);

  ByteString bs3 = ByteString::CreateFromCPUUInt32(0x1020304);
  EXPECT_EQ(4, bs1.GetLength());
  ASSERT_NE(nullptr, bs3.GetData());
  EXPECT_TRUE(bs3.ConvertToCPUUInt32(&val));
  EXPECT_EQ(0x1020304, val);
  EXPECT_FALSE(bs3.IsZero());

#if __BYTE_ORDER == __LITTLE_ENDIAN
  EXPECT_FALSE(bs1.Equals(bs3));
#else
  EXPECT_TRUE(bs1.Equals(bs3));
#endif
}

TEST_F(ByteStringTest, Resize) {
  ByteString bs(kTest2, sizeof(kTest2));

  const size_t kSizeExtension = 10;
  bs.Resize(sizeof(kTest2) + kSizeExtension);
  EXPECT_EQ(sizeof(kTest2) + kSizeExtension, bs.GetLength());
  ASSERT_NE(nullptr, bs.GetData());
  EXPECT_EQ(0, memcmp(bs.GetData(), kTest2, sizeof(kTest2)));
  for (size_t i = sizeof(kTest2); i < sizeof(kTest2) + kSizeExtension; ++i) {
    EXPECT_EQ(0, bs.GetData()[i]);
  }

  const size_t kSizeReduction = 2;
  bs.Resize(sizeof(kTest2) - kSizeReduction);
  EXPECT_EQ(sizeof(kTest2) - kSizeReduction, bs.GetLength());
  EXPECT_EQ(0, memcmp(bs.GetData(), kTest2, sizeof(kTest2) - kSizeReduction));
}

TEST_F(ByteStringTest, HexEncode) {
  ByteString bs(kTest2, sizeof(kTest2));
  EXPECT_EQ(kTest2HexString, bs.HexEncode());
}

TEST_F(ByteStringTest, BitwiseAndWithAndWithoutOffsets) {
  const size_t kOffset[] = {0, 2, 7};
  for (size_t i = 0; i < arraysize(kOffset); ++i) {
    ByteString bs(kTest1, sizeof(kTest1));
    bs.RemovePrefix(kOffset[i]);
    ByteString mask;
    ByteString expected_result;
    CalculateBitwiseAndResult(&bs, &mask, &expected_result,
                              sizeof(kTest1) - kOffset[i]);
    EXPECT_TRUE(bs.BitwiseAnd(mask));
    EXPECT_TRUE(bs.Equals(expected_result));
    bs.Resize(sizeof(kTest1) - 1);
    EXPECT_FALSE(bs.BitwiseAnd(mask));
  }
}

TEST_F(ByteStringTest, BitwiseOrWithAndWithoutOffsets) {
  const size_t kOffset[] = {0, 2, 7};
  for (size_t i = 0; i < arraysize(kOffset); ++i) {
    ByteString bs(kTest1, sizeof(kTest1));
    bs.RemovePrefix(kOffset[i]);
    ByteString merge;
    ByteString expected_result;
    CalculateBitwiseOrResult(&bs, &merge, &expected_result,
                             sizeof(kTest1) - kOffset[i]);
    EXPECT_TRUE(bs.BitwiseOr(merge));
    EXPECT_TRUE(bs.Equals(expected_result));
    bs.Resize(sizeof(kTest1) - 1);
    EXPECT_FALSE(bs.BitwiseOr(merge));
  }
}

TEST_F(ByteStringTest, BitwiseInvertWithAndWithoutOffsets) {
  const size_t kOffset[] = {0, 2, 7};
  for (size_t i = 0; i < arraysize(kOffset); ++i) {
    ByteString bs(kTest1, sizeof(kTest1));
    bs.RemovePrefix(kOffset[i]);
    ByteString invert;
    for (size_t j = kOffset[i]; j < sizeof(kTest1); j++) {
      unsigned char val = kTest1[j] ^ 0xff;
      invert.Append(ByteString(&val, 1));
    }
    bs.BitwiseInvert();
    EXPECT_TRUE(bs.Equals(invert));
  }
}

// The tests, below, test various ByteString operations where some bytes have
// been removed from the beginning of one or more of the ByteStrings in the
// test.

TEST_F(ByteStringTest, EmptyOffset) {
  uint32_t val;

  ByteString bs1(kTest1, sizeof(kTest1));
  bs1.RemovePrefix(sizeof(kTest1));
  EXPECT_TRUE(bs1.IsEmpty());
  EXPECT_EQ(0, bs1.GetLength());
  EXPECT_EQ(nullptr, bs1.GetData());
  EXPECT_FALSE(bs1.ConvertToNetUInt32(&val));
  EXPECT_TRUE(bs1.IsZero());
}

TEST_F(ByteStringTest, NonEmptyOffset) {
  ByteString bs1(kTest1, sizeof(kTest1));
  const size_t kNewLength1 = 2;
  const size_t kOffset1 = sizeof(kTest1) - kNewLength1;

  {
    bs1.RemovePrefix(kOffset1);
    ASSERT_NE(nullptr, bs1.GetData());
    EXPECT_FALSE(bs1.IsEmpty());
    EXPECT_EQ(kNewLength1, bs1.GetLength());
    for (unsigned int i = kOffset1; i < sizeof(kTest1); i++) {
      EXPECT_EQ(bs1.GetData()[i - kOffset1], kTest1[i]);
    }
    uint32_t val;
    EXPECT_FALSE(bs1.ConvertToNetUInt32(&val));
    EXPECT_FALSE(bs1.IsZero());
  }

  // Check a non-equal ByteString.
  {
    const size_t kNewLength2 = 3;
    const size_t kOffset2 = sizeof(kTest2) - kNewLength2;
    ByteString bs2(kTest2, sizeof(kTest2));
    bs2.RemovePrefix(kOffset2);
    ASSERT_NE(nullptr, bs2.GetData());
    EXPECT_EQ(kNewLength2, bs2.GetLength());
    for (unsigned int i = kOffset2; i < sizeof(kTest2); i++) {
      EXPECT_EQ(bs2.GetData()[i - kOffset2], kTest2[i]);
    }
    EXPECT_FALSE(bs2.IsZero());
    EXPECT_FALSE(bs2.Equals(bs1));
  }

  // Check whether two equal ByteStrings are, in fact, equal.
  {
    ByteString bs6(kTest1, sizeof(kTest1));
    bs6.RemovePrefix(kOffset1);
    EXPECT_TRUE(bs6.Equals(bs1));
  }
}

TEST_F(ByteStringTest, CopyTerminatorOffset) {
  {
    ByteString bs4(string(kTest4), false);
    const size_t kOffset4 = 1;
    bs4.RemovePrefix(kOffset4);
    EXPECT_EQ(strlen(kTest4) - kOffset4, bs4.GetLength());
    EXPECT_EQ(0, memcmp(kTest4 + kOffset4, bs4.GetData(), bs4.GetLength()));
  }

  {
    ByteString bs5(string(kTest4), true);
    const size_t kOffset5 = 1;
    bs5.RemovePrefix(kOffset5);
    EXPECT_EQ(strlen(kTest4) + 1 - kOffset5, bs5.GetLength());
    EXPECT_EQ(0, memcmp(kTest4 + kOffset5, bs5.GetData(), bs5.GetLength()));
  }
}

TEST_F(ByteStringTest, SubStringOffset) {
  const size_t kFramgmetOffset = 3;
  const size_t kFragmentLength = 4;
  ByteString bs1(kTest1, sizeof(kTest1));
  ByteString fragment(kTest1, kFramgmetOffset + kFragmentLength);
  fragment.RemovePrefix(kFramgmetOffset);
  EXPECT_TRUE(fragment.Equals(bs1.GetSubstring(kFramgmetOffset,
                                               kFragmentLength)));

  const int kMargin = sizeof(kTest1) - kFramgmetOffset;
  ByteString end_fragment(kTest1 + kMargin, sizeof(kTest1) - kMargin);
  EXPECT_TRUE(end_fragment.Equals(bs1.GetSubstring(kMargin, sizeof(kTest1))));

  // Verify that the ByteString correctly handles accessing a substring
  // outside the range of the ByteString.
  const size_t kBogusOffset = 10;
  EXPECT_TRUE(bs1.GetSubstring(sizeof(kTest1), kBogusOffset).IsEmpty());
}

TEST_F(ByteStringTest, ResizeOffset) {
  ByteString bs(kTest2, sizeof(kTest2));
  const size_t kOffset = 1;
  bs.RemovePrefix(kOffset);

  const size_t kSizeExtension = 10;
  bs.Resize(sizeof(kTest2) + kSizeExtension);
  EXPECT_EQ(sizeof(kTest2) + kSizeExtension, bs.GetLength());
  ASSERT_NE(nullptr, bs.GetData());
  EXPECT_EQ(0, memcmp(bs.GetData(),
                      kTest2 + kOffset,
                      sizeof(kTest2) - kOffset));
  for (size_t i = sizeof(kTest2) - kOffset;
       i < sizeof(kTest2) + kSizeExtension; ++i) {
    EXPECT_EQ(0, bs.GetData()[i]);
  }

  const size_t kSizeReduction = 2;
  bs.Resize(sizeof(kTest2) - kSizeReduction);
  EXPECT_EQ(sizeof(kTest2) - kSizeReduction, bs.GetLength());
  EXPECT_EQ(0, memcmp(bs.GetData(), kTest2 + kOffset,
                      sizeof(kTest2) - kSizeReduction));
}

TEST_F(ByteStringTest, HexEncodeWithOffset) {
  ByteString bs(kTest2, sizeof(kTest2));
  const size_t kOffset = 2;
  const size_t kBytesPerHexDigit = 2;
  bs.RemovePrefix(kOffset);
  EXPECT_EQ(kTest2HexString + kOffset * kBytesPerHexDigit, bs.HexEncode());
}

TEST_F(ByteStringTest, ChopByteClear) {
  ByteString bs(kTest1, sizeof(kTest1));
  ByteString expected_result(kTest2, sizeof(kTest2));
  bs.RemovePrefix(5);
  bs.Clear();
  bs.Append(ByteString(kTest2, sizeof(kTest2)));

  EXPECT_TRUE(bs.Equals(expected_result));
}

TEST_F(ByteStringTest, CreateFromHexString) {
  ByteString bs = ByteString::CreateFromHexString("");
  EXPECT_TRUE(bs.IsEmpty());

  ByteString bs1 = ByteString::CreateFromHexString("0");
  EXPECT_TRUE(bs1.IsEmpty());

  ByteString bs2 = ByteString::CreateFromHexString("0y");
  EXPECT_TRUE(bs2.IsEmpty());

  ByteString bs3 = ByteString::CreateFromHexString("ab");
  EXPECT_EQ(1, bs3.GetLength());
  EXPECT_EQ(0xab, bs3.GetData()[0]);

  ByteString bs4 = ByteString::CreateFromHexString(kTest1HexString);
  EXPECT_EQ(kTest1HexString, bs4.HexEncode());
}

TEST_F(ByteStringTest, ConvertFromNetToCPUUInt32Array) {
  ByteString bs1;
  EXPECT_TRUE(bs1.ConvertFromNetToCPUUInt32Array());
  EXPECT_TRUE(bs1.IsEmpty());

  // Conversion should fail when the length of ByteString is not a
  // multiple of 4.
  ByteString bs2(kTest1, sizeof(kTest1));
  EXPECT_EQ(kTest1HexString, bs2.HexEncode());
  EXPECT_FALSE(bs2.ConvertFromNetToCPUUInt32Array());
  EXPECT_EQ(kTest1HexString, bs2.HexEncode());

  // Conversion should succeed when the length of ByteString is a
  // multiple of 4. Also test the case when bytes stored in ByteString
  // is not word-aligned after calling RemovePrefix().
  bs2.RemovePrefix(2);
  EXPECT_EQ(kTest1HexSubstring, bs2.HexEncode());
  EXPECT_TRUE(bs2.ConvertFromNetToCPUUInt32Array());
  if (IsCPUSameAsNetOrder()) {
    EXPECT_EQ(kTest1HexSubstring, bs2.HexEncode());
  } else {
    EXPECT_EQ(kTest1HexSubstringReordered, bs2.HexEncode());
  }
}

TEST_F(ByteStringTest, ConvertFromCPUToNetUInt32Array) {
  ByteString bs1;
  EXPECT_TRUE(bs1.ConvertFromCPUToNetUInt32Array());
  EXPECT_TRUE(bs1.IsEmpty());

  // Conversion should fail when the length of ByteString is not a
  // multiple of 4.
  ByteString bs2(kTest1, sizeof(kTest1));
  EXPECT_EQ(kTest1HexString, bs2.HexEncode());
  EXPECT_FALSE(bs2.ConvertFromCPUToNetUInt32Array());
  EXPECT_EQ(kTest1HexString, bs2.HexEncode());

  // Conversion should succeed when the length of ByteString is a
  // multiple of 4. Also test the case when bytes stored in ByteString
  // is not word-aligned after calling RemovePrefix().
  bs2.RemovePrefix(2);
  EXPECT_EQ(kTest1HexSubstring, bs2.HexEncode());
  EXPECT_TRUE(bs2.ConvertFromCPUToNetUInt32Array());
  if (IsCPUSameAsNetOrder()) {
    EXPECT_EQ(kTest1HexSubstring, bs2.HexEncode());
  } else {
    EXPECT_EQ(kTest1HexSubstringReordered, bs2.HexEncode());
  }
}

TEST_F(ByteStringTest, LessThan) {
  ByteString bs1(kTest1, sizeof(kTest1));
  ByteString bs2(kTest2, sizeof(kTest2));
  ByteString bs3(kTest3, sizeof(kTest3));
  ByteString bs5(kTest5, sizeof(kTest5));

  // bs2 is shorter, but the first four bytes of bs1 are less than those in bs2.
  EXPECT_TRUE(ByteString::IsLessThan(bs1, bs2));

  // bs2 and bs3 are the same length, but bs3 has less byte values.
  EXPECT_TRUE(ByteString::IsLessThan(bs3, bs2));

  // bs3 is shorter than bs1 and the first four bytes of bs3 are less than
  // the first four bytes of bs1.
  EXPECT_TRUE(ByteString::IsLessThan(bs3, bs1));

  // The first three bytes of bs5 are equal to the first three bytes of bs2,
  // but bs5 is shorter than bs2.
  EXPECT_TRUE(ByteString::IsLessThan(bs5, bs2));

  // A Bytestring is not less than another identical one.
  EXPECT_FALSE(ByteString::IsLessThan(bs5, bs5));
}

}  // namespace shill
