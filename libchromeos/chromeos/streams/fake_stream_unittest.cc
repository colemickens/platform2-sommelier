// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/streams/fake_stream.h>

#include <vector>

#include <base/callback.h>
#include <base/test/simple_test_clock.h>
#include <chromeos/bind_lambda.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::InSequence;
using testing::Invoke;
using testing::_;

namespace chromeos {

// Mock-like task runner that allow the tests to inspect the calls to
// TaskRunner::PostDelayedTask and verify the delays.
class TestTaskRunner : public base::TaskRunner {
 public:
  MOCK_METHOD3(PostDelayedTask, bool(const tracked_objects::Location&,
                                     const base::Closure&,
                                     base::TimeDelta));
  bool RunsTasksOnCurrentThread() const { return true; }
};

class FakeStreamTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = new TestTaskRunner;

    auto callback = [this](const tracked_objects::Location& from_here,
                           const base::Closure& task,
                           base::TimeDelta delay) -> bool {
      clock_.Advance(delay);
      task.Run();
      return true;
    };

    ON_CALL(*task_runner_, PostDelayedTask(_, _, _))
        .WillByDefault(Invoke(callback));
  }

  void CreateStream(Stream::AccessMode mode) {
    stream_.reset(new FakeStream{mode, task_runner_, &clock_});
  }

  // Performs non-blocking read on the stream and returns the read data
  // as a string in |out_buffer|. Returns true if the read was successful or
  // false when an error occurs. |*eos| is set to true when end of stream is
  // reached.
  bool ReadString(size_t size_to_read, std::string* out_buffer, bool* eos) {
    std::vector<char> data;
    data.resize(size_to_read);
    size_t size_read = 0;
    bool ok = stream_->ReadNonBlocking(data.data(), data.size(), &size_read,
                                       eos, nullptr);
    if (ok) {
      out_buffer->assign(data.data(), data.data() + size_read);
    } else {
      out_buffer->clear();
    }
    return ok;
  }

  // Writes a string to a stream. Returns the number of bytes written or -1
  // in case an error occurred.
  int WriteString(const std::string& data) {
    size_t written = 0;
    if (!stream_->WriteNonBlocking(data.data(), data.size(), &written, nullptr))
      return -1;
    return static_cast<int>(written);
  }

 protected:
  base::SimpleTestClock clock_;
  std::unique_ptr<FakeStream> stream_;
  scoped_refptr<TestTaskRunner> task_runner_;
  const base::TimeDelta zero_delay;
};

TEST_F(FakeStreamTest, InitReadOnly) {
  CreateStream(Stream::AccessMode::READ);
  EXPECT_TRUE(stream_->IsOpen());
  EXPECT_TRUE(stream_->CanRead());
  EXPECT_FALSE(stream_->CanWrite());
  EXPECT_FALSE(stream_->CanSeek());
  EXPECT_FALSE(stream_->CanGetSize());
  EXPECT_EQ(0, stream_->GetSize());
  EXPECT_EQ(0, stream_->GetRemainingSize());
  EXPECT_EQ(0, stream_->GetPosition());
}

TEST_F(FakeStreamTest, InitWriteOnly) {
  CreateStream(Stream::AccessMode::WRITE);
  EXPECT_TRUE(stream_->IsOpen());
  EXPECT_FALSE(stream_->CanRead());
  EXPECT_TRUE(stream_->CanWrite());
  EXPECT_FALSE(stream_->CanSeek());
  EXPECT_FALSE(stream_->CanGetSize());
  EXPECT_EQ(0, stream_->GetSize());
  EXPECT_EQ(0, stream_->GetRemainingSize());
  EXPECT_EQ(0, stream_->GetPosition());
}

TEST_F(FakeStreamTest, InitReadWrite) {
  CreateStream(Stream::AccessMode::READ_WRITE);
  EXPECT_TRUE(stream_->IsOpen());
  EXPECT_TRUE(stream_->CanRead());
  EXPECT_TRUE(stream_->CanWrite());
  EXPECT_FALSE(stream_->CanSeek());
  EXPECT_FALSE(stream_->CanGetSize());
  EXPECT_EQ(0, stream_->GetSize());
  EXPECT_EQ(0, stream_->GetRemainingSize());
  EXPECT_EQ(0, stream_->GetPosition());
}

