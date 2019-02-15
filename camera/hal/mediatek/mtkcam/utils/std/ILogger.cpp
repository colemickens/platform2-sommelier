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

#include <memory>
#include <mtkcam/utils/std/ILogger.h>
#include <string>

namespace NSCam {
namespace Utils {

ILog::ILog() {}

ILog::ILog(const std::shared_ptr<ILogObj>& log) : mLog(log) {}

ILog::~ILog() {}

class DefaultLogger : public ILogObj {
 public:
  DefaultLogger(const char* str,
                const char* name,
                unsigned level,
                unsigned sensor,
                unsigned mwFrame,
                unsigned mwRequest,
                unsigned frame,
                unsigned request);
  virtual ~DefaultLogger();
  const char* getLogStr() const;
  const char* getUserName() const;
  unsigned getLogLevel() const;
  unsigned getLogSensorID() const;
  unsigned getLogFrameID() const;
  unsigned getLogMWFrameID() const;
  unsigned getLogRequestID() const;
  unsigned getLogMWRequestID() const;

 private:
  std::string mStr;
  std::string mUserName;
  unsigned mLogLevel;
  unsigned mSensorID;
  unsigned mMWFrameID;
  unsigned mMWRequestID;
  unsigned mFrameID;
  unsigned mRequestID;
};

DefaultLogger::DefaultLogger(const char* str,
                             const char* name,
                             unsigned level,
                             unsigned sensor,
                             unsigned mwFrame,
                             unsigned mwRequest,
                             unsigned frame,
                             unsigned request)
    : mStr(str),
      mUserName(name),
      mLogLevel(level),
      mSensorID(sensor),
      mMWFrameID(mwFrame),
      mMWRequestID(mwRequest),
      mFrameID(frame),
      mRequestID(request) {}

DefaultLogger::~DefaultLogger() {}

const char* DefaultLogger::getLogStr() const {
  return mStr.c_str();
}

const char* DefaultLogger::getUserName() const {
  return mUserName.c_str();
}

unsigned DefaultLogger::getLogLevel() const {
  return mLogLevel;
}

unsigned DefaultLogger::getLogSensorID() const {
  return mSensorID;
}

unsigned DefaultLogger::getLogFrameID() const {
  return mFrameID;
}

unsigned DefaultLogger::getLogRequestID() const {
  return mRequestID;
}

unsigned DefaultLogger::getLogMWFrameID() const {
  return mMWFrameID;
}

unsigned DefaultLogger::getLogMWRequestID() const {
  return mMWRequestID;
}

ILog makeLogger(const char* str,
                const char* name,
                unsigned logLevel,
                unsigned sensorID,
                unsigned mwFrameID,
                unsigned mwRequestID,
                unsigned frameID,
                unsigned requestID) {
  return ILog(std::make_shared<DefaultLogger>(str, name, logLevel, sensorID,
                                              mwFrameID, mwRequestID, frameID,
                                              requestID));
}

ILog makeSensorLogger(const char* name, unsigned logLevel, unsigned sensorID) {
  char str[128];
  snprintf(str, sizeof(str), "%s cam %d", name, sensorID);
  return makeLogger(str, name, logLevel, sensorID);
}

ILog makeFrameLogger(const char* name,
                     unsigned logLevel,
                     unsigned sensorID,
                     unsigned mwFrameID,
                     unsigned mwRequestID,
                     unsigned frameID) {
  char str[128];
  snprintf(str, sizeof(str), "%s cam %d MWFrame:#%d MWReq:#%d, frame %d ", name,
           sensorID, mwFrameID, mwRequestID, frameID);
  return makeLogger(str, name, logLevel, sensorID, mwFrameID, mwRequestID,
                    frameID);
}

ILog makeRequestLogger(const char* name,
                       unsigned logLevel,
                       unsigned sensorID,
                       unsigned mwFrameID,
                       unsigned mwRequestID,
                       unsigned frameID,
                       unsigned requestID) {
  char str[128];
  snprintf(str, sizeof(str), "%s cam %d MWFrame:#%d MWReq:#%d, frame %d-%d ",
           name, sensorID, mwFrameID, mwRequestID, frameID, requestID);
  return makeLogger(str, name, logLevel, sensorID, mwFrameID, mwRequestID,
                    frameID, requestID);
}

ILog makeSensorLogger(const ILog& log, unsigned sensorID) {
  return makeSensorLogger(log.getUserName(), log.getLogLevel(), sensorID);
}

ILog makeFrameLogger(const ILog& log,
                     unsigned mwFrameID,
                     unsigned mwRequestID,
                     unsigned frameID) {
  return makeFrameLogger(log.getUserName(), log.getLogLevel(),
                         log.getLogSensorID(), mwFrameID, mwRequestID, frameID);
}

ILog makeRequestLogger(const ILog& log, unsigned requestID) {
  return makeRequestLogger(log.getUserName(), log.getLogLevel(),
                           log.getLogSensorID(), log.getLogMWFrameID(),
                           log.getLogMWRequestID(), log.getLogFrameID(),
                           requestID);
}

ILog makeSubSensorLogger(const ILog& log, unsigned sensorID) {
  return makeRequestLogger(log.getUserName(), log.getLogLevel(), sensorID,
                           log.getLogMWFrameID(), log.getLogMWRequestID(),
                           log.getLogFrameID(), log.getLogRequestID());
}

}  // namespace Utils
}  // namespace NSCam
