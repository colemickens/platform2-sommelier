// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_PROTO_MOJOM_CONVERSION_H_
#define MEDIA_PERCEPTION_PROTO_MOJOM_CONVERSION_H_

#include <vector>

#include "media_perception/common.pb.h"
#include "media_perception/device_management.pb.h"
#include "media_perception/pipeline.pb.h"
#include "mojom/common.mojom.h"
#include "mojom/device_management.mojom.h"
#include "mojom/media_perception.mojom.h"
#include "mojom/pipeline.mojom.h"

namespace chromeos {
namespace media_perception {
namespace mojom {

PixelFormat ToMojom(mri::PixelFormat format);
VideoStreamParamsPtr ToMojom(const mri::VideoStreamParams& params);
VideoDevicePtr ToMojom(const mri::VideoDevice& device);
VirtualVideoDevicePtr ToMojom(const mri::VirtualVideoDevice& device);
AudioStreamParamsPtr ToMojom(const mri::AudioStreamParams& params);
AudioDevicePtr ToMojom(const mri::AudioDevice& device);
DeviceType ToMojom(mri::DeviceType type);
DeviceTemplatePtr ToMojom(const mri::DeviceTemplate& device_template);

// Common conversions.
DistanceUnits ToMojom(mri::DistanceUnits units);
NormalizedBoundingBoxPtr ToMojom(const mri::NormalizedBoundingBox& bbox);
DistancePtr ToMojom(const mri::Distance& distance);

// Pipeline conversions.
PipelineStatus ToMojom(mri::PipelineStatus status);
PipelineErrorType ToMojom(mri::PipelineErrorType error_type);
PipelineErrorPtr ToMojom(const mri::PipelineError& error);
PipelineStatePtr ToMojom(const mri::PipelineState& state);


}  // namespace mojom
}  // namespace media_perception
}  // namespace chromeos

namespace mri {

std::vector<uint8_t> SerializeVideoDeviceProto(const VideoDevice& device);

PixelFormat ToProto(
    chromeos::media_perception::mojom::PixelFormat format);
VideoStreamParams ToProto(
    const chromeos::media_perception::mojom::VideoStreamParamsPtr& params_ptr);
VideoDevice ToProto(
    const chromeos::media_perception::mojom::VideoDevicePtr& device_ptr);
VirtualVideoDevice ToProto(
    const chromeos::media_perception::mojom::VirtualVideoDevicePtr& device_ptr);
AudioStreamParams ToProto(
    const chromeos::media_perception::mojom::AudioStreamParamsPtr& params_ptr);
AudioDevice ToProto(
    const chromeos::media_perception::mojom::AudioDevicePtr& device_ptr);
DeviceType ToProto(
    chromeos::media_perception::mojom::DeviceType type);
DeviceTemplate ToProto(
    const chromeos::media_perception::mojom::DeviceTemplatePtr& template_ptr);

// Common conversions.
DistanceUnits ToProto(chromeos::media_perception::mojom::DistanceUnits units);
NormalizedBoundingBox ToProto(
    const chromeos::media_perception::mojom::NormalizedBoundingBoxPtr&
    bbox_ptr);
Distance ToProto(
    const chromeos::media_perception::mojom::DistancePtr& distance_ptr);

// Pipeline conversions.
PipelineStatus ToProto(
    chromeos::media_perception::mojom::PipelineStatus status);
PipelineErrorType ToProto(
    chromeos::media_perception::mojom::PipelineErrorType error_type);
PipelineError ToProto(
    const chromeos::media_perception::mojom::PipelineErrorPtr& error_ptr);
PipelineState ToProto(
    const chromeos::media_perception::mojom::PipelineStatePtr& state_ptr);

}  // namespace mri

#endif  // MEDIA_PERCEPTION_PROTO_MOJOM_CONVERSION_H_
