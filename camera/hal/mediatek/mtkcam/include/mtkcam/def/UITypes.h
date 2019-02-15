/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_UITYPES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_UITYPES_H_

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *  Camera UI Types.
 ******************************************************************************/
struct MPoint;
struct MSize;
struct MRect;

/**
 *
 * @brief Camera point type.
 *
 */
struct MPoint {
  typedef int value_type;

  value_type x;
  value_type y;

#ifdef __cplusplus

 public:  ////                Instantiation.
  // we don't provide copy-ctor and copy assignment on purpose
  // because we want the compiler generated versions
  explicit inline MPoint(int _x = 0, int _y = 0) : x(_x), y(_y) {}

 public:  ////                Operators.
  // Checks for equality between two points.
  inline bool operator==(MPoint const& rhs) const {
    return (x == rhs.x) && (y == rhs.y);
  }

  // Checks for inequality between two points.
  inline bool operator!=(MPoint const& rhs) const { return !operator==(rhs); }

  inline bool operator<(MPoint const& rhs) const {
    return y < rhs.y || (y == rhs.y && x < rhs.x);
  }

  inline MPoint& operator+=(MPoint const& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  inline MPoint& operator-=(MPoint const& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }

  inline MPoint operator+(MPoint const& rhs) const {
    MPoint const result(x + rhs.x, y + rhs.y);
    return result;
  }

  inline MPoint operator-(MPoint const& rhs) const {
    MPoint const result(x - rhs.x, y - rhs.y);
    return result;
  }

  inline MPoint& operator-() {
    x = -x;
    y = -y;
    return *this;
  }

 public:  // Attributes.
  inline bool isOrigin() const { return !(x | y); }
#endif  // __cplusplus
};

/**
 *
 * @brief Camera size type.
 *
 */
struct MSize {
  typedef int value_type;
  value_type w;
  value_type h;

#ifdef __cplusplus

 public:  // Instantiation.
  // we don't provide copy-ctor and copy assignment on purpose
  // because we want the compiler generated versions
  inline MSize() : w(0), h(0) {}

  inline MSize(int _w, int _h) : w(_w), h(_h) {}

  inline MSize(MPoint const& topLeft, MPoint const& bottomRight)
      : w(bottomRight.x - topLeft.x), h(bottomRight.y - topLeft.y) {}

 public:  ////                Operations.
  // Returns the product of w and h.
  inline value_type size() const { return w * h; }

 public:  ////                Operators.
  // Checks for invalid size with width <= 0 or height <= 0.
  inline bool operator!() const { return (w <= 0) || (h <= 0); }

  // Checks for equality between two sizes.
  inline bool operator==(MSize const& rhs) const {
    return (w == rhs.w) && (h == rhs.h);
  }

  // Checks for inequality between two sizes.
  inline bool operator!=(MSize const& rhs) const { return !operator==(rhs); }

  // Adds a size to this size.
  inline MSize& operator+=(MSize const& rhs) {
    w += rhs.w;
    h += rhs.h;
    return *this;
  }

  // Subtracts a size from this size.
  inline MSize& operator-=(MSize const& rhs) {
    w -= rhs.w;
    h -= rhs.h;
    return *this;
  }

  // Adds two sizes.
  inline MSize operator+(MSize const& rhs) const {
    MSize const result(w + rhs.w, h + rhs.h);
    return result;
  }

  // Subtracts two sizes.
  inline MSize operator-(MSize const& rhs) const {
    MSize const result(w - rhs.w, h - rhs.h);
    return result;
  }

  // Multiplies a size by a scalar.
  inline MSize operator*(value_type scalar) const {
    MSize const result(w * scalar, h * scalar);
    return result;
  }

  // Divides a size by a scalar.
  inline MSize operator/(value_type scalar) const {
    MSize const result(w / scalar, h / scalar);
    return result;
  }

  // Shifts bits in a size to the right.
  inline MSize operator>>(value_type shift) const {
    MSize const result(w >> shift, h >> shift);
    return result;
  }

  // Shifts bits in a size to the left.
  inline MSize operator<<(value_type shift) const {
    MSize const result(w << shift, h << shift);
    return result;
  }

#endif  // __cplusplus
};

/**
 *
 * @brief Camera rectangle type.
 *
 */
struct MRect {
  typedef int value_type;
  MPoint p;  //  left-top corner
  MSize s;   //  width, height

#ifdef __cplusplus

