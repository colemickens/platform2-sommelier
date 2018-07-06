// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/qmi_uim.h"

struct qmi_elem_info uim_qmi_result_ei[] = {
    {
        .data_type = QMI_UNSIGNED_2_BYTE,
        .elem_len = 1,
        .elem_size = sizeof(uint16_t),
        .offset = offsetof(struct uim_qmi_result, result),
    },
    {
        .data_type = QMI_UNSIGNED_2_BYTE,
        .elem_len = 1,
        .elem_size = sizeof(uint16_t),
        .offset = offsetof(struct uim_qmi_result, error),
    },
    {}};

struct qmi_elem_info uim_card_result_t_ei[] = {
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = 1,
        .elem_size = sizeof(uint8_t),
        .offset = offsetof(struct uim_card_result_t, sw1),
    },
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = 1,
        .elem_size = sizeof(uint8_t),
        .offset = offsetof(struct uim_card_result_t, sw2),
    },
    {}};

struct qmi_elem_info uim_open_logical_channel_req_ei[] = {
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = 1,
        .elem_size = sizeof(uint8_t),
        .tlv_type = 1,
        .offset = offsetof(struct uim_open_logical_channel_req, slot),
    },
    {
        .data_type = QMI_OPT_FLAG,
        .elem_len = 1,
        .elem_size = sizeof(bool),
        .tlv_type = 16,
        .offset = offsetof(struct uim_open_logical_channel_req, aid_valid),
    },
    {
        .data_type = QMI_DATA_LEN,
        .elem_len = 1,
        .elem_size = sizeof(uint8_t),
        .tlv_type = 16,
        .offset = offsetof(struct uim_open_logical_channel_req, aid_len),
    },
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = ARRAY_SIZE,
        .elem_size = sizeof(uint8_t),
        .array_type = VAR_LEN_ARRAY,
        .tlv_type = 16,
        .offset = offsetof(struct uim_open_logical_channel_req, aid),
    },
    {}};

struct qmi_elem_info uim_open_logical_channel_resp_ei[] = {
    {
        .data_type = QMI_STRUCT,
        .elem_len = 1,
        .elem_size = sizeof(struct uim_qmi_result),
        .tlv_type = 2,
        .offset = offsetof(struct uim_open_logical_channel_resp, result),
        .ei_array = uim_qmi_result_ei,
    },
    {
        .data_type = QMI_OPT_FLAG,
        .elem_len = 1,
        .elem_size = sizeof(bool),
        .tlv_type = 16,
        .offset =
            offsetof(struct uim_open_logical_channel_resp, channel_id_valid),
    },
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = 1,
        .elem_size = sizeof(uint8_t),
        .tlv_type = 16,
        .offset = offsetof(struct uim_open_logical_channel_resp, channel_id),
    },
    {
        .data_type = QMI_OPT_FLAG,
        .elem_len = 1,
        .elem_size = sizeof(bool),
        .tlv_type = 17,
        .offset =
            offsetof(struct uim_open_logical_channel_resp, card_result_valid),
    },
    {
        .data_type = QMI_STRUCT,
        .elem_len = 1,
        .elem_size = sizeof(struct uim_card_result_t),
        .tlv_type = 17,
        .offset = offsetof(struct uim_open_logical_channel_resp, card_result),
        .ei_array = uim_card_result_t_ei,
    },
    {
        .data_type = QMI_OPT_FLAG,
        .elem_len = 1,
        .elem_size = sizeof(bool),
        .tlv_type = 18,
        .offset = offsetof(struct uim_open_logical_channel_resp,
                           select_response_valid),
    },
    {
        .data_type = QMI_DATA_LEN,
        .elem_len = 1,
        .elem_size = sizeof(uint8_t),
        .tlv_type = 18,
        .offset =
            offsetof(struct uim_open_logical_channel_resp, select_response_len),
    },
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = ARRAY_SIZE,
        .elem_size = sizeof(uint8_t),
        .array_type = VAR_LEN_ARRAY,
        .tlv_type = 18,
        .offset =
            offsetof(struct uim_open_logical_channel_resp, select_response),
    },
    {}};

struct qmi_elem_info uim_reset_req_ei[] = {{}};

struct qmi_elem_info uim_reset_resp_ei[] = {
    {
        .data_type = QMI_STRUCT,
        .elem_len = 1,
        .elem_size = sizeof(struct uim_qmi_result),
        .tlv_type = 2,
        .offset = offsetof(struct uim_reset_resp, result),
        .ei_array = uim_qmi_result_ei,
    },
    {}};

struct qmi_elem_info uim_send_apdu_req_ei[] = {
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = 1,
        .elem_size = sizeof(uint8_t),
        .tlv_type = 1,
        .offset = offsetof(struct uim_send_apdu_req, slot),
    },
    {
        .data_type = QMI_DATA_LEN,
        .elem_len = 1,
        .elem_size = sizeof(uint16_t),
        .tlv_type = 2,
        .offset = offsetof(struct uim_send_apdu_req, apdu_len),
    },
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = ARRAY_SIZE,
        .elem_size = sizeof(uint8_t),
        .array_type = VAR_LEN_ARRAY,
        .tlv_type = 2,
        .offset = offsetof(struct uim_send_apdu_req, apdu),
    },
    {
        .data_type = QMI_OPT_FLAG,
        .elem_len = 1,
        .elem_size = sizeof(bool),
        .tlv_type = 16,
        .offset = offsetof(struct uim_send_apdu_req, channel_id_valid),
    },
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = 1,
        .elem_size = sizeof(uint8_t),
        .tlv_type = 16,
        .offset = offsetof(struct uim_send_apdu_req, channel_id),
    },
    {}};

struct qmi_elem_info uim_send_apdu_resp_ei[] = {
    {
        .data_type = QMI_STRUCT,
        .elem_len = 1,
        .elem_size = sizeof(struct uim_qmi_result),
        .tlv_type = 2,
        .offset = offsetof(struct uim_send_apdu_resp, result),
        .ei_array = uim_qmi_result_ei,
    },
    {
        .data_type = QMI_OPT_FLAG,
        .elem_len = 1,
        .elem_size = sizeof(bool),
        .tlv_type = 16,
        .offset = offsetof(struct uim_send_apdu_resp, apdu_response_valid),
    },
    {
        .data_type = QMI_DATA_LEN,
        .elem_len = 1,
        .elem_size = sizeof(uint16_t),
        .tlv_type = 16,
        .offset = offsetof(struct uim_send_apdu_resp, apdu_response_len),
    },
    {
        .data_type = QMI_UNSIGNED_1_BYTE,
        .elem_len = ARRAY_SIZE,
        .elem_size = sizeof(uint8_t),
        .array_type = VAR_LEN_ARRAY,
        .tlv_type = 16,
        .offset = offsetof(struct uim_send_apdu_resp, apdu_response),
    },
    {}};
