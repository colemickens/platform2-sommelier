// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_test/camera3_still_capture_fixture.h"

#include <algorithm>
#include <cmath>

namespace camera3_test {

void Camera3StillCaptureFixture::SetUp() {
  ASSERT_EQ(0, cam_service_.Initialize(base::Bind(
                   &Camera3StillCaptureFixture::ProcessStillCaptureResult,
                   base::Unretained(this))))
      << "Failed to initialize camera service";
  for (const auto& it : cam_ids_) {
    jpeg_max_sizes_[it] = cam_service_.GetStaticInfo(it)->GetJpegMaxSize();
  }
}

void Camera3StillCaptureFixture::ProcessStillCaptureResult(
    int cam_id,
    uint32_t frame_number,
    CameraMetadataUniquePtr metadata,
    BufferHandleUniquePtr buffer) {
  VLOGF_ENTER();
  StillCaptureResult* result = &still_capture_results_[cam_id];
  result->result_metadatas.emplace_back(std::move(metadata));
  result->buffer_handles.emplace_back(std::move(buffer));

  time_t cur_time;
  time(&cur_time);
  result->result_date_time.push_back(cur_time);
  sem_post(&result->capture_result_sem);
}

int Camera3StillCaptureFixture::WaitStillCaptureResult(
    int cam_id,
    const struct timespec& timeout) {
  // Wait for capture result callback
  return sem_timedwait(&still_capture_results_[cam_id].capture_result_sem,
                       &timeout);
}

Camera3StillCaptureFixture::StillCaptureResult::StillCaptureResult() {
  sem_init(&capture_result_sem, 0, 0);
}

Camera3StillCaptureFixture::StillCaptureResult::~StillCaptureResult() {
  sem_destroy(&capture_result_sem);
}

static int32_t GetJpegInfo(uint8_t* data,
                           size_t size,
                           ResolutionInfo* resolution,
                           ExifData** exif_data) {
#define JPEG_APP1 (JPEG_APP0 + 1)
  const unsigned int kMaxMarkerLength = 0xffff;
  struct jpeg_decompress_struct jpeg_info;
  struct jpeg_error_mgr jpeg_error;
  jpeg_info.err = jpeg_std_error(&jpeg_error);
  jpeg_create_decompress(&jpeg_info);
  jpeg_mem_src(&jpeg_info, data, size);
  jpeg_save_markers(&jpeg_info, JPEG_APP1, kMaxMarkerLength);
  if (jpeg_read_header(&jpeg_info, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&jpeg_info);
    return -EINVAL;
  }
  if (resolution) {
    *resolution = ResolutionInfo(jpeg_info.image_width, jpeg_info.image_height);
  }
  if (exif_data) {
    for (auto marker = jpeg_info.marker_list; marker != nullptr;
         marker = marker->next) {
      if (marker->marker == JPEG_APP1) {
        *exif_data = exif_data_new_from_data(marker->data, marker->data_length);
        break;
      }
    }
  }
  jpeg_destroy_decompress(&jpeg_info);
  return 0;
}

Camera3StillCaptureFixture::JpegExifInfo::JpegExifInfo(
    const BufferHandleUniquePtr& buffer,
    size_t size)
    : buffer_handle(buffer),
      buffer_size(size),
      buffer_addr(nullptr),
      jpeg_resolution(ResolutionInfo(0, 0)),
      exif_data(nullptr) {
  // Get JPEG image address and size
  if (Camera3TestGralloc::GetInstance()->Lock(*buffer_handle, 0, 0, 0, size, 1,
                                              &buffer_addr) != 0 ||
      !buffer_addr) {
    ADD_FAILURE() << "Failed to map buffer";
  }
}

Camera3StillCaptureFixture::JpegExifInfo::~JpegExifInfo() {
  if (exif_data) {
    exif_data_unref(exif_data);
  }
  Camera3TestGralloc::GetInstance()->Unlock(*buffer_handle);
}

bool Camera3StillCaptureFixture::JpegExifInfo::Initialize() {
  if (!buffer_addr) {
    return false;
  }
  auto jpeg_blob = reinterpret_cast<camera3_jpeg_blob_t*>(
      static_cast<uint8_t*>(buffer_addr) + buffer_size -
      sizeof(camera3_jpeg_blob_t));
  if (static_cast<void*>(jpeg_blob) < buffer_addr ||
      jpeg_blob->jpeg_blob_id != CAMERA3_JPEG_BLOB_ID) {
    ADD_FAILURE() << "Invalid JPEG BLOB ID";
    return false;
  }
  if (GetJpegInfo(static_cast<uint8_t*>(buffer_addr), jpeg_blob->jpeg_size,
                  &jpeg_resolution, &exif_data) != 0) {
    ADD_FAILURE() << "No valid JPEG image found in the buffer";
    return false;
  }
  return true;
}

// Test parameters:
// - Camera ID
class Camera3SimpleStillCaptureTest
    : public Camera3StillCaptureFixture,
      public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3SimpleStillCaptureTest()
      : Camera3StillCaptureFixture(std::vector<int>(1, GetParam())),
        cam_id_(GetParam()) {}

