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
#include "ifttt.h"

#define HTTP_PORT 80

#define __HTTP_HEAD                                                          \
	"POST /trigger/" IFTTT_EVENT "/with/key/" IFTTT_KEY " HTTP/1.1\r\n"    \
	"Host: maker.ifttt.com\r\n"                                            \
	"Content-Type: application/json\r\n"                                   \
	"Content-Length: " IFTTT_DATA_LEN "\r\n\r\n"                           \
	IFTTT_DATA "\r\n"

#define HTTP_HEAD                                                          \
	"POST /trigger/%s/with/key/%s HTTP/1.1\r\n"                            \
	"Host: maker.ifttt.com\r\n"                                            \
	"Content-Type: application/json\r\n"                                   \
	"Content-Length: %d\r\n\r\n"                                           \
	"%s\r\n"

#define RECV_BUF_SIZE 1024

#define HTTP_REQ_LEN(x) \
	(strlen(HTTP_HEAD) - 8 + strlen(x->event) + strlen(x->key) + strlen(x->data) + 2)

static int build_http_request(char *req, struct ifttt_data *data)
{
	return sprintf(req, HTTP_HEAD, data->event, data->key, strlen(data->data), data->data);
}

int ifttt_send(struct ifttt_data *data)
{
	int err;
	int fd;
	char *p;
	int bytes;
	size_t off;
	struct addrinfo *res;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	char send_buf[HTTP_REQ_LEN(data)];
	char recv_buf[RECV_BUF_SIZE];

	int len = build_http_request(send_buf, data);
	if (len != HTTP_REQ_LEN(data)) {
		printk("build_http_request() failed, %d != %d\r\n%s", len, HTTP_REQ_LEN(data), send_buf);
		return -EINVAL;
	}

	err = getaddrinfo("maker.ifttt.com", NULL, &hints, &res);
	if (err) {
		printk("getaddrinfo() failed, err %d\n", errno);
		return -EHOSTUNREACH;
	}

	((struct sockaddr_in *)res->ai_addr)->sin_port = htons(HTTP_PORT);

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd == -1) {
		printk("Failed to open socket!\n");
		goto clean_up;
	}

	printk("Connecting to %s\n", "maker.ifttt.com");
	err = connect(fd, res->ai_addr, sizeof(struct sockaddr_in));
	if (err) {
		printk("connect() failed, err: %d\n", errno);
		goto clean_up;
	}

	off = 0;
	do {
		bytes = send(fd, &send_buf[off], HTTP_REQ_LEN(data) - off, 0);
		if (bytes < 0) {
			printk("send() failed, err %d\n", errno);
            err = -EINVAL;
			goto clean_up;
		}
		off += bytes;
	} while (off < HTTP_REQ_LEN(data));

	printk("Sent %d bytes\n", off);

	off = 0;
	do {
		bytes = recv(fd, &recv_buf[off], RECV_BUF_SIZE - off, 0);
		if (bytes < 0) {
			printk("recv() failed, err %d\n", errno);
            err = -EINVAL;
			goto clean_up;
		}
		off += bytes;
	} while (bytes != 0 /* peer closed connection */);

	printk("Received %d bytes\n", off);

	/* Print HTTP response */
	p = strstr(recv_buf, "\r\n");
	if (p) {
		off = p - recv_buf;
		recv_buf[off + 1] = '\0';
		printk("\n>\t %s\n\n", recv_buf);
        if (!strcmp(recv_buf, "HTTP/1.1 200 OK")) {
            printk("Respose not expected!\n");
            err = -EINVAL;
            goto clean_up;
        }
	} else {
		printk("%s", recv_buf);
        err = -EINVAL;
        goto clean_up;
    }

	printk("Finished, closing socket.\n");

clean_up:
	freeaddrinfo(res);
	(void)close(fd);

    return err;
}
