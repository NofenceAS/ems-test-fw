//
// Created by Per on 17.03.2017.
//

#ifndef NOFENCE_UBX_CONTROLLER_H
#define NOFENCE_UBX_CONTROLLER_H

#include "UBX.h"
#include <device.h>
#include <drivers/spi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Decoder state */
typedef enum {
	UBX_DECODE_SYNC1 = 0,
	UBX_DECODE_SYNC2,
	UBX_DECODE_CLASS,
	UBX_DECODE_ID,
	UBX_DECODE_LENGTH1,
	UBX_DECODE_LENGTH2,
	UBX_DECODE_PAYLOAD,
	UBX_DECODE_CHKSUM1,
	UBX_DECODE_CHKSUM2
} ubx_decode_state_t;

/* Rx message state */
typedef enum {
	UBX_RXMSG_IGNORE = 0,
	UBX_RXMSG_HANDLE,
	UBX_RXMSG_DISABLE,
	UBX_RXMSG_ERROR_LENGTH
} ubx_rxmsg_state_t;

/* ACK state */
typedef enum {
	UBX_ACK_IDLE = 0,
	UBX_ACK_WAITING,
	UBX_ACK_GOT_ACK,
	UBX_ACK_GOT_NAK
} ubx_ack_state_t;

typedef enum { UBX_OK = 0, UBX_ERR_ACK_NAK, UBX_ERR_TIMEOUT, UBX_ERR_SYSTEM } UBX_RET_t;

typedef enum {
	UBX_PARSING,
	UBX_CB_HANDLED_STOP_TX,
	UBX_CB_CONTINUE,
} UBX_CB_RET_t;

typedef UBX_CB_RET_t (*UBX_USER_CALLBACK_t)(uint16_t msgId, ubx_payload_t *pPayload,
					    uint16_t payload_len);

typedef struct {
	// private fields
	ubx_ack_state_t _ack_state;
	ubx_decode_state_t _decode_state;
	uint16_t _rx_msg_id;
	ubx_rxmsg_state_t _rx_state;
	uint16_t _rx_payload_length;
	uint16_t _rx_payload_index;
	uint8_t _rx_ck_a;
	uint8_t _rx_ck_b;
	uint16_t _ack_waiting_ubxId;
	size_t _buf_size;
	size_t _tx_payload_length;
	uint16_t _tx_payload_index;
	uint16_t _rx_n_FF;

	// public fields
	UBX_USER_CALLBACK_t intern_callback;
	UBX_USER_CALLBACK_t api_callback;
	uint16_t callback_payload_len;
	UBX_RET_t callback_ret;

} UBX_CONTROLLER_t;

void ubxc_init(UBX_USER_CALLBACK_t api_cb);
void ubxc_read_all(const struct device *dev, const struct spi_config *spi_config);

UBX_RET_t UBX_CFG_CFG(const struct device *dev, const struct spi_config *spi_config, uint8_t type,
		      uint32_t mask);
UBX_RET_t UBX_CFG_RXM(const struct device *dev, const struct spi_config *spi_config,
		      UBX_CFG_RXM_MODE_t mode);
UBX_RET_t UBX_CFG_PRT(const struct device *dev, const struct spi_config *spi_config, uint8_t Target,
		      uint8_t ProtoMask);
UBX_RET_t UBX_CFG_PM2_SET_MINACQTIME(const struct device *dev, const struct spi_config *spi_config,
				     uint16_t mAT);
UBX_RET_t UBX_CFG_PM2_UPDATERATE(const struct device *dev, const struct spi_config *spi_config,
				 uint32_t updatePeriod);
UBX_RET_t UBX_CFG_PM2(const struct device *dev, const struct spi_config *spi_config,
		      uint32_t updatePeriod, uint16_t minAcqTime);
UBX_RET_t UBX_CFG_PMS(const struct device *dev, const struct spi_config *spi_config, uint8_t psv,
		      uint16_t periode, uint16_t ontime);
// UBX_RET_t UBX_RXM_PMREQ(uint8_t backup, uint16_t duration, uint16_t
// wakeupsource);
UBX_RET_t UBX_CFG_ITFM(const struct device *dev, const struct spi_config *spi_config);
UBX_RET_t UBX_CFG_MSG(const struct device *dev, const struct spi_config *spi_config,
		      uint16_t CLS_ID, uint8_t Rate);
UBX_RET_t UBX_CFG_NAV5(const struct device *dev, const struct spi_config *spi_config);
UBX_RET_t UBX_CFG_NAVX5(const struct device *dev, const struct spi_config *spi_config);
UBX_RET_t UBX_CFG_INF(const struct device *dev, const struct spi_config *spi_config,
		      uint8_t Protocol, uint8_t mask);
UBX_RET_t UBX_CFG_RST(const struct device *dev, const struct spi_config *spi_config,
		      uint16_t BbrMask, uint8_t Mode);
UBX_RET_t UBX_CFG_RATE(const struct device *dev, const struct spi_config *spi_config,
		       uint16_t measRate);
UBX_RET_t UBX_CFG_GNSS(const struct device *dev, const struct spi_config *spi_config);
UBX_RET_t UBX_MGA_ANO_RAW(const struct device *dev, const struct spi_config *spi_config,
			  UBX_MGA_ANO_RAW_t *pAno);
UBX_RET_t UBX_MGA_INI_TIME_UTC(const struct device *dev, const struct spi_config *spi_config,
			       uint32_t unixTime);
UBX_RET_t UBX_RXM_PMREQ(const struct device *dev, const struct spi_config *spi_config);

#endif // NOFENCE_UBX_CONTROLLER_H
