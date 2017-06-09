/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ARC_EXIF_UTILS_H_
#define INCLUDE_ARC_EXIF_UTILS_H_

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libexif/exif-data.h>
}

namespace arc {

// ExifUtils can generate APP1 segment with tags which caller set. ExifUtils can
// also add a thumbnail in the APP1 segment if thumbnail size is specified.
// ExifUtils can be reused with different images by calling initialize().
//
// Example of using this class :
//  ExifUtils utils;
//  utils.initialize();
//  ...
//  // Call ExifUtils functions to set Exif tags.
//  ...
//  utils.GenerateApp1(thumbnail_buffer, thumbnail_size);
//  unsigned int app1Length = utils.GetApp1Length();
//  uint8_t* app1Buffer = new uint8_t[app1Length];
//  memcpy(app1Buffer, utils.GetApp1Buffer(), app1Length);
class ExifUtils {
 public:
  ExifUtils();
  ~ExifUtils();

  // Initialize() can be called multiple times. The setting of Exif tags will be
  // cleared.
  bool Initialize();

  // Sets the width (number of columes) of main image.
  // Returns false if memory allocation fails.
  bool SetImageWidth(uint16_t width);

  // Sets the length (number of rows) of main image.
  // Returns false if memory allocation fails.
  bool SetImageLength(uint16_t length);

  // Sets the manufacturer of camera.
  // Returns false if memory allocation fails.
  bool SetMaker(const std::string& maker);

  // Sets the model number of camera.
  // Returns false if memory allocation fails.
  bool SetModel(const std::string& model);

  // Sets the date and time of image last modified. It takes local time. The
  // name of the tag is DateTime in IFD0.
  // Returns false if memory allocation fails.
  bool SetDateTime(const struct tm& t);

  // Sets the focal length of lens used to take the image in millimeters.
  // Returns false if memory allocation fails.
  bool SetFocalLength(uint32_t numerator, uint32_t denominator);

  // Sets the latitude with degrees minutes seconds format.
  // Returns false if memory allocation fails.
  bool SetGpsLatitude(double latitude);

  // Sets the longitude with degrees minutes seconds format.
  // Returns false if memory allocation fails.
  bool SetGpsLongitude(double longitude);

  // Sets the altitude in meters.
  // Returns false if memory allocation fails.
  bool SetGpsAltitude(double altitude);

  // Sets GPS date stamp and time stamp (atomic clock). It takes UTC time.
  // Returns false if memory allocation fails.
  bool SetGpsTimestamp(const struct tm& t);

  // Sets GPS processing method.
  // Returns false if memory allocation fails.
  bool SetGpsProcessingMethod(const std::string& method);

  // Sets image orientation.
  // Returns false if memory allocation fails.
  bool SetOrientation(uint16_t orientation);

  // Generates APP1 segment.
  // Returns false if generating APP1 segment fails.
  bool GenerateApp1(const void* thumbnail_buffer, uint32_t size);

  // Gets buffer of APP1 segment. This method must be called only after calling
  // generateAPP1().
  const uint8_t* GetApp1Buffer();

  // Gets length of APP1 segment. This method must be called only after calling
  // generateAPP1().
  unsigned int GetApp1Length();

 private:
  // Resets the pointers and memories.
  void Reset();

  // Adds a variable length tag to |exif_data_|. It will remove the original one
  // if the tag exists.
  // Returns the entry of the tag. The reference count of returned ExifEntry is
  // two.
  std::unique_ptr<ExifEntry> AddVariableLengthEntry(ExifIfd ifd,
                                                    ExifTag tag,
                                                    ExifFormat format,
                                                    uint64_t components,
                                                    unsigned int size);

  // Adds a entry of |tag| in |exif_data_|. It won't remove the original one if
  // the tag exists.
  // Returns the entry of the tag. It adds one reference count to returned
  // ExifEntry.
  std::unique_ptr<ExifEntry> AddEntry(ExifIfd ifd, ExifTag tag);

  // Destroys the buffer of APP1 segment if exists.
  void DestroyApp1();

  // The Exif data (APP1). Owned by this class.
  ExifData* exif_data_;
  // The raw data of APP1 segment. It's allocated by ExifMem in |exif_data_| but
  // owned by this class.
  uint8_t* app1_buffer_;
  // The length of |app1_buffer_|.
  unsigned int app1_length_;
};

}  // namespace arc

#endif  // INCLUDE_ARC_EXIF_UTILS_H_
