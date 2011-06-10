// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "image_burner_impl.h"
#include "image_burner_test_utils.h"

namespace imageburn {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::AtMost;
using ::testing::StrEq;

const int kTestDataBlockSize = 8;
char kDataBlockLength8[9] = "12345678";
char kDataBlockLength3[4] = "abc";
char kDataBlockLength1[2] = "@";

ACTION_P(SetArgPointee, value) {
  *arg0 = value;
}

ACTION_P(SetArgCharString, value) {
  strcpy(arg0, value);
}

class ImageBurnerImplTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    burner_.reset(new BurnerImpl(&writer_,
                             &reader_,
                             &signal_sender_,
                             &root_path_getter_));
    EXPECT_CALL(signal_sender_, SendProgressSignal(_, _, _))
        .Times(0);
    EXPECT_CALL(writer_, Open(_))
        .Times(0);
    EXPECT_CALL(writer_, Close())
        .Times(0);
    EXPECT_CALL(writer_, Write(_, _))
        .Times(0);
    EXPECT_CALL(reader_, Open(_))
        .Times(0);
    EXPECT_CALL(reader_, Close())
        .Times(0);
    EXPECT_CALL(reader_, Read(_, _))
        .Times(0);
    EXPECT_CALL(reader_, GetSize())
        .Times(0);
    EXPECT_CALL(root_path_getter_, GetRootPath(_))
        .Times(AtMost(1))
        .WillRepeatedly(DoAll(SetArgPointee("/dev/sda"),
                              Return(true)));
    burner_->SetDataBlockSize(kTestDataBlockSize);
  }

  virtual void TearDown() {
  }

  virtual void SetFinishedSignalExpectation(const char* path, bool success) {
    EXPECT_CALL(signal_sender_,
                SendFinishedSignal(StrEq(path),
                                   success,
                                   StrEq(success ? "" : "Error during burn")))
        .Times(1);
  }

  virtual void SetUpEmptyFileMocks() {
    // These return values for empty file.
    EXPECT_CALL(reader_, Read(_, _))
        .Times(AtMost(1))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(reader_, GetSize())
        .Times(AtMost(1))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(writer_, Write(_, _))
        .Times(0);
  }

  virtual void SetUpDefaultOpenAndCloseMocks() {
    EXPECT_CALL(reader_, Open("some_path"))
       .WillOnce(Return(true));
    EXPECT_CALL(writer_, Close())
        .WillOnce(Return(true));
    EXPECT_CALL(reader_, Close())
        .WillOnce(Return(true));
    EXPECT_CALL(writer_, Open("/dev/sdb"))
        .WillOnce(Return(true));
  }

 protected:
  scoped_ptr<BurnerImpl> burner_;
  MockFileSystemWriter writer_;
  MockFileSystemReader reader_;
  MockSignalSender signal_sender_;
  MockRootPathGetter root_path_getter_;
};

TEST_F(ImageBurnerImplTest, BlankTargetPath) {
  SetFinishedSignalExpectation("", false);
  EXPECT_EQ(burner_->BurnImage("some_path", ""),
            imageburn::IMAGEBURN_ERROR_INVALID_TARGET_PATH);
}

TEST_F(ImageBurnerImplTest, NullTargetPath) {
  EXPECT_CALL(signal_sender_,
              SendFinishedSignal(NULL, false, StrEq("Error during burn")))
      .Times(1);
  EXPECT_EQ(burner_->BurnImage("some_path", NULL),
           imageburn::IMAGEBURN_ERROR_NULL_TARGET_PATH);
}