 public:  ////                Instantiation.
  // we don't provide copy-ctor and copy assignment on purpose
  // because we want the compiler generated versions
  inline MRect() : p(0, 0), s(0, 0) {}

  inline MRect(int _w, int _h) : p(0, 0), s(_w, _h) {}

  inline MRect(MPoint const& topLeft, MPoint const& bottomRight)
      : p(topLeft), s(topLeft, bottomRight) {}

  inline MRect(MPoint const& _p, MSize const& _s) : p(_p), s(_s) {}

 public:  ////                Operators.
  // Checks for equality between two rectangles.
  inline bool operator==(MRect const& rhs) const {
    return (p == rhs.p) && (s == rhs.s);
  }

  // Checks for inequality between two rectangles.
  inline bool operator!=(MRect const& rhs) const { return !operator==(rhs); }

 public:  ////                Accessors.
  inline MPoint leftTop() const { return p; }
  inline MPoint leftBottom() const { return MPoint(p.x, p.y + s.h); }
  inline MPoint rightTop() const { return MPoint(p.x + s.w, p.y); }
  inline MPoint rightBottom() const { return MPoint(p.x + s.w, p.y + s.h); }

  inline MSize const& size() const { return s; }

  inline value_type width() const { return s.w; }
  inline value_type height() const { return s.h; }

 public:  ////                Operations.
  inline void clear() { p.x = p.y = s.w = s.h = 0; }

#endif  // __cplusplus
};

struct MPointF;
struct MSizeF;
struct MRectF;

/**
 *
 * @brief Camera point type.
 *
 */
struct MPointF {
  typedef float value_type;

  value_type x;
  value_type y;

#ifdef __cplusplus

 public:  ////                Instantiation.
  // we don't provide copy-ctor and copy assignment on purpose
  // because we want the compiler generated versions
  inline MPointF() : x(0), y(0) {}

  inline MPointF(value_type _x, value_type _y) : x(_x), y(_y) {}

  explicit inline MPointF(MPoint const& rhs) : x(rhs.x), y(rhs.y) {}

 public:  ////                Operators.
  // Checks for equality between two points.
  inline bool operator==(MPointF const& rhs) const {
    return (x == rhs.x) && (y == rhs.y);
  }

  // Checks for inequality between two points.
  inline bool operator!=(MPointF const& rhs) const { return !operator==(rhs); }

  inline bool operator<(MPointF const& rhs) const {
    return y < rhs.y || (y == rhs.y && x < rhs.x);
  }

