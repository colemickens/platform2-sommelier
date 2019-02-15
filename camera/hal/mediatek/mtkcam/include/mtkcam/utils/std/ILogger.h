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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_ILOGGER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_ILOGGER_H_

#include <memory>
#include <mtkcam/def/common.h>
#include <string>

namespace NSCam {
namespace Utils {

class ILogBase {
 public:
  virtual ~ILogBase() {}
  virtual const char* getLogStr() const = 0;
  virtual const char* getUserName() const = 0;
  virtual unsigned getLogLevel() const = 0;
  virtual unsigned getLogSensorID() const = 0;
  virtual unsigned getLogMWFrameID() const = 0;
  virtual unsigned getLogMWRequestID() const = 0;
  virtual unsigned getLogFrameID() const = 0;
  virtual unsigned getLogRequestID() const = 0;
};

class ILogObj : public ILogBase {
 public:
  virtual ~ILogObj() {}
};

#define DECL_ILOG(type, func, def)                \
  virtual inline type func() const {              \
    return (mLog != NULL) ? mLog->func() : (def); \
  }

class VISIBILITY_PUBLIC ILog : public ILogBase {
 public:
  ILog();
  explicit ILog(const std::shared_ptr<ILogObj>& log);
  virtual ~ILog();
  DECL_ILOG(const char*, getLogStr, "");
  DECL_ILOG(const char*, getUserName, "unknwon");
  DECL_ILOG(unsigned, getLogLevel, 0);
  DECL_ILOG(unsigned, getLogSensorID, -1);
  DECL_ILOG(unsigned, getLogMWFrameID, 0);
  DECL_ILOG(unsigned, getLogMWRequestID, 0);
  DECL_ILOG(unsigned, getLogFrameID, 0);
  DECL_ILOG(unsigned, getLogRequestID, 0);

 private:
  std::shared_ptr<ILogObj> mLog;
};
#undef DECL_ILOG

inline const char* getLogStr(const char* str) {
  return str;
}
inline const char* getLogStr(const std::string& str) {
  return str.c_str();
}
inline const char* getLogStr(const ILog& log) {
  return log.getLogStr();
}

ILog VISIBILITY_PUBLIC makeLogger(const char* str,
                                  const char* name = "unknwon",
                                  unsigned logLevel = 0,
                                  unsigned sensorID = -1,
                                  unsigned mwFrameID = 0,
                                  unsigned mwRequestID = 0,
                                  unsigned frameID = 0,
                                  unsigned requestID = 0);
ILog makeSensorLogger(const char* name, unsigned logLevel, unsigned sensorID);
ILog makeFrameLogger(const char* name,
                     unsigned logLevel,
                     unsigned sensorID,
                     unsigned mwFrameID,
                     unsigned mwRequestID,
                     unsigned frameID);
ILog makeRequestLogger(const char* name,
                       unsigned logLevel,
                       unsigned sensorID,
                       unsigned mwFrameID,
                       unsigned mwRequestID,
                       unsigned frameID,
                       unsigned requestID);
ILog VISIBILITY_PUBLIC makeSensorLogger(const ILog& log, unsigned sensorID);
ILog VISIBILITY_PUBLIC makeFrameLogger(const ILog& log,
                                       unsigned mwFrameID,
                                       unsigned mwRequestID,
                                       unsigned frameID);
ILog VISIBILITY_PUBLIC makeRequestLogger(const ILog& log, unsigned requestID);
ILog VISIBILITY_PUBLIC makeSubSensorLogger(const ILog& log, unsigned sensorID);

template <typename T>
inline ILog spToILog(const T& p) {
  return p != NULL ? p->mLog : ILog();
}

}  // namespace Utils
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_ILOGGER_H_