TEST_F(ImageBurnerImplTest, NullSourcePath) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  EXPECT_EQ(burner_->BurnImage(NULL, "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_NULL_SOURCE_PATH);
}

TEST_F(ImageBurnerImplTest, TargetIllegalFilePath) {
  SetFinishedSignalExpectation("/usr/local/chromeos_image.bin.zip", false);
  EXPECT_EQ(burner_->BurnImage("some_path",
                               "/usr/local/chromeos_image.bin.zip"),
            imageburn::IMAGEBURN_ERROR_INVALID_TARGET_PATH);
}

TEST_F(ImageBurnerImplTest, TargetNotAParent1) {
  SetFinishedSignalExpectation("/dev/sdb/sdb1", false);
  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb/sdb1"),
            imageburn::IMAGEBURN_ERROR_INVALID_TARGET_PATH);
}

TEST_F(ImageBurnerImplTest, TargetNotAParent2) {
  SetFinishedSignalExpectation("/dev/sdb1", false);
  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb1"),
            imageburn::IMAGEBURN_ERROR_INVALID_TARGET_PATH);
}

TEST_F(ImageBurnerImplTest, TargetPathEqualsRootPath) {
  SetFinishedSignalExpectation("/dev/sda", false);
  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sda"),
            imageburn::IMAGEBURN_ERROR_TARGET_PATH_ON_ROOT);
}

TEST_F(ImageBurnerImplTest, TargetPathIsOnTheRootPath) {
  SetFinishedSignalExpectation("/dev/sda/sda3", false);
  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sda/sda3"),
            imageburn::IMAGEBURN_ERROR_INVALID_TARGET_PATH);
}

TEST_F(ImageBurnerImplTest, TargetPathEqualsRootPathOtherThanSda) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  EXPECT_CALL(root_path_getter_, GetRootPath(_))
      .WillOnce(DoAll(SetArgPointee("/dev/sdb"),
                      Return(true)));
  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_TARGET_PATH_ON_ROOT);
}

TEST_F(ImageBurnerImplTest, RootPathCannotBeFound) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  EXPECT_CALL(root_path_getter_, GetRootPath(_))
      .WillOnce(Return(false));
  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_TARGET_PATH_ON_ROOT);
}

TEST_F(ImageBurnerImplTest, SourceFileCannotBeOpened) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpEmptyFileMocks();

  EXPECT_CALL(writer_, Close())
      .WillOnce(Return(true));
  EXPECT_CALL(reader_, Close())
      .WillOnce(Return(true));
  EXPECT_CALL(reader_, Open("some_path"))
      .WillOnce(Return(false));
  // We should not try to open target file if we don't open source file.
  EXPECT_CALL(writer_, Open("/dev/sdb"))
      .Times(0);

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_CANNOT_OPEN_SOURCE);
}

TEST_F(ImageBurnerImplTest, TargetFileCannotBeOpened) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpEmptyFileMocks();

  EXPECT_CALL(reader_, Open("some_path"))
     .WillOnce(Return(true));
  EXPECT_CALL(writer_, Close())
      .WillOnce(Return(true));
  EXPECT_CALL(reader_, Close())
      .WillOnce(Return(true));
  EXPECT_CALL(writer_, Open("/dev/sdb"))
      .WillOnce(Return(false));

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_CANNOT_OPEN_TARGET);
}

TEST_F(ImageBurnerImplTest, TargetFileCannotBeClosed) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpEmptyFileMocks();

  EXPECT_CALL(reader_, Open("some_path"))
     .WillOnce(Return(true));
  EXPECT_CALL(writer_, Open("/dev/sdb"))
      .WillOnce(Return(true));
  EXPECT_CALL(writer_, Close())
      .WillOnce(Return(false));
  EXPECT_CALL(reader_, Close())
      .WillOnce(Return(true));

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_CANNOT_CLOSE_TARGET);
}

TEST_F(ImageBurnerImplTest, SourceFileCannotBeClosed) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpEmptyFileMocks();

  EXPECT_CALL(reader_, Open("some_path"))
     .WillOnce(Return(true));
  EXPECT_CALL(writer_, Open("/dev/sdb"))
      .WillOnce(Return(true));
  EXPECT_CALL(writer_, Close())
      .WillOnce(Return(true));
  EXPECT_CALL(reader_, Close())
    .WillOnce(Return(false));

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_CANNOT_CLOSE_SOURCE);
}