 protected:
  int cam_id_;

  void TakePictureTest(uint32_t num_still_pictures);

  struct ExifTestData {
    ResolutionInfo thumbnail_resolution;
    int32_t orientation;
    uint8_t jpeg_quality;
    uint8_t thumbnail_quality;
  };

  void ValidateExifKeys(const ResolutionInfo& jpeg_resolution,
                        const ExifTestData& exif_test_data,
                        const BufferHandleUniquePtr& buffer,
                        size_t buffer_size,
                        const camera_metadata_t& metadata,
                        const time_t& date_time);
};

static int32_t GetExifTagInteger(const ExifData& exif_data,
                                 const ExifIfd& ifd,
                                 const ExifTag& tag,
                                 const ExifByteOrder& byte_order) {
  ExifEntry* entry = exif_data_get_entry((&exif_data), tag);
  if (!entry) {
    ADD_FAILURE() << "Failed to get EXIF tag "
                  << exif_tag_get_name_in_ifd(tag, ifd);
    return -EINVAL;
  }
  switch (entry->format) {
    case EXIF_FORMAT_SHORT:
      return exif_get_short(entry->data, byte_order);
    case EXIF_FORMAT_LONG:
      return exif_get_long(entry->data, byte_order);
    case EXIF_FORMAT_SLONG:
      return exif_get_slong(entry->data, byte_order);
    default:
      ADD_FAILURE() << "Invalid EXIF entry format " << entry->format;
      return -EINVAL;
  }
}

static const char* GetExifTagString(const ExifData& exif_data,
                                    const ExifIfd& ifd,
                                    const ExifTag& tag) {
  ExifEntry* entry = exif_data_get_entry((&exif_data), tag);
  if (!entry) {
    ADD_FAILURE() << "Failed to get EXIF tag "
                  << exif_tag_get_name_in_ifd(tag, ifd);
    return nullptr;
  }
  EXPECT_EQ(EXIF_FORMAT_ASCII, entry->format)
      << "Invalid EXIF entry string format " << entry->format;
  return reinterpret_cast<char*>(entry->data);
}

static float GetExifTagFloat(const ExifData& exif_data,
                             const ExifIfd& ifd,
                             const ExifTag& tag,
                             const ExifByteOrder& byte_order) {
  ExifEntry* entry = exif_data_get_entry((&exif_data), tag);
  if (!entry) {
    ADD_FAILURE() << "Failed to get EXIF tag "
                  << exif_tag_get_name_in_ifd(tag, ifd);
    return -EINVAL;
  }
  switch (entry->format) {
    case EXIF_FORMAT_RATIONAL: {
      ExifRational rational = exif_get_rational(entry->data, byte_order);
      return static_cast<float>(rational.numerator) /
             static_cast<float>(rational.denominator);
    }
    case EXIF_FORMAT_SRATIONAL: {
      ExifSRational srational = exif_get_srational(entry->data, byte_order);
      return static_cast<float>(srational.numerator) /
             static_cast<float>(srational.denominator);
    }
    default:
      ADD_FAILURE() << "Invalid EXIF entry format " << entry->format;
      return -EINVAL;
  }
}

