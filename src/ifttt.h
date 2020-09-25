/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
/**@file
 *
 * @defgroup ifttt IFTTT webhooks
 * @brief  Module to send HTTP POST to IFTTT webhooks service
 * @{
 */

#ifndef IFTTT_H__
#define IFTTT_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ifttt_data {
    char *event;
    char *key;
    char *data;
};

/**
 * @brief Send data to IFTTT.
 *
 * @param data Pointer to the data
 *
 * @return 0 if the operation was successful, otherwise a (negative) error code.
 */
int ifttt_send(struct ifttt_data *data);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* IFTTT_H__ */