TEST_F(ImageBurnerImplTest, TargetAndSourceFilesCannotBeClosed) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpEmptyFileMocks();

  EXPECT_CALL(reader_, Open("some_path"))
     .WillOnce(Return(true));
  EXPECT_CALL(writer_, Open("/dev/sdb"))
      .WillOnce(Return(true));
  EXPECT_CALL(writer_, Close())
    .WillOnce(Return(false));
  EXPECT_CALL(reader_, Close())
    .WillOnce(Return(false));

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_CANNOT_CLOSE_TARGET);
}

TEST_F(ImageBurnerImplTest, CloseTargetErrorDoesNotOverwritePreviousErrors) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpEmptyFileMocks();

  EXPECT_CALL(reader_, Open("some_path"))
     .WillOnce(Return(true));
  EXPECT_CALL(reader_, Close())
      .WillOnce(Return(true));
  // Let's set up an error.
  EXPECT_CALL(writer_, Open("/dev/sdb"))
      .WillOnce(Return(false));
  EXPECT_CALL(writer_, Close())
      .WillOnce(Return(false));

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_CANNOT_OPEN_TARGET);
}

TEST_F(ImageBurnerImplTest, CloseSourceErrorDoesNotOverwritePreviousErrors) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpEmptyFileMocks();

  EXPECT_CALL(reader_, Open("some_path"))
     .WillOnce(Return(true));
  EXPECT_CALL(writer_, Close())
      .WillOnce(Return(true));
  // Let's set up an error.
  EXPECT_CALL(writer_, Open("/dev/sdb"))
      .WillOnce(Return(false));
  EXPECT_CALL(reader_, Close())
      .WillOnce(Return(false));

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_CANNOT_OPEN_TARGET);
}

TEST_F(ImageBurnerImplTest, ErrorReadingFirstRead) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(21));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(Return(-1));
  EXPECT_CALL(writer_, Write(_, _))
      .Times(0);

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_FAILED_READING_SOURCE);
}

TEST_F(ImageBurnerImplTest, ErrorReadingFourthRead) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(53));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(Return(-1));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .Times(3)
      .WillRepeatedly(DoAll(SetArgCharString(kDataBlockLength8),
                            Return(kTestDataBlockSize)))
      .RetiresOnSaturation();
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength8), kTestDataBlockSize))
      .Times(3)
      .WillRepeatedly(Return(kTestDataBlockSize));
  EXPECT_CALL(signal_sender_, SendProgressSignal(24, 53, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(16, 53, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(8, 53, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_FAILED_READING_SOURCE);
}

TEST_F(ImageBurnerImplTest, ErrorWriting) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(53));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(DoAll(SetArgCharString(kDataBlockLength8),
                      Return(kTestDataBlockSize)));
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength8), kTestDataBlockSize))
      .WillOnce(Return(-1));

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_FAILED_WRITING_TO_TARGET);

}

TEST_F(ImageBurnerImplTest, ErrorWritingWrongReturn) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(53));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgCharString(kDataBlockLength8),
                            Return(kTestDataBlockSize)));
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength8), kTestDataBlockSize))
      .WillOnce(Return(8))
      .WillOnce(Return(7));
  EXPECT_CALL(signal_sender_, SendProgressSignal(8, 53, "/dev/sdb"))
      .Times(1);

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_FAILED_WRITING_TO_TARGET);
}

TEST_F(ImageBurnerImplTest, ErrorWritingLastBlockFileSize2x8) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(16));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgCharString(kDataBlockLength8),
                            Return(kTestDataBlockSize)));
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength8), kTestDataBlockSize))
      .WillOnce(Return(8))
      .WillOnce(Return(-1));
  EXPECT_CALL(signal_sender_, SendProgressSignal(8, 16, "/dev/sdb"))
      .Times(1);

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_FAILED_WRITING_TO_TARGET);
}