static int64_t GetMetadataInteger(const camera_metadata_t& metadata,
                                  int32_t key) {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(&metadata, key, &entry)) {
    ADD_FAILURE() << "Cannot find the metadata "
                  << get_camera_metadata_tag_name(key);
    return -EINVAL;
  }
  switch (entry.type) {
    case TYPE_BYTE:
      return entry.data.u8[0];
    case TYPE_INT32:
      return entry.data.i32[0];
    case TYPE_INT64:
      return entry.data.i64[0];
    default:
      ADD_FAILURE() << "Unexpected metadata entry type " << entry.type;
      return -EINVAL;
  }
}

static float GetMetadataKeyValueFloat(const camera_metadata_t& metadata,
                                      int32_t key) {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(&metadata, key, &entry)) {
    ADD_FAILURE() << "Cannot find the metadata "
                  << get_camera_metadata_tag_name(key);
    return -EINVAL;
  }
  if (entry.type != TYPE_FLOAT) {
    ADD_FAILURE() << "Unexpected metadata entry type " << entry.type;
    return -EINVAL;
  }
  return entry.data.f[0];
}

static ResolutionInfo GetMetadataThumbnailSize(
    const camera_metadata_t& metadata) {
  const size_t kNumOfElementsInSizeEntry = 2;
  enum { SIZE_ENTRY_WIDTH_INDEX, SIZE_ENTRY_HEIGHT_INDEX };
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(&metadata, ANDROID_JPEG_THUMBNAIL_SIZE,
                                    &entry)) {
    ADD_FAILURE() << "Cannot find the metadata ANDROID_JPEG_THUMBNAIL_SIZE";
    return ResolutionInfo(-1, -1);
  }
  if (entry.count != kNumOfElementsInSizeEntry) {
    ADD_FAILURE()
        << "Invalid entry count of metadata ANDROID_JPEG_THUMBNAIL_SIZE ("
        << entry.count << ")";
    return ResolutionInfo(-1, -1);
  }
  return ResolutionInfo(entry.data.i32[SIZE_ENTRY_WIDTH_INDEX],
                        entry.data.i32[SIZE_ENTRY_HEIGHT_INDEX]);
}

enum {
  ORIENTATION_UNDEFINED,
  ORIENTATION_NORMAL,
  ORIENTATION_FLIP_HORIZONTAL,
  ORIENTATION_ROTATE_180,
  ORIENTATION_FLIP_VERTICAL,
  ORIENTATION_TRANSPOSE,
  ORIENTATION_ROTATE_90,
  ORIENTATION_TRANSVERSE,
  ORIENTATION_ROTATE_270,
};

static int32_t GetExifOrientationInDegree(int32_t exif_orientation) {
  switch (exif_orientation) {
    case ORIENTATION_NORMAL:
      return 0;
    case ORIENTATION_ROTATE_90:
      return 90;
    case ORIENTATION_ROTATE_180:
      return 180;
    case ORIENTATION_ROTATE_270:
      return 270;
    default:
      ADD_FAILURE() << "It is impossible to get non 0, 90, 180, 270 degress "
                       "exif info based on the request orientation range";
      return 0;
  }
}

