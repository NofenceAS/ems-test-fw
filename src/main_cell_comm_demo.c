/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Sample app to demonstrate cellular communication over Ublox SARA-R4(22) modem.
 */

#include <autoconf.h>
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include <stdio.h>
#include <sys/printk.h>
#include <zephyr.h>

#include <net/net_if.h>
#include <net/net_mgmt.h>
#include <net/net_event.h>
#include <net/net_conn_mgr.h>
#include <net/socket.h>

#include "cellular_communicator_demo/common.h"

//LOG_MODULE_REGISTER(main);
LOG_MODULE_REGISTER(net_echo_client_sample, LOG_LEVEL_DBG);
#define GSM_DEVICE DT_LABEL(DT_INST(0, ublox_sara_r4))
#define APP_BANNER "Starting cellular communication demo"

#define INVALID_SOCK (-1)
//#define EVENT_MASK (NET_EVENT_L4_CONNECTED | \
//		    NET_EVENT_L4_DISCONNECTED)

/******************************************************************************/
/* Local Data Definitions                                                     */
/******************************************************************************/
static struct net_if *iface;
static struct net_if_config *cfg;

//static struct lte_status lteStatus;
//static lte_event_function_t lteCallbackFunction = NULL;

int lteInit(void)
{
    int rc = 1;

    /* wait for network interface to be ready */
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("Could not get iface (network interface)!");
        rc = -1;
        goto exit;
    }

    cfg = net_if_get_config(iface);
    if (!cfg) {
        LOG_ERR("Could not get iface config!");
        rc = -2;
        goto exit;
    }
    return rc;

    exit:
    return rc;
}

bool lteIsReady(void)
{
#ifdef CONFIG_DNS_RESOLVER
    struct sockaddr_in *dnsAddr;

	if (iface != NULL && cfg != NULL && &dns->servers[0] != NULL) {
		dnsAddr = net_sin(&dns->servers[0].dns_server);
		return net_if_is_up(iface) && cfg->ip.ipv4 &&
		       !net_ipv4_is_addr_unspecified(&dnsAddr->sin_addr);
	}
#else
    if (iface != NULL && cfg != NULL) {
        return net_if_is_up(iface) && cfg->ip.ipv4;
    }
#endif /* CONFIG_DNS_RESOLVER */
    return false;
}

APP_DMEM struct configs conf = {
        .ipv4 = {
                .proto = "IPv4",
                .udp.sock = INVALID_SOCK,
                .tcp.sock = INVALID_SOCK,
        },
        .ipv6 = {
                .proto = "IPv6",
                .udp.sock = INVALID_SOCK,
                .tcp.sock = INVALID_SOCK,
        },
};

static APP_BMEM struct pollfd fds[4];
static APP_BMEM int nfds;

static APP_BMEM bool connected;
K_SEM_DEFINE(run_app, 0, 1);

static struct net_mgmt_event_callback mgmt_cb;

static void prepare_fds(void)
{
    if (conf.ipv4.udp.sock >= 0) {
        fds[nfds].fd = conf.ipv4.udp.sock;
        fds[nfds].events = POLLIN;
        nfds++;
    }

    if (conf.ipv4.tcp.sock >= 0) {
        fds[nfds].fd = conf.ipv4.tcp.sock;
        fds[nfds].events = POLLIN;
        nfds++;
    }

    if (conf.ipv6.udp.sock >= 0) {
        fds[nfds].fd = conf.ipv6.udp.sock;
        fds[nfds].events = POLLIN;
        nfds++;
    }

    if (conf.ipv6.tcp.sock >= 0) {
        fds[nfds].fd = conf.ipv6.tcp.sock;
        fds[nfds].events = POLLIN;
        nfds++;
    }
}

static void wait(void)
{
    /* Wait for event on any socket used. Once event occurs,
     * we'll check them all.
     */
    if (poll(fds, nfds, -1) < 0) {
        LOG_ERR("Error in poll:%d", errno);
    }
}