TEST_F(FakeStreamTest, ReadEmpty) {
  CreateStream(Stream::AccessMode::READ);
  std::string data;
  bool eos = false;
  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_TRUE(eos);
  EXPECT_TRUE(data.empty());
}

TEST_F(FakeStreamTest, ReadFullPacket) {
  CreateStream(Stream::AccessMode::READ);
  stream_->AddReadPacketString({}, "foo");
  std::string data;
  bool eos = false;
  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("foo", data);

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_TRUE(eos);
  EXPECT_TRUE(data.empty());
}

TEST_F(FakeStreamTest, ReadPartialPacket) {
  CreateStream(Stream::AccessMode::READ);
  stream_->AddReadPacketString({}, "foobar");
  std::string data;
  bool eos = false;
  EXPECT_TRUE(ReadString(3, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("foo", data);

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("bar", data);

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_TRUE(eos);
  EXPECT_TRUE(data.empty());
}

TEST_F(FakeStreamTest, ReadMultiplePackets) {
  CreateStream(Stream::AccessMode::READ);
  stream_->AddReadPacketString({}, "foobar");
  stream_->AddReadPacketString({}, "baz");
  stream_->AddReadPacketString({}, "quux");
  std::string data;
  bool eos = false;
  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("foobar", data);

  EXPECT_TRUE(ReadString(2, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("ba", data);

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("z", data);

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("quux", data);

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_TRUE(eos);
  EXPECT_TRUE(data.empty());

  stream_->AddReadPacketString({}, "foo-bar");
  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("foo-bar", data);
}

TEST_F(FakeStreamTest, ReadPacketsWithDelay) {
  CreateStream(Stream::AccessMode::READ);
  stream_->AddReadPacketString({}, "foobar");
  stream_->AddReadPacketString(base::TimeDelta::FromSeconds(1), "baz");
  std::string data;
  bool eos = false;
  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("foobar", data);

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_TRUE(data.empty());

  clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("baz", data);
}

TEST_F(FakeStreamTest, ReadPacketsWithError) {
  CreateStream(Stream::AccessMode::READ);
  stream_->AddReadPacketString({}, "foobar");
  stream_->QueueReadErrorWithMessage(base::TimeDelta::FromSeconds(1),
                                     "Dummy error");
  stream_->AddReadPacketString({}, "baz");

  std::string data;
  bool eos = false;
  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("foobar", data);

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_TRUE(data.empty());

  clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_FALSE(ReadString(100, &data, &eos));

  EXPECT_TRUE(ReadString(100, &data, &eos));
  EXPECT_FALSE(eos);
  EXPECT_EQ("baz", data);
}

TEST_F(FakeStreamTest, WaitForDataRead) {
  CreateStream(Stream::AccessMode::READ);

  EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, zero_delay)).Times(2);

  int call_count = 0;
  auto callback = [&call_count](Stream::AccessMode mode) {
    call_count++;
    EXPECT_EQ(Stream::AccessMode::READ, mode);
  };

  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::READ,
                                   base::Bind(callback), nullptr));
  EXPECT_EQ(1, call_count);

  stream_->AddReadPacketString({}, "foobar");
  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::READ,
                                   base::Bind(callback), nullptr));
  EXPECT_EQ(2, call_count);

  stream_->ClearReadQueue();

  auto one_sec_delay = base::TimeDelta::FromSeconds(1);
  stream_->AddReadPacketString(one_sec_delay, "baz");
  EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, one_sec_delay)).Times(1);
  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::READ,
                                   base::Bind(callback), nullptr));
  EXPECT_EQ(3, call_count);
}

TEST_F(FakeStreamTest, ReadAsync) {
  CreateStream(Stream::AccessMode::READ);
  std::string input_data = "foobar-baz";
  size_t split_pos = input_data.find('-');

  auto one_sec_delay = base::TimeDelta::FromSeconds(1);
  stream_->AddReadPacketString({}, input_data.substr(0, split_pos));
  stream_->AddReadPacketString(one_sec_delay, input_data.substr(split_pos));

  {
    InSequence seq;
    EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, zero_delay)).Times(1);
    EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, one_sec_delay)).Times(1);
  }

  std::vector<char> buffer;
  buffer.resize(input_data.size());

  int success_count = 0;
  int error_count = 0;
  auto on_success = [&success_count] { success_count++; };
  auto on_failure = [&error_count](const Error* error) { error_count++; };

  EXPECT_TRUE(stream_->ReadAllAsync(buffer.data(), buffer.size(),
                                    base::Bind(on_success),
                                    base::Bind(on_failure),
                                    nullptr));
  EXPECT_EQ(1, success_count);
  EXPECT_EQ(0, error_count);
  EXPECT_EQ(input_data, (std::string{buffer.begin(), buffer.end()}));
}

