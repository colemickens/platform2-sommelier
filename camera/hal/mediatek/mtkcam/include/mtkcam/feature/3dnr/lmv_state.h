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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_LMV_STATE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_LMV_STATE_H_

#include <memory>
#include <mutex>

class LmvStateMachine {
 public:
  enum SensorId { WIDE = 0, TELE, SENSOR_ID_MAX };

  enum LmvState {
    STATE_ON_WIDE,
    STATE_GOING_TO_TELE,
    STATE_ON_TELE,
    STATE_GOING_TO_WIDE
  };

  enum SwitchAction {
    DO_NOTHING,
    SEND_SWITCH_OUT_TO_WIDE,  // Send switch-out to the wide pipeline
    SEND_SWITCH_OUT_TO_TELE   // Send switch-out to the tele pipeline
  };

  // The result of switch_out command from P1 output metadata
  enum SwitchResult { RESULT_OK, RESULT_FAILED, RESULT_SWITCHING };

  struct Transition {
    LmvState oldState;
    LmvState newState;
  };

  struct Action {
    SwitchAction cmd;
  };

 public:
  // Create singleton when system init
  // initState    [IN]  The initial state of LMV
  static std::shared_ptr<LmvStateMachine> createInstance(
      LmvState initState = STATE_ON_WIDE);

  // Get the singleton of LMV state machine.
  static std::shared_ptr<LmvStateMachine> getInstance();

  // Query the action which can switch LMV to expected pipeline path.
  // Client MUST follow the action to send switch_out command.
  //
  // Parameters:
  //  sensor      [IN]  Expected working pipeline, wide or tele
  //  action      [OUT] Action must be followed
  //  transition  [OUT] Optional. The transition of the state machine on this
  //  call
  virtual void switchTo(SensorId sensor,
                        Action* action,
                        Transition* transition = NULL) = 0;

  // Notify the switch_out result which carried by the output metadata of P1
  //
  // Parameters:
  //  sourcePath  [IN] From which pipeline, wide or tele
  //  result      [IN] Switch_out result
  //  transition  [OUT] Optional. The transition of the state machine on this
  //  call
  virtual void notifySwitchResult(SensorId sourcePath,
                                  SwitchResult result,
                                  Transition* transition = NULL) = 0;

  // Notify the LMV data validity which carried by the output metadata of P1
  //
  // Parameters:
  //  sourcePath  [IN] From which pipeline, wide or tele
  //  isValid     [IN] LMV data validity
  //  transition  [OUT] Optional. The transition of the state machine on this
  //  call
  virtual void notifyLmvValidity(SensorId sourcePath,
                                 bool isValid,
                                 Transition* transition = NULL) = 0;

  // Get current LMV state
  LmvState getCurrentState();

  // LMV state needs extra enques to get stable.
  // FlowControl has to continue enqueing until this API return false
  // Parameters:
  //  sensor      [IN] Does the sensor need enque?
  //  willFollow  [IN] FlowControl guarantees the enque will be performed
  //                   If true, needEnque() will decrease internal counter
  virtual bool needEnque(SensorId sensor, bool willFollow) = 0;

  // Not allow copy-construction
  LmvStateMachine(const LmvStateMachine&) = delete;

  // Not allow copy-assignment
  LmvStateMachine& operator=(const LmvStateMachine&) = delete;

 protected:
  std::mutex mStateMutex;
  LmvState mCurrentState;

  explicit LmvStateMachine(LmvState initState);
  virtual ~LmvStateMachine();

 private:
  static std::shared_ptr<LmvStateMachine> spInstance;
  static std::mutex sSingletonMutex;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_LMV_STATE_H_
