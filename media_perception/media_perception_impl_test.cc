// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "media_perception/device_management.pb.h"
#include "media_perception/fake_rtanalytics.h"
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
    fake_rtanalytics_ = new FakeRtanalytics();
    rtanalytics_ = std::shared_ptr<Rtanalytics>(fake_rtanalytics_);
    media_perception_impl_ = std::make_unique<MediaPerceptionImpl>(
        mojo::MakeRequest(&media_perception_ptr_),
        vidcap_client_, rtanalytics_);
  }

  chromeos::media_perception::mojom::MediaPerceptionPtr media_perception_ptr_;
  FakeVideoCaptureServiceClient* fake_vidcap_client_;
  std::shared_ptr<VideoCaptureServiceClient> vidcap_client_;
  FakeRtanalytics* fake_rtanalytics_;
  std::shared_ptr<Rtanalytics> rtanalytics_;
  std::unique_ptr<MediaPerceptionImpl> media_perception_impl_;
};

TEST_F(MediaPerceptionImplTest, TestGetVideoDevices) {
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

TEST_F(MediaPerceptionImplTest, TestSetupConfiguration) {
  bool setup_configuration_callback_done = false;

  media_perception_ptr_->SetupConfiguration(
      "test_configuration",
      base::Bind([](
          bool* setup_configuration_callback_done,
          chromeos::media_perception::mojom::SuccessStatusPtr status,
          chromeos::media_perception::mojom::PerceptionInterfaceRequestsPtr
          requests) {
        EXPECT_EQ(status->success, true);
        EXPECT_EQ(*status->failure_reason, "test_configuration");
        *setup_configuration_callback_done = true;
      }, &setup_configuration_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(setup_configuration_callback_done);
}

TEST_F(MediaPerceptionImplTest, TestGetPipelineState) {
  bool get_pipeline_callback_done = false;

  media_perception_ptr_->GetPipelineState(
      "test_configuration",
      base::Bind([](
          bool* get_pipeline_callback_done,
          chromeos::media_perception::mojom::PipelineStatePtr pipeline_state) {
        EXPECT_EQ(pipeline_state->status,
                  chromeos::media_perception::mojom::PipelineStatus::SUSPENDED);
        *get_pipeline_callback_done = true;
      }, &get_pipeline_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(get_pipeline_callback_done);
}

TEST_F(MediaPerceptionImplTest, TestSetPipelineState) {
  bool set_pipeline_callback_done = false;

  chromeos::media_perception::mojom::PipelineStatePtr desired_pipeline_state =
      chromeos::media_perception::mojom::PipelineState::New();
  desired_pipeline_state->status =
      chromeos::media_perception::mojom::PipelineStatus::RUNNING;
  media_perception_ptr_->SetPipelineState(
      "test_configuration",
      std::move(desired_pipeline_state),
      base::Bind([](
          bool* set_pipeline_callback_done,
          chromeos::media_perception::mojom::PipelineStatePtr pipeline_state) {
        EXPECT_EQ(pipeline_state->status,
                  chromeos::media_perception::mojom::PipelineStatus::RUNNING);
        *set_pipeline_callback_done = true;
      }, &set_pipeline_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(set_pipeline_callback_done);
}

}  // namespace
}  // namespace mri