TEST_F(FakeStreamTest, WriteEmpty) {
  CreateStream(Stream::AccessMode::WRITE);
  EXPECT_EQ(-1, WriteString("foo"));
}

TEST_F(FakeStreamTest, WritePartial) {
  CreateStream(Stream::AccessMode::WRITE);
  stream_->ExpectWritePacketSize({}, 6);
  EXPECT_EQ(3, WriteString("foo"));
  EXPECT_EQ(3, WriteString("bar"));
  EXPECT_EQ(-1, WriteString("baz"));

  EXPECT_EQ("foobar", stream_->GetFlushedOutputDataAsString());
}

TEST_F(FakeStreamTest, WriteFullPackets) {
  CreateStream(Stream::AccessMode::WRITE);

  stream_->ExpectWritePacketSize({}, 3);
  EXPECT_EQ(3, WriteString("foo"));
  EXPECT_EQ(-1, WriteString("bar"));

  stream_->ExpectWritePacketSize({}, 3);
  EXPECT_EQ(3, WriteString("bar"));

  stream_->ExpectWritePacketSize({}, 3);
  EXPECT_EQ(3, WriteString("quux"));

  EXPECT_EQ("foobarquu", stream_->GetFlushedOutputDataAsString());
}

TEST_F(FakeStreamTest, WriteAndVerifyData) {
  CreateStream(Stream::AccessMode::WRITE);

  stream_->ExpectWritePacketString({}, "foo");
  stream_->ExpectWritePacketString({}, "bar");
  EXPECT_EQ(3, WriteString("foobar"));
  EXPECT_EQ(3, WriteString("bar"));

  stream_->ExpectWritePacketString({}, "foo");
  stream_->ExpectWritePacketString({}, "baz");
  EXPECT_EQ(3, WriteString("foobar"));
  EXPECT_EQ(-1, WriteString("bar"));

  stream_->ExpectWritePacketString({}, "foobar");
  EXPECT_EQ(3, WriteString("foo"));
  EXPECT_EQ(2, WriteString("ba"));
  EXPECT_EQ(-1, WriteString("z"));
}

TEST_F(FakeStreamTest, WriteWithDelay) {
  CreateStream(Stream::AccessMode::WRITE);

  const auto delay = base::TimeDelta::FromMilliseconds(500);

  stream_->ExpectWritePacketSize({}, 3);
  stream_->ExpectWritePacketSize(delay, 3);
  EXPECT_EQ(3, WriteString("foobar"));

  EXPECT_EQ(0, WriteString("bar"));
  EXPECT_EQ(0, WriteString("bar"));
  clock_.Advance(delay);
  EXPECT_EQ(3, WriteString("bar"));

  EXPECT_EQ("foobar", stream_->GetFlushedOutputDataAsString());
}

TEST_F(FakeStreamTest, WriteWithError) {
  CreateStream(Stream::AccessMode::WRITE);

  const auto delay = base::TimeDelta::FromMilliseconds(500);

  stream_->ExpectWritePacketSize({}, 3);
  stream_->QueueWriteError({});
  stream_->ExpectWritePacketSize({}, 3);
  stream_->QueueWriteErrorWithMessage(delay, "Dummy message");
  stream_->ExpectWritePacketString({}, "foobar");

  const std::string data = "foobarbaz";
  EXPECT_EQ(3, WriteString(data));
  EXPECT_EQ(-1, WriteString(data));  // Simulated error #1.
  EXPECT_EQ(3, WriteString(data));
  EXPECT_EQ(0, WriteString(data));  // Waiting for data...
  clock_.Advance(delay);
  EXPECT_EQ(-1, WriteString(data));  // Simulated error #2.
  EXPECT_EQ(6, WriteString(data));
  EXPECT_EQ(-1, WriteString(data));  // No more data expected.
}