  inline MPointF& operator+=(MPoint const& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  inline MPointF& operator+=(MPointF const& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  inline MPointF& operator-=(MPointF const& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }

  inline MPointF& operator=(MPoint const& rhs) {
    x = rhs.x;
    y = rhs.y;
    return *this;
  }

  inline MPointF operator+(MPointF const& rhs) const {
    MPointF const result(x + rhs.x, y + rhs.y);
    return result;
  }

  inline MPointF operator+(MPoint const& rhs) const {
    MPointF const result(x + rhs.x, y + rhs.y);
    return result;
  }

  inline MPointF operator-(MPointF const& rhs) const {
    MPointF const result(x - rhs.x, y - rhs.y);
    return result;
  }

 public:  ////                Attributes.
  inline bool isOrigin() const { return (x == 0.0f) && (y == 0.0f); }

  inline MPoint toMPoint() const {
    MPoint const result(x, y);
    return result;
  }
#endif  // __cplusplus
};

/**
 *
 * @brief Camera size type.
 *
 */
struct MSizeF {
  typedef float value_type;
  value_type w;
  value_type h;

#ifdef __cplusplus

 public:  ////                Instantiation.
  // we don't provide copy-ctor and copy assignment on purpose
  // because we want the compiler generated versions
  inline MSizeF() : w(0), h(0) {}

  inline MSizeF(value_type _w, value_type _h) : w(_w), h(_h) {}

  inline MSizeF(MPointF const& topLeft, MPointF const& bottomRight)
      : w(bottomRight.x - topLeft.x), h(bottomRight.y - topLeft.y) {}

  explicit inline MSizeF(MSize const& rhs) : w(rhs.w), h(rhs.h) {}

 public:  ////                Operations.
  // Returns the product of w and h.
  inline value_type size() const { return w * h; }

 public:  ////                Operators.
  // Checks for invalid size with width <= 0 or height <= 0.
  inline bool operator!() const { return (w <= 0) || (h <= 0); }

  // Checks for equality between two sizes.
  inline bool operator==(MSizeF const& rhs) const {
    return (w == rhs.w) && (h == rhs.h);
  }

  // Checks for inequality between two sizes.
  inline bool operator!=(MSizeF const& rhs) const { return !operator==(rhs); }

  // Adds a size to this size.
  inline MSizeF& operator+=(MSizeF const& rhs) {
    w += rhs.w;
    h += rhs.h;
    return *this;
  }

  inline MSizeF& operator+=(MSize const& rhs) {
    w += rhs.w;
    h += rhs.h;
    return *this;
  }

  // Subtracts a size from this size.
  inline MSizeF& operator-=(MSizeF const& rhs) {
    w -= rhs.w;
    h -= rhs.h;
    return *this;
  }

  inline MSizeF& operator=(MSize const& rhs) {
    w = rhs.w;
    h = rhs.h;
    return *this;
  }

  // Adds two sizes.
  inline MSizeF operator+(MSizeF const& rhs) const {
    MSizeF const result(w + rhs.w, h + rhs.h);
    return result;
  }

  inline MSizeF operator+(MSize const& rhs) const {
    MSizeF const result(w + rhs.w, h + rhs.h);
    return result;
  }

  // Subtracts two sizes.
  inline MSizeF operator-(MSizeF const& rhs) const {
    MSizeF const result(w - rhs.w, h - rhs.h);
    return result;
  }

  // Multiplies a size by a scalar.
  inline MSizeF operator*(value_type scalar) const {
    MSizeF const result(w * scalar, h * scalar);
    return result;
  }

  // Divides a size by a scalar.
  inline MSizeF operator/(value_type scalar) const {
    MSizeF const result(w / scalar, h / scalar);
    return result;
  }

 public:  ////                Operations.
  inline MSize toMSize() const {
    MSize const result(w, h);
    return result;
  }
#endif  // __cplusplus
};

/**
 *
 * @brief Camera rectangle type.
 *
 */
struct MRectF {
  typedef float value_type;
  MPointF p;  //  left-top corner
  MSizeF s;   //  width, height

#ifdef __cplusplus

 public:  ////                Instantiation.
  // we don't provide copy-ctor and copy assignment on purpose
  // because we want the compiler generated versions
  inline MRectF() : p(0, 0), s(0, 0) {}

  inline MRectF(value_type _w, value_type _h) : p(0, 0), s(_w, _h) {}

  inline MRectF(MPointF const& topLeft, MPointF const& bottomRight)
      : p(topLeft), s(topLeft, bottomRight) {}

  inline MRectF(MPointF const& _p, MSizeF const& _s) : p(_p), s(_s) {}

  inline MRectF(MPoint const& _p, MSize const& _s) : p(_p), s(_s) {}

 public:  ////                Operators.
  // Checks for equality between two rectangles.
  inline bool operator==(MRectF const& rhs) const {
    return (p == rhs.p) && (s == rhs.s);
  }

  // Checks for inequality between two rectangles.
  inline bool operator!=(MRectF const& rhs) const { return !operator==(rhs); }

  inline MRectF& operator=(MRect const& rhs) {
    p = rhs.p;
    s = rhs.s;
    return *this;
  }

 public:  ////                Accessors.
  inline MPointF leftTop() const { return p; }
  inline MPointF leftBottom() const { return MPointF(p.x, p.y + s.h); }
  inline MPointF rightTop() const { return MPointF(p.x + s.w, p.y); }
  inline MPointF rightBottom() const { return MPointF(p.x + s.w, p.y + s.h); }

  inline MSizeF const& size() const { return s; }

  inline value_type width() const { return s.w; }
  inline value_type height() const { return s.h; }

 public:  ////                Operations.
  inline void clear() { p.x = p.y = s.w = s.h = 0; }
  inline MRect toMRect() const {
    MRect const result(p.toMPoint(), s.toMSize());
    return result;
  }

#endif  // __cplusplus
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_UITYPES_H_
