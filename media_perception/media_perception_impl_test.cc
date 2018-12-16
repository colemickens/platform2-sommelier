// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include <base/bind.h>
#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "media_perception/device_management.pb.h"
#include "media_perception/fake_video_capture_service_client.h"
#include "media_perception/media_perception_impl.h"
#include "media_perception/proto_mojom_conversion.h"

namespace mri {
namespace {

class MediaPerceptionImplTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_vidcap_client_ = new FakeVideoCaptureServiceClient();
    vidcap_client_ = std::shared_ptr<VideoCaptureServiceClient>(
        fake_vidcap_client_);
    media_perception_impl_ = std::make_unique<MediaPerceptionImpl>(
        mojo::MakeRequest(&media_perception_ptr_),
        vidcap_client_);
  }

  chromeos::media_perception::mojom::MediaPerceptionPtr media_perception_ptr_;
  FakeVideoCaptureServiceClient* fake_vidcap_client_;
  std::shared_ptr<VideoCaptureServiceClient> vidcap_client_;
  std::unique_ptr<MediaPerceptionImpl> media_perception_impl_;
};

TEST_F(MediaPerceptionImplTest, TestGetDevices) {
  bool get_devices_callback_done = false;
  media_perception_ptr_->GetVideoDevices(
      base::Bind([](bool* get_devices_callback_done,
          std::vector<chromeos::media_perception::mojom::VideoDevicePtr>
          devices) {
        EXPECT_EQ(devices.size(), 0);
        *get_devices_callback_done = true;
      }, &get_devices_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(get_devices_callback_done);

  // Set up a couple of fake devices.
  std::vector<SerializedVideoDevice> serialized_devices;
  VideoDevice device;
  device.set_id("1");
  serialized_devices.push_back(SerializeVideoDeviceProto(device));
  device.set_id("2");
  serialized_devices.push_back(SerializeVideoDeviceProto(device));
  fake_vidcap_client_->SetDevicesForGetDevices(serialized_devices);

  get_devices_callback_done = false;
  media_perception_ptr_->GetVideoDevices(
        base::Bind([](bool *get_devices_callback_done,
          std::vector<chromeos::media_perception::mojom::VideoDevicePtr>
          devices) {
        EXPECT_EQ(devices.size(), 2);
        EXPECT_EQ(devices[0]->id, "1");
        EXPECT_EQ(devices[1]->id, "2");
        *get_devices_callback_done = true;
      }, &get_devices_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(get_devices_callback_done);
}

}  // namespace
}  // namespace mri
