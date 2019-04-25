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

#ifndef CAMERA_HAL_MEDIATEK_MTKLIBV4L2__VIDEODEV2_H_
#define CAMERA_HAL_MEDIATEK_MTKLIBV4L2__VIDEODEV2_H_

/* Vendor specific - Mediatek ISP compressed formats */
#ifndef V4L2_PIX_FMT_MTISP_U8
#define V4L2_PIX_FMT_MTISP_U8 v4l2_fourcc('M', 'T', 'U', '8') /* 8 bit */
#endif

#ifndef V4L2_PIX_FMT_MTISP_U10
#define V4L2_PIX_FMT_MTISP_U10 v4l2_fourcc('M', 'T', 'U', 'A') /* 10 bit */
#endif

#ifndef V4L2_PIX_FMT_MTISP_U12
#define V4L2_PIX_FMT_MTISP_U12 v4l2_fourcc('M', 'T', 'U', 'C') /* 12 bit */
#endif

#ifndef V4L2_PIX_FMT_MTISP_U14
#define V4L2_PIX_FMT_MTISP_U14 v4l2_fourcc('M', 'T', 'U', 'E') /* 14 bit */
#endif

#ifndef V4L2_META_FMT_MTISP_PARAMS
/* Vendor specific - Mediatek ISP parameters for firmware */
#define V4L2_META_FMT_MTISP_PARAMS v4l2_fourcc('M', 'T', 'f', 'p')
#endif

// bayer order expansion
#ifndef V4L2_PIX_FMT_MTISP_SBGGR8
#define V4L2_PIX_FMT_MTISP_SBGGR8 \
  v4l2_fourcc('M', 'B', 'B', '8') /*  Packed  8-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGBRG8
#define V4L2_PIX_FMT_MTISP_SGBRG8 \
  v4l2_fourcc('M', 'B', 'G', '8') /*  Packed  8-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGRBG8
#define V4L2_PIX_FMT_MTISP_SGRBG8 \
  v4l2_fourcc('M', 'B', 'g', '8') /*  Packed  8-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SRGGB8
#define V4L2_PIX_FMT_MTISP_SRGGB8 \
  v4l2_fourcc('M', 'B', 'R', '8') /*  Packed  8-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SBGGR10
#define V4L2_PIX_FMT_MTISP_SBGGR10 \
  v4l2_fourcc('M', 'B', 'B', 'A') /*  Packed 10-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGBRG10
#define V4L2_PIX_FMT_MTISP_SGBRG10 \
  v4l2_fourcc('M', 'B', 'G', 'A') /*  Packed 10-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGRBG10
#define V4L2_PIX_FMT_MTISP_SGRBG10 \
  v4l2_fourcc('M', 'B', 'g', 'A') /*  Packed 10-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SRGGB10
#define V4L2_PIX_FMT_MTISP_SRGGB10 \
  v4l2_fourcc('M', 'B', 'R', 'A') /*  Packed 10-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SBGGR12
#define V4L2_PIX_FMT_MTISP_SBGGR12 \
  v4l2_fourcc('M', 'B', 'B', 'C') /*  Packed 12-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGBRG12
#define V4L2_PIX_FMT_MTISP_SGBRG12 \
  v4l2_fourcc('M', 'B', 'G', 'C') /*  Packed 12-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGRBG12
#define V4L2_PIX_FMT_MTISP_SGRBG12 \
  v4l2_fourcc('M', 'B', 'g', 'C') /*  Packed 12-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SRGGB12
#define V4L2_PIX_FMT_MTISP_SRGGB12 \
  v4l2_fourcc('M', 'B', 'R', 'C') /*  Packed 12-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SBGGR14
#define V4L2_PIX_FMT_MTISP_SBGGR14 \
  v4l2_fourcc('M', 'B', 'B', 'E') /*  Packed 14-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGBRG14
#define V4L2_PIX_FMT_MTISP_SGBRG14 \
  v4l2_fourcc('M', 'B', 'G', 'E') /*  Packed 14-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGRBG14
#define V4L2_PIX_FMT_MTISP_SGRBG14 \
  v4l2_fourcc('M', 'B', 'g', 'E') /*  Packed 14-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SRGGB14
#define V4L2_PIX_FMT_MTISP_SRGGB14 \
  v4l2_fourcc('M', 'B', 'R', 'E') /*  Packed 14-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SBGGR8F
#define V4L2_PIX_FMT_MTISP_SBGGR8F \
  v4l2_fourcc('M', 'F', 'B', '8') /*  Full-G  8-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGBRG8F
#define V4L2_PIX_FMT_MTISP_SGBRG8F \
  v4l2_fourcc('M', 'F', 'G', '8') /*  Full-G  8-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGRBG8F
#define V4L2_PIX_FMT_MTISP_SGRBG8F \
  v4l2_fourcc('M', 'F', 'g', '8') /*  Full-G  8-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SRGGB8F
#define V4L2_PIX_FMT_MTISP_SRGGB8F \
  v4l2_fourcc('M', 'F', 'R', '8') /*  Full-G  8-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SBGGR10F
#define V4L2_PIX_FMT_MTISP_SBGGR10F \
  v4l2_fourcc('M', 'F', 'B', 'A') /*  Full-G 10-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGBRG10F
#define V4L2_PIX_FMT_MTISP_SGBRG10F \
  v4l2_fourcc('M', 'F', 'G', 'A') /*  Full-G 10-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGRBG10F
#define V4L2_PIX_FMT_MTISP_SGRBG10F \
  v4l2_fourcc('M', 'F', 'g', 'A') /*  Full-G 10-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SRGGB10F
#define V4L2_PIX_FMT_MTISP_SRGGB10F \
  v4l2_fourcc('M', 'F', 'R', 'A') /*  Full-G 10-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SBGGR12F
#define V4L2_PIX_FMT_MTISP_SBGGR12F \
  v4l2_fourcc('M', 'F', 'B', 'C') /*  Full-G 12-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGBRG12F
#define V4L2_PIX_FMT_MTISP_SGBRG12F \
  v4l2_fourcc('M', 'F', 'G', 'C') /*  Full-G 12-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGRBG12F
#define V4L2_PIX_FMT_MTISP_SGRBG12F \
  v4l2_fourcc('M', 'F', 'g', 'C') /*  Full-G 12-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SRGGB12F
#define V4L2_PIX_FMT_MTISP_SRGGB12F \
  v4l2_fourcc('M', 'F', 'R', 'C') /*  Full-G 12-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SBGGR14F
#define V4L2_PIX_FMT_MTISP_SBGGR14F \
  v4l2_fourcc('M', 'F', 'B', 'E') /*  Full-G 14-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGBRG14F
#define V4L2_PIX_FMT_MTISP_SGBRG14F \
  v4l2_fourcc('M', 'F', 'G', 'E') /*  Full-G 14-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SGRBG14F
#define V4L2_PIX_FMT_MTISP_SGRBG14F \
  v4l2_fourcc('M', 'F', 'g', 'E') /*  Full-G 14-bit  */
#endif
#ifndef V4L2_PIX_FMT_MTISP_SRGGB14F
#define V4L2_PIX_FMT_MTISP_SRGGB14F \
  v4l2_fourcc('M', 'F', 'R', 'E') /*  Full-G 14-bit  */
#endif

#endif  // CAMERA_HAL_MEDIATEK_MTKLIBV4L2__VIDEODEV2_H_
