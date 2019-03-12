// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/files/scoped_file.h>
#include <base/posix/eintr_wrapper.h>
#include <base/scoped_generic.h>
#include <base/strings/stringprintf.h>
#include <gbm.h>
#include <gtest/gtest.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace {

namespace internal {
struct ScopedDrmFDCloseTraits {
  static int InvalidValue() { return -1; }
  static void Free(int fd) { drmClose(fd); }
};
}  // namespace internal
using ScopedDrmFD = base::ScopedGeneric<int, internal::ScopedDrmFDCloseTraits>;

namespace internal {
struct DrmModeResourcesDeleter {
  void operator()(drmModeRes* ptr) { drmModeFreeResources(ptr); }
};
}  // namespace internal
using ScopedDrmModeResources =
    std::unique_ptr<drmModeRes, internal::DrmModeResourcesDeleter>;

namespace internal {
struct DrmModeConnectorDeleter {
  void operator()(drmModeConnector* ptr) { drmModeFreeConnector(ptr); }
};
}  // namespace internal
using ScopedDrmModeConnector =
    std::unique_ptr<drmModeConnector, internal::DrmModeConnectorDeleter>;

namespace internal {
struct GbmDeviceDeleter {
  void operator()(gbm_device* gbm) {
    if (gbm)
      gbm_device_destroy(gbm);
  }
};
}  // namespace internal
using ScopedGbmDevice =
    std::unique_ptr<gbm_device, internal::GbmDeviceDeleter>;

namespace internal {
struct GbmBoDeleter {
  void operator()(gbm_bo* bo) {
    if (bo)
      gbm_bo_destroy(bo);
  }
};
}  // namespace internal
using ScopedGbmBo = std::unique_ptr<gbm_bo, internal::GbmBoDeleter>;

constexpr uint32_t kFormatList[] = {
  GBM_FORMAT_C8,
  GBM_FORMAT_RGB332,
  GBM_FORMAT_BGR233,
  GBM_FORMAT_XRGB4444,
  GBM_FORMAT_XBGR4444,
  GBM_FORMAT_RGBX4444,
  GBM_FORMAT_BGRX4444,
  GBM_FORMAT_ARGB4444,
  GBM_FORMAT_ABGR4444,
  GBM_FORMAT_RGBA4444,
  GBM_FORMAT_BGRA4444,
  GBM_FORMAT_XRGB1555,
  GBM_FORMAT_XBGR1555,
  GBM_FORMAT_RGBX5551,
  GBM_FORMAT_BGRX5551,
  GBM_FORMAT_ARGB1555,
  GBM_FORMAT_ABGR1555,
  GBM_FORMAT_RGBA5551,
  GBM_FORMAT_BGRA5551,
  GBM_FORMAT_RGB565,
  GBM_FORMAT_BGR565,
  GBM_FORMAT_RGB888,
  GBM_FORMAT_BGR888,
  GBM_FORMAT_XRGB8888,
  GBM_FORMAT_XBGR8888,
  GBM_FORMAT_RGBX8888,
  GBM_FORMAT_BGRX8888,
  GBM_FORMAT_ARGB8888,
  GBM_FORMAT_ABGR8888,
  GBM_FORMAT_RGBA8888,
  GBM_FORMAT_BGRA8888,
  GBM_FORMAT_XRGB2101010,
  GBM_FORMAT_XBGR2101010,
  GBM_FORMAT_RGBX1010102,
  GBM_FORMAT_BGRX1010102,
  GBM_FORMAT_ARGB2101010,
  GBM_FORMAT_ABGR2101010,
  GBM_FORMAT_RGBA1010102,
  GBM_FORMAT_BGRA1010102,
  GBM_FORMAT_YUYV,
  GBM_FORMAT_YVYU,
  GBM_FORMAT_UYVY,
  GBM_FORMAT_VYUY,
  GBM_FORMAT_AYUV,
  GBM_FORMAT_NV12,
  GBM_FORMAT_YVU420,
};

