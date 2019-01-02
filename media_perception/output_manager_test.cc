// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "media_perception/fake_rtanalytics.h"
#include "media_perception/frame_perception.pb.h"
#include "media_perception/hotword_detection.pb.h"
#include "media_perception/output_manager.h"
#include "media_perception/presence_perception.pb.h"
#include "media_perception/proto_mojom_conversion.h"
#include "media_perception/serialized_proto.h"

namespace mri {
namespace {

class FramePerceptionHandlerImpl :
  public chromeos::media_perception::mojom::FramePerceptionHandler {
 public:
  FramePerceptionHandlerImpl(
      chromeos::media_perception::mojom::FramePerceptionHandlerRequest
      request) : binding_(this, std::move(request)) {
    LOG(INFO) << "Binding is bound: " << binding_.is_bound();
  }

  void OnFramePerception(
      chromeos::media_perception::mojom::FramePerceptionPtr
      frame_perception) override {
    frame_perception_ = ToProto(frame_perception);
  }

  FramePerception frame_perception_;

  mojo::Binding<chromeos::media_perception::mojom::FramePerceptionHandler>
      binding_;
};

class HotwordDetectionHandlerImpl :
  public chromeos::media_perception::mojom::HotwordDetectionHandler {
 public:
  HotwordDetectionHandlerImpl(
      chromeos::media_perception::mojom::HotwordDetectionHandlerRequest
      request) : binding_(this, std::move(request)) {
    LOG(INFO) << "Binding is bound: " << binding_.is_bound();
  }

  void OnHotwordDetection(
      chromeos::media_perception::mojom::HotwordDetectionPtr
      hotword_detection) override {
    hotword_detection_ = ToProto(hotword_detection);
  }

  HotwordDetection hotword_detection_;

  mojo::Binding<chromeos::media_perception::mojom::HotwordDetectionHandler>
      binding_;
};

class PresencePerceptionHandlerImpl :
  public chromeos::media_perception::mojom::PresencePerceptionHandler {
 public:
  PresencePerceptionHandlerImpl(
      chromeos::media_perception::mojom::PresencePerceptionHandlerRequest
      request) : binding_(this, std::move(request)) {
    LOG(INFO) << "Binding is bound: " << binding_.is_bound();
  }

  void OnPresencePerception(
      chromeos::media_perception::mojom::PresencePerceptionPtr
      presence_perception) override {
    presence_perception_ = ToProto(presence_perception);
  }

  PresencePerception presence_perception_;

  mojo::Binding<chromeos::media_perception::mojom::PresencePerceptionHandler>
      binding_;
};


class OutputManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_rtanalytics_ = new FakeRtanalytics();
    rtanalytics_ = std::shared_ptr<Rtanalytics>(fake_rtanalytics_);
  }

  FakeRtanalytics* fake_rtanalytics_;
  std::shared_ptr<Rtanalytics> rtanalytics_;
};

TEST_F(OutputManagerTest, FramePerceptionOutputManagerTest) {
  PerceptionInterfaces perception_interfaces;
  PerceptionInterface* interface = perception_interfaces.add_interface();
  interface->set_interface_type(
      PerceptionInterfaceType::INTERFACE_FRAME_PERCEPTION);
  PipelineOutput* output = interface->add_output();
  output->set_output_type(
      PipelineOutputType::OUTPUT_FRAME_PERCEPTION);
  output->set_stream_name("fake_stream_name");

  chromeos::media_perception::mojom::PerceptionInterfacesPtr interfaces_ptr =
      chromeos::media_perception::mojom::PerceptionInterfaces::New();
  OutputManager output_manager(
      "fake_frame_perception_configuration",
      rtanalytics_,
      perception_interfaces,
      &interfaces_ptr);
  // Verify that the mojo interface was created correctly.
  EXPECT_TRUE(interfaces_ptr->frame_perception_handler_request.is_pending());
  EXPECT_EQ(fake_rtanalytics_->GetMostRecentOutputStreamName(),
            "fake_stream_name");

  FramePerceptionHandlerImpl frame_perception_handler_impl(
      std::move(interfaces_ptr->frame_perception_handler_request));
  base::RunLoop().RunUntilIdle();

  FramePerception frame_perception;
  frame_perception.set_frame_id(1);
  output_manager.HandleFramePerception(
      Serialized<FramePerception>(frame_perception).GetBytes());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      frame_perception_handler_impl.frame_perception_.frame_id(), 1);
}