void Camera3SimpleStillCaptureTest::ValidateExifKeys(
    const ResolutionInfo& jpeg_resolution,
    const ExifTestData& exif_test_data,
    const BufferHandleUniquePtr& buffer,
    size_t buffer_size,
    const camera_metadata_t& metadata,
    const time_t& date_time) {
  const int32_t kExifDateTimeStringLength = 19;
  const double kExifDateTimeErrorMarginSeconds = 60;
  const float kExifFocalLengthErrorMargin = 0.001f;
  const float kExifExposureTimeErrorMarginRation = 0.05f;
  const float kExifExposureTimeMinErrorMarginSeconds = 0.002f;
  const float kOneNanoSecond = 1e-9;
  const float kExifApertureErrorMargin = 0.001f;
  auto GetClosestValueInArray = [](std::vector<float> values, float target) {
    int min_idx = -1;
    float min_distance = std::numeric_limits<float>::max();
    for (size_t i = 0; i < values.size(); i++) {
      float distance = std::abs(values[i] - target);
      if (min_distance > distance) {
        min_distance = distance;
        min_idx = i;
      }
    }
    return (min_idx >= 0) ? values[min_idx]
                          : -std::numeric_limits<float>::max();
  };
  auto SwapWidthAndHeight = [](ResolutionInfo* resolution) {
    *resolution = ResolutionInfo(resolution->Height(), resolution->Width());
  };

  JpegExifInfo jpeg_exif_info(buffer, buffer_size);
  ASSERT_TRUE(jpeg_exif_info.Initialize());
  ExifByteOrder byte_order = exif_data_get_byte_order(jpeg_exif_info.exif_data);
  const ExifData& exif_data = *jpeg_exif_info.exif_data;
  int32_t exif_orientation = GetExifTagInteger(
      exif_data, EXIF_IFD_0, EXIF_TAG_ORIENTATION, byte_order);
  EXPECT_LE(ORIENTATION_UNDEFINED, exif_orientation)
      << "Invalid EXIF orientation value";
  EXPECT_GE(ORIENTATION_ROTATE_270, exif_orientation)
      << "Invalid EXIF orientation value";
  EXPECT_TRUE(exif_orientation == ORIENTATION_UNDEFINED ||
              exif_test_data.orientation ==
                  GetExifOrientationInDegree(exif_orientation))
      << "EXIF orientaiton should match requested orientation";

  if (exif_test_data.thumbnail_resolution == ResolutionInfo(0, 0)) {
    EXPECT_EQ(nullptr, exif_data.data)
        << "JPEG shouldn't have thumbnail when thumbnail size is (0, 0)";
  } else {
    EXPECT_NE(nullptr, exif_data.data) << "No thumbnail found in JPEG image";
    ResolutionInfo expected_thumbnail_resolution(
        exif_test_data.thumbnail_resolution);
    if (exif_test_data.orientation % 180 == 90 &&
        exif_orientation == ORIENTATION_UNDEFINED) {
      // Device physically rotated image+thumbnail data
      // Expect thumbnail size to be also rotated
      SwapWidthAndHeight(&expected_thumbnail_resolution);
    }
    EXPECT_EQ(expected_thumbnail_resolution, GetMetadataThumbnailSize(metadata))
        << "JPEG thumbnail size result and request should match";
    if (exif_data.data) {
      ResolutionInfo actual_thumbnail_resolution(0, 0);
      if (GetJpegInfo(exif_data.data, exif_data.size,
                      &actual_thumbnail_resolution, nullptr) != 0) {
        ADD_FAILURE() << "No valid thumbnail image found in the buffer";
      } else {
        EXPECT_EQ(exif_test_data.thumbnail_resolution,
                  actual_thumbnail_resolution)
            << "EXIF thumbnail image size should match requested size";
      }
    }
  }
  EXPECT_EQ(exif_test_data.orientation,
            GetMetadataInteger(metadata, ANDROID_JPEG_ORIENTATION))
      << "JPEG orientation result and request should match";
  EXPECT_EQ(exif_test_data.jpeg_quality,
            GetMetadataInteger(metadata, ANDROID_JPEG_QUALITY))
      << "JPEG quality result and request should match";
  EXPECT_EQ(exif_test_data.thumbnail_quality,
            GetMetadataInteger(metadata, ANDROID_JPEG_THUMBNAIL_QUALITY))
      << "JPEG thumbnail quality result and request should match";
  // TAG_IMAGE_WIDTH and TAG_IMAGE_LENGTH and TAG_ORIENTATION.
  // Orientation and exif width/height need to be tested carefully, two cases:
  // 1. Device rotates the image buffer physically, then exif width/height may
  // not match the requested still capture size, we need swap them to check.
  // 2. Device uses the exif tag to record the image orientation, it doesn't
  // rotate the jpeg image buffer itself. In this case, the exif width/height
  // should always match the requested still capture size, and the exif
  // orientation should always match the requested orientation.
  ResolutionInfo expected_jpeg_size(jpeg_resolution);
  if (exif_test_data.orientation % 180 == 90 &&
      exif_orientation == ORIENTATION_UNDEFINED) {
    // Device captured image doesn't respect the requested orientation, which
    // means it rotates the image buffer physically. Then we should swap the
    // exif width/height accordingly to compare.
    SwapWidthAndHeight(&expected_jpeg_size);
  }
  EXPECT_EQ(
      expected_jpeg_size,
      ResolutionInfo(GetExifTagInteger(exif_data, EXIF_IFD_0,
                                       EXIF_TAG_IMAGE_WIDTH, byte_order),
                     GetExifTagInteger(exif_data, EXIF_IFD_0,
                                       EXIF_TAG_IMAGE_LENGTH, byte_order)))
      << "EXIF JPEG size should match requested size";
  EXPECT_EQ(
      expected_jpeg_size,
      ResolutionInfo(GetExifTagInteger(exif_data, EXIF_IFD_EXIF,
                                       EXIF_TAG_PIXEL_X_DIMENSION, byte_order),
                     GetExifTagInteger(exif_data, EXIF_IFD_EXIF,
                                       EXIF_TAG_PIXEL_Y_DIMENSION, byte_order)))
      << "EXIF JPEG pixel dimension should match requested size";
  EXPECT_EQ(jpeg_resolution, jpeg_exif_info.jpeg_resolution)
      << "JPEG size result and request should match";

  // Validate date time between EXIF data and current time
  const char* exif_datetime_string =
      GetExifTagString(exif_data, EXIF_IFD_0, EXIF_TAG_DATE_TIME);
  if (exif_datetime_string) {
    EXPECT_EQ(kExifDateTimeStringLength, strlen(exif_datetime_string))
        << "EXIF dateTime is in wrong format";
    struct tm exif_tm = {};
    strptime(exif_datetime_string, "%Y:%m:%d %H:%M:%S", &exif_tm);
    time_t exif_time = mktime(&exif_tm);
    EXPECT_GT(kExifDateTimeErrorMarginSeconds, difftime(date_time, exif_time))
        << "Capture time deviates too much from the current time";
    const char* exif_datetime_digitized = GetExifTagString(
        exif_data, EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_DIGITIZED);
    if (exif_datetime_digitized) {
      EXPECT_EQ(kExifDateTimeStringLength, strlen(exif_datetime_digitized))
          << "EXIF digitizedTime is in wrong format";
      EXPECT_EQ(0, strncmp(exif_datetime_string, exif_datetime_digitized,
                           kExifDateTimeStringLength))
          << "EXIF dataTime should match digitizedTime";
    }
  }

  // Validate focal length between EXIF data and metadata
  std::vector<float> focal_lengths;
  if (cam_service_.GetStaticInfo(cam_id_)->GetAvailableFocalLengths(
          &focal_lengths) != 0 ||
      focal_lengths.empty()) {
    ADD_FAILURE() << "Failed to get available focal lengths";
  } else {
    float exif_focal_length = GetExifTagFloat(
        exif_data, EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH, byte_order);
    EXPECT_GT(
        kExifFocalLengthErrorMargin,
        std::abs(GetClosestValueInArray(focal_lengths, exif_focal_length) -
                 exif_focal_length))
        << "EXIF focal length should be one of the available focal lengths";
    float result_focal_length =
        GetMetadataKeyValueFloat(metadata, ANDROID_LENS_FOCAL_LENGTH);
    EXPECT_LT(0.0f, result_focal_length)
        << "Result Focal length " << result_focal_length
        << " should be positive";
    EXPECT_NE(focal_lengths.end(),
              std::find(focal_lengths.begin(), focal_lengths.end(),
                        result_focal_length))
        << "Result Focal length should be one of the available focal lengths";
    EXPECT_GT(kExifFocalLengthErrorMargin,
              std::abs(result_focal_length - exif_focal_length))
        << "EXIF focal length should match capture result";
  }

  // Validate exposure time between EXIF data and metadata
  float exif_exposure_time = GetExifTagFloat(
      exif_data, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_TIME, byte_order);
  if (exif_exposure_time > 0 &&
      cam_service_.GetStaticInfo(cam_id_)->IsKeyAvailable(
          ANDROID_SENSOR_EXPOSURE_TIME)) {
    float result_exposure_time = static_cast<float>(GetMetadataInteger(
                                     metadata, ANDROID_SENSOR_EXPOSURE_TIME)) *
                                 kOneNanoSecond;
    float tolerance =
        std::max(result_exposure_time * kExifExposureTimeErrorMarginRation,
                 kExifExposureTimeMinErrorMarginSeconds);
    EXPECT_GT(tolerance, std::abs(result_exposure_time - exif_exposure_time))
        << "Exif exposure time doesn't match";
  }

  // Validate aperture between EXIF data and metadata
  float exif_aperture =
      GetExifTagFloat(exif_data, EXIF_IFD_EXIF, EXIF_TAG_FNUMBER, byte_order);
  if (exif_aperture > 0 && cam_service_.GetStaticInfo(cam_id_)->IsKeyAvailable(
                               ANDROID_LENS_INFO_AVAILABLE_APERTURES)) {
    std::vector<float> apertures;
    if (cam_service_.GetStaticInfo(cam_id_)->GetAvailableApertures(
            &apertures) != 0 ||
        apertures.empty()) {
      ADD_FAILURE() << "Failed to get available apertures";
    } else {
      float result_aperture =
          GetMetadataKeyValueFloat(metadata, ANDROID_LENS_APERTURE);
      EXPECT_GT(kExifApertureErrorMargin,
                std::abs(GetClosestValueInArray(apertures, exif_aperture) -
                         exif_aperture))
          << "Aperture value should be one of the available apertures";
      EXPECT_GT(kExifApertureErrorMargin,
                std::abs(result_aperture - exif_aperture))
          << "Exif aperture length should match capture result";
    }
  }

  // Verify EXIF TAG_FLASH is available
  GetExifTagInteger(exif_data, EXIF_IFD_EXIF, EXIF_TAG_FLASH, byte_order);
  // Verify EXIF TAG_WHITE_BALANCE is available
  GetExifTagInteger(exif_data, EXIF_IFD_EXIF, EXIF_TAG_WHITE_BALANCE,
                    byte_order);
  // TODO(hywu): For full devices, validate flash and AWB between EXIF and
  // metadata

  // Validate ISO between EXIF and metadata
  if (cam_service_.GetStaticInfo(cam_id_)->IsKeyAvailable(
          ANDROID_SENSOR_SENSITIVITY)) {
    int32_t iso = GetExifTagInteger(exif_data, EXIF_IFD_EXIF,
                                    EXIF_TAG_ISO_SPEED_RATINGS, byte_order);
    EXPECT_EQ(GetMetadataInteger(metadata, ANDROID_SENSOR_SENSITIVITY), iso)
        << "EXIF TAG_ISO is incorrect";
  }

  auto is_number = [&](const ExifIfd& ifd, const ExifTag& tag) {
    const char* c = GetExifTagString(exif_data, ifd, tag);
    const std::string s(c ? c : "");
    return !s.empty() && std::find_if(s.begin(), s.end(), [](char c) {
                           return !std::isdigit(c);
                         }) == s.end();
  };
  EXPECT_TRUE(is_number(EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME));
  EXPECT_TRUE(is_number(EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME_ORIGINAL));
  EXPECT_TRUE(is_number(EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME_DIGITIZED));
}

