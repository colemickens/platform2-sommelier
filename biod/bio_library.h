// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_BIO_LIBRARY_H_
#define BIOD_BIO_LIBRARY_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>

#include "biod/bio_algorithm.h"

namespace biod {

enum class BioAlgorithmType {
  kUnknown = -1,
  kFingerprint = BIO_ALGORITHM_FINGERPRINT,
  kIris = BIO_ALGORITHM_IRIS,
};

class BioLibrary;

class BioImage {
 public:
  BioImage() = default;
  BioImage(BioImage&& rhs);
  BioImage(const std::shared_ptr<BioLibrary>& lib, bio_image_t image)
      : lib_(lib), image_(image) {}
  ~BioImage();

  BioImage& operator=(BioImage&& rhs);

  explicit operator bool() const { return lib_ && image_; }

  bio_image_t get() const { return image_; }

  bool SetData(std::vector<uint8_t>* data);
  bool Destroy();

 private:
  std::shared_ptr<BioLibrary> lib_;
  std::vector<uint8_t> data_;
  bio_image_t image_ = nullptr;
};

class BioTemplate {
 public:
  BioTemplate() = default;
  BioTemplate(BioTemplate&& rhs);
  BioTemplate(const std::shared_ptr<BioLibrary>& lib, bio_template_t tmplate)
      : lib_(lib), template_(tmplate) {}
  ~BioTemplate();

  BioTemplate& operator=(BioTemplate&& rhs);

  explicit operator bool() const { return lib_ && template_; }

  int MatchImage(const BioImage& image);
  bool Serialize(std::vector<uint8_t>* out);

  bool Destroy();

 private:
  std::shared_ptr<BioLibrary> lib_;
  bio_template_t template_ = nullptr;
};

class BioEnrollment {
 public:
  BioEnrollment() = default;
  BioEnrollment(BioEnrollment&& rhs);
  BioEnrollment(const std::shared_ptr<BioLibrary>& lib,
                bio_enrollment_t enrollment)
      : lib_(lib), enrollment_(enrollment) {}
  ~BioEnrollment();

  BioEnrollment& operator=(BioEnrollment&& rhs);

  explicit operator bool() const { return lib_ && enrollment_; }

  int AddImage(const BioImage& image);
  int IsComplete();
  int GetPercentComplete();
  BioTemplate Finish();
  bool Destroy();

 private:
  std::shared_ptr<BioLibrary> lib_;
  bio_enrollment_t enrollment_ = nullptr;
};

class BioSensor {
 public:
  struct Model {
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t model_id;
    uint32_t version;
  };

  BioSensor() = default;
  BioSensor(BioSensor&& rhs);
  BioSensor(const std::shared_ptr<BioLibrary>& lib, bio_sensor_t sensor)
      : lib_(lib), sensor_(sensor) {}
  ~BioSensor();

  BioSensor& operator=(BioSensor&& rhs);

  explicit operator bool() const { return lib_ && sensor_; }

  bool SetModel(const Model& model);
  bool SetFormat(uint32_t pixel_format);
  bool SetSize(uint32_t width, uint32_t height);
  // Must have called SetSize prior to this call.
  BioImage CreateImage();
  BioEnrollment BeginEnrollment();
  bool Destroy();

 private:
  std::shared_ptr<BioLibrary> lib_;
  bio_sensor_t sensor_ = nullptr;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

class BioLibrary : public std::enable_shared_from_this<BioLibrary> {
 public:
  ~BioLibrary();

  // Loads the bio algorithm implementation from the shared object at path. On
  // success, a valid BioLibrary pointer is returned.
  static std::shared_ptr<BioLibrary> Load(const base::FilePath& path);

  // Gets a single function named sym from the loaded bio library. Returns a
  // NULL pointer if the symbol can not be loaded. This is useful for accessing
  // non-standard bio library functions.
  template <typename T>
  T GetFunction(const std::string& sym) const {
    void* sym_ptr = GetSymbolPointer(sym);
    if (sym_ptr == nullptr)
      return nullptr;
    return reinterpret_cast<T>(sym_ptr);
  }

  BioAlgorithmType GetAlgorithmType();
  base::StringPiece GetAlgorithmName();
  base::StringPiece GetAlgorithmVersion();
  base::StringPiece GetAlgorithmBanner();
  BioSensor CreateSensor();
  BioTemplate DeserializeTemplate(const std::vector<uint8_t>& data);

 private:
  friend class BioImage;
  friend class BioTemplate;
  friend class BioEnrollment;
  friend class BioSensor;