TEST_F(OutputManagerTest, HotwordDetectionOutputManagerTest) {
  PerceptionInterfaces perception_interfaces;
  PerceptionInterface* interface = perception_interfaces.add_interface();
  interface->set_interface_type(
      PerceptionInterfaceType::INTERFACE_HOTWORD_DETECTION);
  PipelineOutput* output = interface->add_output();
  output->set_output_type(
      PipelineOutputType::OUTPUT_HOTWORD_DETECTION);
  output->set_stream_name("fake_stream_name");

  chromeos::media_perception::mojom::PerceptionInterfacesPtr interfaces_ptr =
      chromeos::media_perception::mojom::PerceptionInterfaces::New();
  OutputManager output_manager(
      "fake_hotword_detection_configuration",
      rtanalytics_,
      perception_interfaces,
      &interfaces_ptr);
  // Verify that the mojo interface was created correctly.
  EXPECT_TRUE(interfaces_ptr->hotword_detection_handler_request.is_pending());
  EXPECT_EQ(fake_rtanalytics_->GetMostRecentOutputStreamName(),
            "fake_stream_name");

  HotwordDetectionHandlerImpl hotword_detection_handler_impl(
      std::move(interfaces_ptr->hotword_detection_handler_request));
  base::RunLoop().RunUntilIdle();

  HotwordDetection hotword_detection;
  hotword_detection.add_hotwords()->set_type(HotwordType::OK_GOOGLE);
  output_manager.HandleHotwordDetection(
      Serialized<HotwordDetection>(hotword_detection).GetBytes());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotword_detection_handler_impl.hotword_detection_.hotwords(0).type(),
      HotwordType::OK_GOOGLE);
}

TEST_F(OutputManagerTest, PresencePerceptionOutputManagerTest) {
  PerceptionInterfaces perception_interfaces;
  PerceptionInterface* interface = perception_interfaces.add_interface();
  interface->set_interface_type(
      PerceptionInterfaceType::INTERFACE_PRESENCE_PERCEPTION);
  PipelineOutput* output = interface->add_output();
  output->set_output_type(
      PipelineOutputType::OUTPUT_PRESENCE_PERCEPTION);
  output->set_stream_name("fake_stream_name");

  chromeos::media_perception::mojom::PerceptionInterfacesPtr interfaces_ptr =
      chromeos::media_perception::mojom::PerceptionInterfaces::New();
  OutputManager output_manager(
      "fake_presence_perception_configuration",
      rtanalytics_,
      perception_interfaces,
      &interfaces_ptr);
  // Verify that the mojo interface was created correctly.
  EXPECT_TRUE(interfaces_ptr->presence_perception_handler_request.is_pending());
  EXPECT_EQ(fake_rtanalytics_->GetMostRecentOutputStreamName(),
            "fake_stream_name");

  PresencePerceptionHandlerImpl presence_perception_handler_impl(
      std::move(interfaces_ptr->presence_perception_handler_request));
  base::RunLoop().RunUntilIdle();

  PresencePerception presence_perception;
  presence_perception.set_timestamp_us(1);
  output_manager.HandlePresencePerception(
      Serialized<PresencePerception>(presence_perception).GetBytes());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      presence_perception_handler_impl.presence_perception_.timestamp_us(), 1);
}

}  // namespace
}  // namespace mri
