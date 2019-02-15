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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_BASICPROCESSOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_BASICPROCESSOR_H_

#include "P2_Processor.h"
#include "P2_Util.h"
#include <list>
#include <memory>
#include <vector>
namespace P2 {

class BasicProcessor : virtual public Processor<P2InitParam,
                                                P2ConfigParam,
                                                std::shared_ptr<P2Request>> {
 public:
  BasicProcessor();
  virtual ~BasicProcessor();

 protected:
  virtual MBOOL onInit(const P2InitParam& param);
  virtual MVOID onUninit();
  virtual MVOID onThreadStart();
  virtual MVOID onThreadStop();
  virtual MBOOL onConfig(const P2ConfigParam& param);
  virtual MBOOL onEnque(const std::shared_ptr<P2Request>& request);
  virtual MVOID onNotifyFlush();
  virtual MVOID onWaitFlush();

 private:
  class P2Payload;
  class P2Cookie;

 private:
  MBOOL initNormalStream();
  MVOID uninitNormalStream();
  MBOOL init3A();
  MVOID uninit3A();
  MVOID onP2CB(const QParams& qparams,
               std::shared_ptr<P2Payload> const& payload);
  MBOOL processVenc(const std::shared_ptr<P2Request>& request);
  MBOOL configVencStream(MBOOL enable, MINT32 fps = 0, MSize size = MSize());

 private:
  ILog mLog;
  P2Info mP2Info;
  std::shared_ptr<INormalStream> mNormalStream = nullptr;
  std::shared_ptr<IHal3A> mHal3A = nullptr;
  MBOOL mEnableVencStream = MFALSE;
  std::vector<std::shared_ptr<IImageBuffer>> mTuningBuffers;

 private:
  class P2Payload {
   public:
    P2Payload();
    explicit P2Payload(const std::shared_ptr<P2Request>& request);
    virtual ~P2Payload();
    std::shared_ptr<P2Request> mRequest;
    QParams mQParams;
    TuningParam mTuning;
    P2Util::SimpleIO mIO;
    P2Obj mP2Obj;
  };

  class P2Cookie {
   public:
    P2Cookie(BasicProcessor* parent, const std::shared_ptr<P2Payload>& payload);
    P2Cookie(BasicProcessor* parent,
             const std::vector<std::shared_ptr<P2Payload>>& payloads);
    MVOID updateResult(MBOOL result);
    ILog getILog();

    BasicProcessor* mParent;
    std::vector<std::shared_ptr<P2Payload>> mPayloads;
  };
  std::mutex mP2CookieMutex;
  std::condition_variable mP2Condition;
  std::list<P2Cookie*> mP2CookieList;

  enum { NO_CHECK_ORDER = 0, CHECK_ORDER = 1 };

  ILog getILog(const std::shared_ptr<P2Payload>& payload);
  ILog getILog(const std::vector<std::shared_ptr<P2Payload>>& payloads);

  template <typename T>
  MBOOL processP2(T payload);
  QParams prepareEnqueQParams(std::shared_ptr<P2Payload> payload);
  QParams prepareEnqueQParams(std::vector<std::shared_ptr<P2Payload>> payload);
  MVOID updateResult(std::shared_ptr<P2Payload> payload, MBOOL result);
  MVOID updateResult(std::vector<std::shared_ptr<P2Payload>> payload,
                     MBOOL result);
  MVOID processP2CB(const QParams& qparams, P2Cookie* cookie);
  template <typename T>
  P2Cookie* createCookie(const ILog& log, const T& payload);
  MBOOL freeCookie(P2Cookie* cookie, MBOOL checkOrder);
  static MVOID p2CB(QParams* pQparams);
  MVOID waitP2CBDone();

 private:
  std::vector<std::shared_ptr<P2Payload>> mBurstQueue;

  MBOOL processBurst(const std::shared_ptr<P2Payload>& payload);
  MBOOL checkBurst();
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_BASICPROCESSOR_H_