  using bio_algorithm_init_fp = int (*)(void);
  using bio_algorithm_exit_fp = int (*)(void);
  using bio_algorithm_get_type_fp = enum bio_algoritm_type (*)(void);
  using bio_algorithm_get_name_fp = const char* (*)(void);
  using bio_algorithm_get_version_fp = const char* (*)(void);
  using bio_algorithm_get_banner_fp = const char* (*)(void);
  using bio_sensor_create_fp = int (*)(bio_sensor_t* sensor);
  using bio_sensor_destroy_fp = int (*)(bio_sensor_t sensor);
  using bio_sensor_set_model_fp = int (*)(bio_sensor_t sensor,
                                          uint32_t vendor_id,
                                          uint32_t product_id,
                                          uint32_t model_id,
                                          uint32_t version);
  using bio_sensor_set_format_fp = int (*)(bio_sensor_t sensor,
                                           uint32_t pixel_format);
  using bio_sensor_set_size_fp = int (*)(bio_sensor_t sensor,
                                         uint32_t width,
                                         uint32_t height);
  using bio_image_create_fp = int (*)(bio_sensor_t sensor,
                                      uint32_t width,
                                      uint32_t height,
                                      bio_image_t* image);
  using bio_image_set_size_fp = int (*)(bio_image_t image,
                                        uint32_t width,
                                        uint32_t height);
  using bio_image_set_data_fp = int (*)(bio_image_t image,
                                        const uint8_t* data,
                                        size_t size);
  using bio_image_destroy_fp = int (*)(bio_image_t image);
  using bio_template_image_match_fp = int (*)(bio_template_t tmpl,
                                              bio_image_t image);
  using bio_template_deserialize_fp = int (*)(const uint8_t* template_data,
                                              size_t size,
                                              bio_template_t* tmpl);
  using bio_template_get_serialized_size_fp = ssize_t (*)(bio_template_t tmpl);
  using bio_template_serialize_fp = int (*)(bio_template_t tmpl,
                                            uint8_t* template_data,
                                            size_t size);
  using bio_template_destroy_fp = int (*)(bio_template_t tmpl);
  using bio_enrollment_begin_fp = int (*)(bio_sensor_t sensor,
                                          bio_enrollment_t* enrollment);
  using bio_enrollment_add_image_fp = int (*)(bio_enrollment_t enrollment,
                                              bio_image_t image);
  using bio_enrollment_is_complete_fp = int (*)(bio_enrollment_t enrollment);
  using bio_enrollment_get_percent_complete_fp =
      int (*)(bio_enrollment_t enrollment);
  using bio_enrollment_finish_fp = int (*)(bio_enrollment_t enrollment,
                                           bio_template_t* tmpl);

  BioLibrary() = default;
  bool Init(const base::FilePath& path);
  void* GetSymbolPointer(const std::string& sym) const;

  void* handle_ = nullptr;
  bool needs_exit_ = false;

  // See bio_algorithm.h for the documentation about these functions.
  bio_algorithm_init_fp algorithm_init_;
  bio_algorithm_exit_fp algorithm_exit_;
  bio_algorithm_get_type_fp algorithm_get_type_;
  bio_algorithm_get_name_fp algorithm_get_name_;
  bio_algorithm_get_version_fp algorithm_get_version_;
  bio_algorithm_get_banner_fp algorithm_get_banner_;
  bio_sensor_create_fp sensor_create_;
  bio_sensor_destroy_fp sensor_destroy_;
  bio_sensor_set_model_fp sensor_set_model_;
  bio_sensor_set_format_fp sensor_set_format_;
  bio_sensor_set_size_fp sensor_set_size_;
  bio_image_create_fp image_create_;
  bio_image_set_size_fp image_set_size_;
  bio_image_set_data_fp image_set_data_;
  bio_image_destroy_fp image_destroy_;
  bio_template_image_match_fp template_image_match_;
  bio_template_deserialize_fp template_deserialize_;
  bio_template_get_serialized_size_fp template_get_serialized_size_;
  bio_template_serialize_fp template_serialize_;
  bio_template_destroy_fp template_destroy_;
  bio_enrollment_begin_fp enrollment_begin_;
  bio_enrollment_add_image_fp enrollment_add_image_;
  bio_enrollment_is_complete_fp enrollment_is_complete_;
  bio_enrollment_get_percent_complete_fp enrollment_get_percent_complete_;
  bio_enrollment_finish_fp enrollment_finish_;

  DISALLOW_COPY_AND_ASSIGN(BioLibrary);
};

}  //  namespace biod

#endif  // BIOD_BIO_LIBRARY_H_