TEST_F(ImageBurnerImplTest, ErrorWritingLastBlock) {
  SetFinishedSignalExpectation("/dev/sdb", false);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(13));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(DoAll(SetArgCharString(kDataBlockLength8),
                            Return(kTestDataBlockSize)))
      .WillOnce(DoAll(SetArgCharString(kDataBlockLength3),
                            Return(3)));
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength3), 3))
      .WillOnce(Return(-1));
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength8), kTestDataBlockSize))
      .WillOnce(Return(8))
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(8, 13, "/dev/sdb"))
      .Times(1);

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_ERROR_FAILED_WRITING_TO_TARGET);
}


TEST_F(ImageBurnerImplTest, EmptyFile) {
  SetFinishedSignalExpectation("/dev/sdb", true);
  SetUpDefaultOpenAndCloseMocks();
  SetUpEmptyFileMocks();

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_OK);
}

TEST_F(ImageBurnerImplTest, FileSizeLessThanDataBlockSize) {
  SetFinishedSignalExpectation("/dev/sdb", true);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(3));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(Return(0));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(DoAll(SetArgCharString(kDataBlockLength3),
                      Return(3)))
      .RetiresOnSaturation();
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength3), 3))
      .WillOnce(Return(3))
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(3, 3, "/dev/sdb"))
      .Times(1);

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_OK);
}

TEST_F(ImageBurnerImplTest, FileSizeEqualsDataBlockSize) {
  SetFinishedSignalExpectation("/dev/sdb", true);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(8));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(Return(0));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(DoAll(SetArgCharString(kDataBlockLength8),
                      Return(kTestDataBlockSize)))
      .RetiresOnSaturation();
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength8), kTestDataBlockSize))
      .WillOnce(Return(kTestDataBlockSize))
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(8, 8, "/dev/sdb"))
      .Times(1);

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_OK);
}

TEST_F(ImageBurnerImplTest, FileSizeMultipleOfDataBlockSize) {
  SetFinishedSignalExpectation("/dev/sdb", true);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(24));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(Return(0));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .Times(3)
      .WillRepeatedly(DoAll(SetArgCharString(kDataBlockLength8),
                            Return(kTestDataBlockSize)))
      .RetiresOnSaturation();
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength8), kTestDataBlockSize))
      .Times(3)
      .WillRepeatedly(Return(kTestDataBlockSize));
  EXPECT_CALL(signal_sender_, SendProgressSignal(24, 24, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(16, 24, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(8, 24, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_OK);
}

TEST_F(ImageBurnerImplTest, FileSizeNotDivisibleByDAtaBlockSize) {
  SetFinishedSignalExpectation("/dev/sdb", true);
  SetUpDefaultOpenAndCloseMocks();

  EXPECT_CALL(reader_, GetSize())
      .WillOnce(Return(17));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(Return(0));
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .WillOnce(DoAll(SetArgCharString(kDataBlockLength1),
                      Return(1)))
      .RetiresOnSaturation();;
  EXPECT_CALL(reader_, Read(_, kTestDataBlockSize))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgCharString(kDataBlockLength8),
                            Return(kTestDataBlockSize)))
      .RetiresOnSaturation();
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength1), 1))
      .WillOnce(Return(1));
  EXPECT_CALL(writer_, Write(StrEq(kDataBlockLength8), kTestDataBlockSize))
      .Times(2)
      .WillRepeatedly(Return(kTestDataBlockSize))
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(17, 17, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(16, 17, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(signal_sender_, SendProgressSignal(8, 17, "/dev/sdb"))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_EQ(burner_->BurnImage("some_path", "/dev/sdb"),
            imageburn::IMAGEBURN_OK);
}

}  // namespace imageburn.
