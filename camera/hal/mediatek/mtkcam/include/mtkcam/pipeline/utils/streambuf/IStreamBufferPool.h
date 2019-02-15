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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERPOOL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERPOOL_H_

#include <memory>
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/**
 * @class IStreamBufferPool
 *
 * @param <_IBufferT_> the type of buffer interface.
 *  This type must have operations of incStrong and decStrong.
 */
template <class _IBufferT_>
class IStreamBufferPool : public virtual std::enable_shared_from_this<
                              IStreamBufferPool<_IBufferT_> > {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Definitions.
  typedef _IBufferT_ IBufferT;
  typedef std::shared_ptr<IBufferT> SP_IBufferT;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:  ////                        Disallowed.
  //  Copy constructor and Copy assignment are disallowed.
  IStreamBufferPool(IStreamBufferPool const&);
  IStreamBufferPool& operator=(IStreamBufferPool const&);

 protected:  ////                        Instantiation.
  IStreamBufferPool() {}

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Pool/Buffer
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Operations.
  /**
   * Pool name.
   */
  virtual char const* poolName() const = 0;

  /**
   * Dump information for debug.
   */
  virtual MVOID dumpPool() const = 0;

  /**
   * Initialize the pool.
   *
   * @param[in] szCallerName: a null-terminated string for a caller name.
   *
   * @param[in] maxNumberOfBuffers: maximum number of buffers which can be
   *  allocated from this pool.
   *
   * @param[in] minNumberOfInitialCommittedBuffers: minimum number of buffers
   *  which are initially committed.
   *
   * @return 0 indicates success; non-zero indicates an error code.
   */
  virtual MERROR initPool(char const* szCallerName,
                          size_t maxNumberOfBuffers,
                          size_t minNumberOfInitialCommittedBuffers) = 0;

  /**
   * Uninitialize the pool and free all buffers.
   *
   * @param[in] szCallerName: a null-terminated string for a caller name.
   */
  virtual MVOID uninitPool(char const* szCallerName) = 0;

  /**
   * Commit all buffers in the pool.
   * This is a non-blocking call and will enforce to allocate buffers up to
   * the max. count in background.
   *
   * @param[in] szCallerName: a null-terminated string for a caller name.
   *
   * @return 0 indicates success; non-zero indicates an error code.
   */
  virtual MERROR commitPool(char const* szCallerName) = 0;

  /**
   * Try to acquire a buffer from the pool.
   *
   * @param[in] szCallerName: a null-terminated string for a caller name.
   *
   * @param[out] rpBuffer: a reference to a newly acquired buffer.
   *
   * @param[in] nsTimeout: a timeout in nanoseconds.
   *  timeout=0: this call will try acquiring a buffer and return immediately.
   *  timeout>0: this call will block to return until a buffer is acquired,
   *             the timeout expires, or an error occurs.
   *  timeout<0: this call will block to return until a buffer is acquired, or
   *             an error occurs.
   *
   * @return 0 indicates success; non-zero indicates an error code.
   */
  virtual MERROR acquireFromPool(char const* szCallerName,
                                 SP_IBufferT* rpBuffer,
                                 int64_t nsTimeout) = 0;

  /**
   * Release a buffer to the pool.
   *
   * @param[in] szCallerName: a null-terminated string for a caller name.
   *
   * @param[in] pBuffer: a buffer to release.
   *
   * @return 0 indicates success; non-zero indicates an error code.
   */
  virtual MERROR releaseToPool(char const* szCallerName,
                               SP_IBufferT pBuffer) = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERPOOL_H_