TEST_P(Camera3SimpleStillCaptureTest, JpegExifTest) {
  // Reference:
  // camera2/cts/StillCaptureTest.java.java#testJpegExif
  ResolutionInfo jpeg_resolution =
      cam_service_.GetStaticInfo(cam_id_)
          ->GetSortedOutputResolutions(HAL_PIXEL_FORMAT_BLOB)
          .back();
  std::vector<ResolutionInfo> thumbnail_resolutions;
  EXPECT_TRUE(cam_service_.GetStaticInfo(cam_id_)->GetAvailableThumbnailSizes(
                  &thumbnail_resolutions) == 0 &&
              !thumbnail_resolutions.empty())
      << "JPEG thumbnail sizes are not available";
  // Size must contain (0, 0).
  EXPECT_TRUE(std::find(thumbnail_resolutions.begin(),
                        thumbnail_resolutions.end(),
                        ResolutionInfo(0, 0)) != thumbnail_resolutions.end())
      << "JPEG thumbnail sizes should contain (0, 0)";
  // Each size must be distinct.
  EXPECT_EQ(thumbnail_resolutions.size(),
            std::set<ResolutionInfo>(thumbnail_resolutions.begin(),
                                     thumbnail_resolutions.end())
                .size())
      << "JPEG thumbnail sizes contain duplicate items";
  // Must be sorted in ascending order by area, by width if areas are same.
  EXPECT_TRUE(std::is_sorted(thumbnail_resolutions.begin(),
                             thumbnail_resolutions.end()))
      << "JPEG thumbnail sizes are not in ascending order";

  ResolutionInfo preview_resolution =
      cam_service_.GetStaticInfo(cam_id_)
          ->GetSortedOutputResolutions(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
          .back();
  cam_service_.PrepareStillCaptureAndStartPreview(cam_id_, jpeg_resolution,
                                                  preview_resolution);

  ExifTestData exif_test_data[] = {
      {thumbnail_resolutions.front(), 90, 80, 75},
      {thumbnail_resolutions.front(), 180, 90, 85},
      {thumbnail_resolutions.back(), 270, 100, 100},
  };

  CameraMetadataUniquePtr metadata(
      clone_camera_metadata(cam_service_.ConstructDefaultRequestSettings(
          cam_id_, CAMERA3_TEMPLATE_STILL_CAPTURE)));
  ASSERT_NE(nullptr, metadata.get())
      << "Failed to create still capture metadata";
  for (const auto& it : exif_test_data) {
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(*(A)))
    int32_t thumbnail_resolution[] = {it.thumbnail_resolution.Width(),
                                      it.thumbnail_resolution.Height()};
    EXPECT_EQ(0,
              UpdateMetadata(ANDROID_JPEG_THUMBNAIL_SIZE, thumbnail_resolution,
                             ARRAY_SIZE(thumbnail_resolution), &metadata));
    EXPECT_EQ(0, UpdateMetadata(ANDROID_JPEG_ORIENTATION, &it.orientation, 1,
                                &metadata));
    EXPECT_EQ(0, UpdateMetadata(ANDROID_JPEG_QUALITY, &it.jpeg_quality, 1,
                                &metadata));
    EXPECT_EQ(0, UpdateMetadata(ANDROID_JPEG_THUMBNAIL_QUALITY,
                                &it.thumbnail_quality, 1, &metadata));
    cam_service_.TakeStillCapture(cam_id_, metadata.get());
  }

  struct timespec timeout;
  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += ARRAY_SIZE(exif_test_data);  // 1 second per capture
  for (size_t i = 0; i < arraysize(exif_test_data); i++) {
    ASSERT_EQ(0, WaitStillCaptureResult(cam_id_, timeout))
        << "Waiting for still capture result timeout";
  }

  for (size_t i = 0;
       i < still_capture_results_[cam_id_].result_metadatas.size(); i++) {
    ValidateExifKeys(jpeg_resolution, exif_test_data[i],
                     still_capture_results_[cam_id_].buffer_handles[i],
                     jpeg_max_sizes_[cam_id_],
                     *still_capture_results_[cam_id_].result_metadatas[i].get(),
                     still_capture_results_[cam_id_].result_date_time[i]);
  }
  cam_service_.StopPreview(cam_id_);
}