void ExpectBo(gbm_bo* bo) {
  ASSERT_TRUE(bo);
  EXPECT_GE(gbm_bo_get_width(bo), 0);
  EXPECT_GE(gbm_bo_get_height(bo), 0);
  EXPECT_GE(gbm_bo_get_stride(bo), gbm_bo_get_width(bo));

  const uint32_t format = gbm_bo_get_format(bo);

  // TODO(crbug.com/909719): Use base::ContainsValue, after uprev (which
  // supports an array).
  EXPECT_TRUE(
      std::find(std::begin(kFormatList), std::end(kFormatList), format) !=
      std::end(kFormatList)) << format;

  const size_t num_planes = gbm_bo_get_plane_count(bo);
  switch (format) {
    case GBM_FORMAT_NV12:
      EXPECT_EQ(2, num_planes);
      break;
    case GBM_FORMAT_YVU420:
      EXPECT_EQ(3, num_planes);
      break;
    default:
      EXPECT_EQ(1, num_planes);
      break;
  }

  EXPECT_EQ(gbm_bo_get_handle_for_plane(bo, 0).u32, gbm_bo_get_handle(bo).u32);

  EXPECT_EQ(0, gbm_bo_get_offset(bo, 0));
  EXPECT_GE(gbm_bo_get_plane_size(bo, 0),
            gbm_bo_get_width(bo) * gbm_bo_get_height(bo));
  EXPECT_EQ(gbm_bo_get_stride_for_plane(bo, 0), gbm_bo_get_stride(bo));

  for (size_t plane = 0; plane < num_planes; ++plane) {
    EXPECT_GT(gbm_bo_get_handle_for_plane(bo, plane).u32, 0);
    {
      base::ScopedFD fd(gbm_bo_get_plane_fd(bo, plane));
      EXPECT_TRUE(fd.is_valid());
    }
    gbm_bo_get_offset(bo, plane);  // Make sure no crash.
    EXPECT_GT(gbm_bo_get_plane_size(bo, plane), 0);
    EXPECT_GT(gbm_bo_get_stride_for_plane(bo, plane), 0);
  }
}

bool HasConnectedConnector(int fd, const drmModeRes* resources) {
  for (int i = 0; i < resources->count_connectors; ++i) {
    ScopedDrmModeConnector connector(
        drmModeGetConnector(fd, resources->connectors[i]));
    if (!connector)
      continue;
    if (connector->count_modes > 0 &&
        connector->connection == DRM_MODE_CONNECTED) {
      return true;
    }
  }
  return false;
}

ScopedDrmFD DrmOpen() {
  for (int i = 0; i < DRM_MAX_MINOR; ++i) {
    auto dev_name = base::StringPrintf(DRM_DEV_NAME, DRM_DIR_NAME, i);
    ScopedDrmFD fd(HANDLE_EINTR(open(dev_name.c_str(), O_RDWR)));
    if (!fd.is_valid())
      continue;

    ScopedDrmModeResources resources(drmModeGetResources(fd.get()));
    if (!resources)
      continue;

    if (resources->count_crtcs > 0 &&
        HasConnectedConnector(fd.get(), resources.get())) {
      return fd;
    }
  }

  return {};
}

class GraphicsGbmTest : public testing::Test {
 public:
  GraphicsGbmTest() : fd_(DrmOpen()), gbm_(gbm_create_device(fd_.get())) {
    EXPECT_TRUE(fd_.is_valid());
    EXPECT_TRUE(gbm_.get());
  }
  ~GraphicsGbmTest() = default;

 protected:
  ScopedDrmFD fd_;
  ScopedGbmDevice gbm_;
};

TEST_F(GraphicsGbmTest, BackendName) {
  EXPECT_TRUE(gbm_device_get_backend_name(gbm_.get()));
}

TEST_F(GraphicsGbmTest, Reinit) {
  gbm_.reset();
  fd_.reset();

  fd_ = DrmOpen();
  ASSERT_TRUE(fd_.is_valid());
  gbm_.reset(gbm_create_device(fd_.get()));
  ASSERT_TRUE(gbm_.get());

  EXPECT_TRUE(gbm_device_get_backend_name(gbm_.get()));

  ScopedGbmBo bo(gbm_bo_create(
      gbm_.get(), 1024, 1024, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING));
  ExpectBo(bo.get());
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
