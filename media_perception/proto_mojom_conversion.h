// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_PROTO_MOJOM_CONVERSION_H_
#define MEDIA_PERCEPTION_PROTO_MOJOM_CONVERSION_H_

#include "media_perception/device_management.pb.h"
#include "mojom/device_management.mojom.h"

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

}  // namespace mojom
}  // namespace media_perception
}  // namespace chromeos

namespace mri {

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

}  // namespace mri

#endif  // MEDIA_PERCEPTION_PROTO_MOJOM_CONVERSION_H_