TEST_F(FakeStreamTest, WaitForDataWrite) {
  CreateStream(Stream::AccessMode::WRITE);

  EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, zero_delay)).Times(2);

  int call_count = 0;
  auto callback = [&call_count](Stream::AccessMode mode) {
    call_count++;
    EXPECT_EQ(Stream::AccessMode::WRITE, mode);
  };

  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::WRITE,
                                   base::Bind(callback), nullptr));
  EXPECT_EQ(1, call_count);

  stream_->ExpectWritePacketString({}, "foobar");
  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::WRITE,
                                   base::Bind(callback), nullptr));
  EXPECT_EQ(2, call_count);

  stream_->ClearWriteQueue();

  auto one_sec_delay = base::TimeDelta::FromSeconds(1);
  stream_->ExpectWritePacketString(one_sec_delay, "baz");
  EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, one_sec_delay)).Times(1);
  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::WRITE,
                                   base::Bind(callback), nullptr));
  EXPECT_EQ(3, call_count);
}

TEST_F(FakeStreamTest, WriteAsync) {
  CreateStream(Stream::AccessMode::WRITE);
  std::string output_data = "foobar-baz";
  size_t split_pos = output_data.find('-');

  auto one_sec_delay = base::TimeDelta::FromSeconds(1);
  stream_->ExpectWritePacketString({}, output_data.substr(0, split_pos));
  stream_->ExpectWritePacketString(one_sec_delay,
                                   output_data.substr(split_pos));

  {
    InSequence seq;
    EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, zero_delay)).Times(1);
    EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, one_sec_delay)).Times(1);
  }

  int success_count = 0;
  int error_count = 0;
  auto on_success = [&success_count] { success_count++; };
  auto on_failure = [&error_count](const Error* error) { error_count++; };

  EXPECT_TRUE(stream_->WriteAllAsync(output_data.data(), output_data.size(),
                                     base::Bind(on_success),
                                     base::Bind(on_failure),
                                     nullptr));
  EXPECT_EQ(1, success_count);
  EXPECT_EQ(0, error_count);
  EXPECT_EQ(output_data, stream_->GetFlushedOutputDataAsString());
}

TEST_F(FakeStreamTest, WaitForDataReadWrite) {
  CreateStream(Stream::AccessMode::READ_WRITE);
  auto one_sec_delay = base::TimeDelta::FromSeconds(1);
  auto two_sec_delay = base::TimeDelta::FromSeconds(2);

  int call_count = 0;
  auto callback = [&call_count](Stream::AccessMode mode,
                                Stream::AccessMode expected_mode) {
    call_count++;
    EXPECT_EQ(static_cast<int>(expected_mode), static_cast<int>(mode));
  };

  stream_->AddReadPacketString(one_sec_delay, "foo");
  stream_->ExpectWritePacketString(two_sec_delay, "bar");

  EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, one_sec_delay)).Times(1);
  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::READ_WRITE,
                                   base::Bind(callback,
                                              Stream::AccessMode::READ),
                                   nullptr));
  EXPECT_EQ(1, call_count);

  // The above step has adjusted the clock by 1 second already.
  stream_->ClearReadQueue();
  stream_->AddReadPacketString(two_sec_delay, "foo");
  EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, one_sec_delay)).Times(1);
  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::READ_WRITE,
                                   base::Bind(callback,
                                              Stream::AccessMode::WRITE),
                                   nullptr));
  EXPECT_EQ(2, call_count);

  clock_.Advance(one_sec_delay);

  EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, zero_delay)).Times(1);
  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::READ_WRITE,
                                   base::Bind(callback,
                                              Stream::AccessMode::READ_WRITE),
                                   nullptr));
  EXPECT_EQ(3, call_count);

  stream_->ClearReadQueue();
  stream_->ClearWriteQueue();
  stream_->AddReadPacketString(one_sec_delay, "foo");
  stream_->ExpectWritePacketString(one_sec_delay, "bar");

  EXPECT_CALL(*task_runner_, PostDelayedTask(_, _, one_sec_delay)).Times(1);
  EXPECT_TRUE(stream_->WaitForData(Stream::AccessMode::READ_WRITE,
                                   base::Bind(callback,
                                              Stream::AccessMode::READ_WRITE),
                                   nullptr));
  EXPECT_EQ(4, call_count);
}

}  // namespace chromeos
