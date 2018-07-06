// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_QMI_UIM_H_
#define HERMES_QMI_UIM_H_

#include <stdbool.h>
#include <stdint.h>

#include <libqrtr.h>

#define ARRAY_SIZE 256

struct uim_qmi_result {
  uint16_t result;
  uint16_t error;
};

struct uim_card_result_t {
  uint8_t sw1;
  uint8_t sw2;
};

struct uim_open_logical_channel_req {
  uint8_t slot;
  bool aid_valid;
  uint8_t aid_len;
  uint8_t aid[ARRAY_SIZE];
};

struct uim_open_logical_channel_resp {
  struct uim_qmi_result result;
  bool channel_id_valid;
  uint8_t channel_id;
  bool card_result_valid;
  struct uim_card_result_t card_result;
  bool select_response_valid;
  uint8_t select_response_len;
  uint8_t select_response[ARRAY_SIZE];
};

struct uim_reset_req {};

struct uim_reset_resp {
  struct uim_qmi_result result;
};

struct uim_send_apdu_req {
  uint8_t slot;
  uint16_t apdu_len;
  uint8_t apdu[ARRAY_SIZE];
  bool channel_id_valid;
  uint8_t channel_id;
};

struct uim_send_apdu_resp {
  struct uim_qmi_result result;
  bool apdu_response_valid;
  uint16_t apdu_response_len;
  uint8_t apdu_response[ARRAY_SIZE];
};

extern struct qmi_elem_info uim_open_logical_channel_req_ei[];
extern struct qmi_elem_info uim_open_logical_channel_resp_ei[];
extern struct qmi_elem_info uim_reset_req_ei[];
extern struct qmi_elem_info uim_reset_resp_ei[];
extern struct qmi_elem_info uim_send_apdu_req_ei[];
extern struct qmi_elem_info uim_send_apdu_resp_ei[];

#endif  // HERMES_QMI_UIM_H_