static int start_udp_and_tcp(void)
{
    int ret;

    LOG_INF("Starting...\n");

    if (IS_ENABLED(CONFIG_NET_TCP)) {
        ret = start_tcp();
        if (ret < 0) {
            return ret;
        }
    }

    if (IS_ENABLED(CONFIG_NET_UDP)) {
        ret = start_udp();
        if (ret < 0) {
            return ret;
        }
    }

    /* prepare_fds(); might be needed for listening sockets */

    return 0;
}

static int run_udp_and_tcp(void)
{
    int ret;

    /* wait(); might be needed for listening sockets */

    if (IS_ENABLED(CONFIG_NET_TCP)) {
        ret = process_tcp();
        if (ret < 0) {
            return ret;
        }
    }

    if (IS_ENABLED(CONFIG_NET_UDP)) {
        ret = process_udp();
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

static void stop_udp_and_tcp(void)
{
    LOG_INF("Stopping...");

    if (IS_ENABLED(CONFIG_NET_UDP)) {
        stop_udp();
    }

    if (IS_ENABLED(CONFIG_NET_TCP)) {
        stop_tcp();
    }
}

//static void event_handler(struct net_mgmt_event_callback *cb,
//                          uint32_t mgmt_event, struct net_if *iface)
//{
//    if ((mgmt_event & EVENT_MASK) != mgmt_event) {
//        return;
//    }
//
//    if (mgmt_event == NET_EVENT_L4_CONNECTED) {
//        LOG_INF("Network connected");
//
//        connected = true;
//        conf.ipv4.udp.mtu = net_if_get_mtu(iface);
//        conf.ipv6.udp.mtu = conf.ipv4.udp.mtu;
//        k_sem_give(&run_app);
//
//        return;
//    }
//
//    if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
//        LOG_INF("Network disconnected");
//
//        connected = false;
//        k_sem_reset(&run_app);
//
//        return;
//    }
//}

static void init_app(void)
{
    LOG_INF(APP_BANNER);

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
    int err = tls_credential_add(CA_CERTIFICATE_TAG,
				    TLS_CREDENTIAL_CA_CERTIFICATE,
				    ca_certificate,
				    sizeof(ca_certificate));
	if (err < 0) {
		LOG_ERR("Failed to register public certificate: %d", err);
	}
#endif

#if defined(CONFIG_MBEDTLS_KEY_EXCHANGE_PSK_ENABLED)
    err = tls_credential_add(PSK_TAG,
				TLS_CREDENTIAL_PSK,
				psk,
				sizeof(psk));
	if (err < 0) {
		LOG_ERR("Failed to register PSK: %d", err);
	}
	err = tls_credential_add(PSK_TAG,
				TLS_CREDENTIAL_PSK_ID,
				psk_id,
				sizeof(psk_id) - 1);
	if (err < 0) {
		LOG_ERR("Failed to register PSK ID: %d", err);
	}
#endif
}

static int start_client(void)
{
    int iterations = CONFIG_NET_SAMPLE_SEND_ITERATIONS;
    int i = 0;
    int ret;

    while (iterations == 0 || i < iterations) {
        /* Wait for the connection. */
        LOG_INF("waiting for connection semaphore!\n");
        k_sem_take(&run_app, K_FOREVER);

        ret = start_udp_and_tcp();

        while (connected && (ret == 0)) {
            ret = run_udp_and_tcp();

            if (iterations > 0) {
                i++;
                if (i >= iterations) {
                    break;

                }
            }
        }

        stop_udp_and_tcp();
    }
    return ret;
}

void main(void)
{
    int_least8_t ret;
    printk("main %p\n", k_current_get());
    LOG_WRN("main starting!\n");
    const struct device *gsm_dev = device_get_binding(GSM_DEVICE);
    if (!gsm_dev) {
        LOG_ERR("GSM driver %s was not found!\n", GSM_DEVICE);
        return;
    }

    ret = lteInit();
    if (ret == 1)
    {
        LOG_INF("Cellular network interface ready!\n");
        connected = true;
        k_sem_give(&run_app);
    }

    start_client();
}