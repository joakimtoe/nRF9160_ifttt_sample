/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <net/socket.h>
#include <modem/bsdlib.h>
#include <modem/lte_lc.h>
#include <modem/at_cmd.h>
#include <modem/at_notif.h>
#include <modem/modem_key_mgmt.h>
#include <dk_buttons_and_leds.h>
#include "ifttt.h"

#define DATA_FORMAT "{\"value1\":\"%s\",\"value2\":\"%s\",\"value3\":\"%s\"}"
#define VOLTAGE_LEN 4
#define TEMP_LEN 2
#define CLOCK_LEN strlen("18/12/06,22:10:00+08")
#define DATA_LEN (sizeof(DATA_FORMAT) - 6 + VOLTAGE_LEN + TEMP_LEN + CLOCK_LEN)

static struct k_work ifttt_send_work;

static int temp_read(char *buf, size_t buf_len)
{
	int err;
	char at_cmd_buf[64];

	err = at_cmd_write("AT%XTEMP?", at_cmd_buf, sizeof(at_cmd_buf), NULL);
	if (err) {
		printk("ATXTEMP?, error: %d\n", err);
		return err;
	}
	if (!strcmp(at_cmd_buf, "%XTEMP:")) {
		printk("Unexpected response: %s\n", at_cmd_buf);
		return -EINVAL;
	}
	memcpy(buf, &at_cmd_buf[strlen("%XTEMP: ")], TEMP_LEN);
	buf[TEMP_LEN] = 0;
	
	return 0;
}

static int voltage_read(char *buf, size_t buf_len)
{
	int err;
	char at_cmd_buf[64];

	err = at_cmd_write("AT%XVBAT", at_cmd_buf, sizeof(at_cmd_buf), NULL);
	if (err) {
		printk("ATXVBAT, error: %d\n", err);
		return err;
	}
	if (!strcmp(at_cmd_buf, "%XVBAT:")) {
		printk("Unexpected response: %s\n", at_cmd_buf);
		return -EINVAL;
	}
	memcpy(buf, &at_cmd_buf[strlen("%XVBAT: ")], VOLTAGE_LEN);
	buf[VOLTAGE_LEN] = 0;
	
	return 0;
}

static int clock_read(char *buf, size_t buf_len)
{
	int err;
	char at_cmd_buf[64];

	err = at_cmd_write("AT+CCLK?", at_cmd_buf, sizeof(at_cmd_buf), NULL);
	if (err) {
		printk("AT+CCLK?, error: %d\n", err);
		return err;
	}
	if (!strcmp(at_cmd_buf, "+CCLK:")) {
		printk("Unexpected response: %s\n", at_cmd_buf);
		return -EINVAL;
	}
	memcpy(buf, &at_cmd_buf[strlen("+CCLK: \"")], CLOCK_LEN);
	buf[CLOCK_LEN] = 0;

	return 0;
}

static void ifttt_send_work_fn(struct k_work *work)
{
	int err;
	char temp_str[TEMP_LEN + 1];
	char voltage_str[VOLTAGE_LEN + 1];
	char clock_str[CLOCK_LEN + 1];
	char data_str[DATA_LEN];

	NRF_UARTE0_NS->ENABLE = 8;
	printk("ifttt_send_work_fn\n");

	err = temp_read(temp_str, sizeof(temp_str));
	if (err) {
		printk("temp_read, error: %d\n", err);
		goto cleanup;
	}

	err = voltage_read(voltage_str, sizeof(voltage_str));
	if (err) {
		printk("voltage_read, error: %d\n", err);
		goto cleanup;
	}

	err = clock_read(clock_str, sizeof(clock_str));
	if (err) {
		printk("clock_read, error: %d\n", err);
		goto cleanup;
	}

	sprintf(data_str, DATA_FORMAT, temp_str, voltage_str, clock_str);
	printk("data=%s\n", data_str);

	struct ifttt_data data = {
		.event = CONFIG_IFTTT_EVENT,
		.key = CONFIG_IFTTT_KEY,
		.data = data_str
	};

	err = ifttt_send(&data);
	if (err) {
		printk("ifttt_send, error: %d\n", err);
	}

cleanup:
	printk("ifttt_send_work_fn exit\n");
	NRF_UARTE0_NS->ENABLE = 0;
}

/**@brief Callback for button events from the DK buttons and LEDs library. */
static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	uint8_t btn_num;

	while (has_changed) {
		btn_num = 0;

		/* Get bit position for next button that changed state. */
		for (uint8_t i = 0; i < 32; i++) {
			if (has_changed & BIT(i)) {
				btn_num = i + 1;
				break;
			}
		}

		/* Button number has been stored, remove from bitmask. */
		has_changed &= ~(1UL << (btn_num - 1));

		if((btn_num == 1) && (button_states & BIT(btn_num - 1))) {
			k_work_submit(&ifttt_send_work);
		}
	}
}

int lte_psm_edrx_config(void)
{
	int err;

	/** eDRX and PTW config */
	/*err = lte_lc_edrx_req(true);
	if (err) {
		printk("lte_lc_edrx_req, error: %d\n", err);
		return err;
	}*/
	/** Power Saving Mode */
	err = lte_lc_psm_req(true);
	if (err) {
		printk("lte_lc_psm_req, error: %d\n", err);
		return err;
	}

	return 0;
}

void main(void)
{
	int err;

	/* Stop the UART RX for power consumption reasons */
    NRF_UARTE0_NS->TASKS_STOPRX = 1;
	NRF_UARTE1_NS->TASKS_STOPRX = 1;

	printk("IFTTT webhook sample started\n\r");

	k_work_init(&ifttt_send_work, ifttt_send_work_fn);

	err = dk_buttons_init(button_handler);
	if (err) {
		printk("Failed to initialize buttons, err code: %d\n",	err);
		return;
	}

	err = lte_psm_edrx_config();
	if (err) {
		printk("Failed to configure PSM and eDRX!\n");
		return;
	}

	printk("Waiting for network.. ");
	err = lte_lc_init_and_connect();
	if (err) {
		printk("Failed to connect to the LTE network, err %d\n", err);
		return;
	}

	err = lte_psm_edrx_config();
	if (err) {
		printk("Failed to configure PSM and eDRX!\n");
		return;
	}

	printk("OK\n");

	NRF_UARTE0_NS->ENABLE = 0;
	NRF_UARTE1_NS->ENABLE = 0;
}