void Camera3SimpleStillCaptureTest::TakePictureTest(
    uint32_t num_still_pictures) {
  auto IsAFSupported = [this]() {
    std::vector<int32_t> available_af_modes;
    cam_service_.GetStaticInfo(cam_id_)->GetAvailableAFModes(
        &available_af_modes);
    int32_t af_modes[] = {ANDROID_CONTROL_AF_MODE_AUTO,
                          ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
                          ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO,
                          ANDROID_CONTROL_AF_MODE_MACRO};
    for (const auto& it : af_modes) {
      if (std::find(available_af_modes.begin(), available_af_modes.end(), it) !=
          available_af_modes.end()) {
        return true;
      }
    }
    return false;
  };

  ResolutionInfo jpeg_resolution =
      cam_service_.GetStaticInfo(cam_id_)
          ->GetSortedOutputResolutions(HAL_PIXEL_FORMAT_BLOB)
          .back();
  ResolutionInfo preview_resolution =
      cam_service_.GetStaticInfo(cam_id_)
          ->GetSortedOutputResolutions(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
          .back();
  cam_service_.PrepareStillCaptureAndStartPreview(cam_id_, jpeg_resolution,
                                                  preview_resolution);

  // Trigger an auto focus run, and wait for AF locked.
  if (IsAFSupported()) {
    cam_service_.StartAutoFocus(cam_id_);
    ASSERT_EQ(0, cam_service_.WaitForAutoFocusDone(cam_id_))
        << "Wait for auto focus done timed out";
  }
  // Wait for AWB converged, then lock it.
  ASSERT_EQ(0, cam_service_.WaitForAWBConvergedAndLock(cam_id_))
      << "Wait for AWB converged timed out";
  // Trigger an AE precapture metering sequence and wait for AE converged.
  cam_service_.StartAEPrecapture(cam_id_);
  ASSERT_EQ(0, cam_service_.WaitForAEStable(cam_id_))
      << "Wait for AE stable timed out";

  const camera_metadata_t* metadata =
      cam_service_.ConstructDefaultRequestSettings(
          cam_id_, CAMERA3_TEMPLATE_STILL_CAPTURE);
  for (uint32_t i = 0; i < num_still_pictures; i++) {
    cam_service_.TakeStillCapture(cam_id_, metadata);
  }

  cam_service_.StopPreview(cam_id_);
}

TEST_P(Camera3SimpleStillCaptureTest, TakePictureTest) {
  TakePictureTest(1);
}

TEST_P(Camera3SimpleStillCaptureTest, PerformanceTest) {
  TakePictureTest(2);
}

// Test parameters:
// - Camera ID, preview resolution, JPEG resolution
class Camera3JpegResolutionTest
    : public Camera3StillCaptureFixture,
      public ::testing::WithParamInterface<
          std::tuple<int32_t, ResolutionInfo, ResolutionInfo>> {
 public:
  Camera3JpegResolutionTest()
      : Camera3StillCaptureFixture(
            std::vector<int>(1, std::get<0>(GetParam()))),
        cam_id_(std::get<0>(GetParam())) {}

 protected:
  int cam_id_;
};

TEST_P(Camera3JpegResolutionTest, JpegResolutionTest) {
  ResolutionInfo preview_resolution(std::get<1>(GetParam()));
  ResolutionInfo jpeg_resolution(std::get<2>(GetParam()));
  VLOGF(1) << "Device " << cam_id_;
  VLOGF(1) << "Preview resolution " << preview_resolution.Width() << "x"
           << preview_resolution.Height();
  VLOGF(1) << "JPEG resolution " << jpeg_resolution.Width() << "x"
           << jpeg_resolution.Height();

  cam_service_.PrepareStillCaptureAndStartPreview(cam_id_, jpeg_resolution,
                                                  preview_resolution);
  CameraMetadataUniquePtr metadata(
      clone_camera_metadata(cam_service_.ConstructDefaultRequestSettings(
          cam_id_, CAMERA3_TEMPLATE_STILL_CAPTURE)));
  ASSERT_NE(nullptr, metadata.get())
      << "Failed to create still capture metadata";
  cam_service_.TakeStillCapture(cam_id_, metadata.get());
  cam_service_.StopPreview(cam_id_);

  ASSERT_EQ(1, still_capture_results_[cam_id_].buffer_handles.size())
      << "Incorrect number of still captures received";
  JpegExifInfo jpeg_exif_info(
      still_capture_results_[cam_id_].buffer_handles[0],
      jpeg_max_sizes_[cam_id_]);
  ASSERT_TRUE(jpeg_exif_info.Initialize());
  EXPECT_EQ(jpeg_resolution, jpeg_exif_info.jpeg_resolution)
      << "JPEG size result and request should match";
}

INSTANTIATE_TEST_CASE_P(Camera3StillCaptureTest,
                        Camera3SimpleStillCaptureTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

static std::vector<std::tuple<int, ResolutionInfo, ResolutionInfo>>
IterateCameraIdPreviewJpegResolution() {
  std::vector<std::tuple<int, ResolutionInfo, ResolutionInfo>> result;
  auto cam_ids = Camera3Module().GetCameraIds();
  for (const auto& cam_id : cam_ids) {
    auto preview_resolutions = Camera3Module().GetSortedOutputResolutions(
        cam_id, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
    auto jpeg_resolutions = Camera3Module().GetSortedOutputResolutions(
        cam_id, HAL_PIXEL_FORMAT_BLOB);
    for (const auto& preview_resolution : preview_resolutions) {
      for (const auto& jpeg_resolution : jpeg_resolutions) {
        result.emplace_back(cam_id, preview_resolution, jpeg_resolution);
      }
    }
  }
  return result;
}

INSTANTIATE_TEST_CASE_P(
    Camera3StillCaptureTest,
    Camera3JpegResolutionTest,
    ::testing::ValuesIn(IterateCameraIdPreviewJpegResolution()));

}  // namespace camera3_test
