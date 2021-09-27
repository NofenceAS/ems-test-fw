//
// Created by Per on 17.03.2017.
//

#include "ubx_controller.h"
#include "UBX.h"
#include <logging/log.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

LOG_MODULE_REGISTER(ubx_controller, CONFIG_UBX_CONTROLLER_LOG_LEVEL);

/**
 * Min CN0 as recommended by ublox. Set in both NAV5 and NAVX5
 */
#define NAV5_NAVX5_MIN_CN0 7

/**
 * Min # sattelites for navigation. Set in both NAV5 and NAVX5
 */
#define NAV5_NAVX5_MIN_NUMSV 6

#define BYTESWAP16(x) (((x) << 8) | ((x) >> 8))
#define UBX_CLASS(uint16) (((uint16) >> 8) & 0xFF)
#define UBX_MSG(uint16) ((uint16)&0xFF)

static UBX_CONTROLLER_t m_controller;

static void add_byte_to_checksum(UBX_CONTROLLER_t *p, const uint8_t b)
{
	p->_rx_ck_a = p->_rx_ck_a + b;
	p->_rx_ck_b = p->_rx_ck_b + p->_rx_ck_a;
}

void calc_checksum(const uint8_t *buffer, const uint16_t length, ubx_checksum_t *checksum)
{
	for (uint16_t i = 0; i < length; i++) {
		checksum->ck_a = checksum->ck_a + buffer[i];
		checksum->ck_b = checksum->ck_b + checksum->ck_a;
	}
}

static void decode_init(UBX_CONTROLLER_t *p)
{
	p->_decode_state = UBX_DECODE_SYNC1;
	p->_rx_ck_a = 0;
	p->_rx_ck_b = 0;
	p->_rx_payload_length = 0;
	p->_rx_payload_index = 0;
	p->_rx_n_FF = 0;
}

/**
 * Add payload rx byte
 * // -1 = error, 0 = ok, 1 = payload completed
 */
static int8_t payload_rx_add(UBX_CONTROLLER_t *p, const uint8_t b, uint8_t *pBuf)
{
	if (p->_rx_payload_index >= p->_buf_size) {
		return -1;
	}
	pBuf[p->_rx_payload_index] = b;
	if (++p->_rx_payload_index >= p->_rx_payload_length) {
		return 1;
	}
	return 0;
}

/**
 * Called when a payload has parsed and checksummed ok.
 */
static UBX_CB_RET_t payload_rx_done(UBX_CONTROLLER_t *p, ubx_payload_t *pPayload,
				    uint16_t payload_len)
{
	UBX_CB_RET_t ret = UBX_CB_CONTINUE;
	if (p->intern_callback) {
		ret = p->intern_callback(p->_rx_msg_id, pPayload, payload_len);
	}
	if (ret == UBX_CB_CONTINUE) {
		ret = p->api_callback(p->_rx_msg_id, pPayload, payload_len);
	}
	return ret;
}

/**
 * Main state machine for the UBX protocol receiver
 */
static UBX_CB_RET_t parse_char(UBX_CONTROLLER_t *p, const uint8_t b, ubx_payload_t *pBuf)
{
	UBX_CB_RET_t ret = UBX_PARSING;

	switch (p->_decode_state) {
	/* Expecting Sync1 */
	case UBX_DECODE_SYNC1:
		if (b == GPS_UBX_SYNC_CHAR_1) { // Sync1 found --> expecting Sync2
			p->_decode_state = UBX_DECODE_SYNC2;
			p->_rx_n_FF = 0;
		} else if (b == 0xFF) {
			p->_rx_n_FF++;
		}
		break;

		/* Expecting Sync2 */
	case UBX_DECODE_SYNC2:
		if (b == GPS_UBX_SYNC_CHAR_2) { // Sync2 found --> expecting Class
			p->_decode_state = UBX_DECODE_CLASS;
		} else { // Sync1 not followed by Sync2: power parser
			decode_init(p);
		}
		break;

		/* Expecting Class */
	case UBX_DECODE_CLASS:
		add_byte_to_checksum(p,
				     b); // checksum is calculated for everything except
			// Sync and Checksum bytes
		p->_rx_msg_id = b;
		p->_decode_state = UBX_DECODE_ID;
		break;

		/* Expecting ID */
	case UBX_DECODE_ID:
		add_byte_to_checksum(p, b);
		p->_rx_msg_id |= b << 8;
		p->_rx_msg_id = BYTESWAP16(p->_rx_msg_id);
		p->_decode_state = UBX_DECODE_LENGTH1;
		break;

		/* Expecting first length byte */
	case UBX_DECODE_LENGTH1:
		add_byte_to_checksum(p, b);
		p->_rx_payload_length = b;
		p->_decode_state = UBX_DECODE_LENGTH2;
		break;

		/* Expecting second length byte */
	case UBX_DECODE_LENGTH2:
		add_byte_to_checksum(p, b);
		p->_rx_payload_length |= b << 8; // calculate payload size
		p->_decode_state =
			(p->_rx_payload_length > 0) ? UBX_DECODE_PAYLOAD : UBX_DECODE_CHKSUM1;
		break;

		/* Expecting payload */
	case UBX_DECODE_PAYLOAD: {
		add_byte_to_checksum(p, b);
		int8_t ret1 = payload_rx_add(p, b, (uint8_t *)pBuf);
		if (ret1 > 0) {
			p->_decode_state = UBX_DECODE_CHKSUM1;
		} else if (ret1 < 0) {
			decode_init(p);
		}
		break;
	}
		/* Expecting first checksum byte */
	case UBX_DECODE_CHKSUM1:
		if (p->_rx_ck_a != b) {
#ifdef DEBUG_GPS
			char buf[80];
			mini_snprintf(buf, sizeof(buf), "ECHKS1, classId = 0x%x", p->_rx_msg_id);
			NF_TRACE(buf);
#endif
			decode_init(p);
		} else {
			p->_decode_state = UBX_DECODE_CHKSUM2;
		}
		break;

		/* Expecting second checksum byte */
	case UBX_DECODE_CHKSUM2:
		if (p->_rx_ck_b != b) {
		} else {
			ret = payload_rx_done(p, pBuf,
					      p->_rx_payload_length); // finish payload processing
		}
		decode_init(p);
		break;

	default:
		ret = UBX_CB_HANDLED_STOP_TX;
		break;
	}

	return ret;
}

static UBX_RET_t dispatch_FF(const struct device *dev, const struct spi_config *spi_config,
			     void *pBuf, size_t bufSize, UBX_USER_CALLBACK_t userCb)
{
	UBX_RET_t ret;
	m_controller._rx_n_FF = 0;
	m_controller._buf_size = bufSize;
	m_controller.intern_callback = userCb;
	UBX_CB_RET_t cbret, cbret_save;
#define FF_BUF_LEN 50
	uint8_t ffbuf[FF_BUF_LEN];
	const struct spi_buf rx_buf = {
		.buf = ffbuf,
		.len = sizeof(ffbuf),
	};
	const struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };
	do {
		int ret1 = spi_read(dev, spi_config, &rx);
		if (ret1) {
			LOG_ERR("spi_read %d", ret1);
			return UBX_ERR_TIMEOUT; // TODO
		}
		LOG_HEXDUMP_DBG(ffbuf, sizeof(ffbuf), "ffbuf");
		cbret_save = UBX_CB_CONTINUE;
		for (int i = 0; i < FF_BUF_LEN; i++) {
			cbret = parse_char(&m_controller, ffbuf[i], pBuf);
			if (cbret == UBX_CB_HANDLED_STOP_TX) {
				cbret_save = cbret;
			}
		}
	} while (cbret_save != UBX_CB_HANDLED_STOP_TX && m_controller._rx_n_FF < 50);
	if (m_controller._rx_n_FF >= 50) {
		ret = UBX_ERR_TIMEOUT;
	} else {
		ret = m_controller.callback_ret;
	}
	m_controller._rx_n_FF = 0;
	return ret;
}

static UBX_RET_t dispatch(const struct device *dev, const struct spi_config *spi_config,
			  uint16_t ubxId, void *pRawBuf, size_t buf_size, uint16_t payload_length,
			  UBX_USER_CALLBACK_t userCb)
{
	m_controller.callback_ret = UBX_ERR_TIMEOUT;
	m_controller.intern_callback = userCb;
	m_controller._tx_payload_index = 0;
	m_controller._buf_size = buf_size;
	m_controller._tx_payload_length = payload_length;
	m_controller._rx_n_FF = 0;

	GPS_UBX_HEAD_t header;
	ubx_checksum_t checksum;

	m_controller._ack_waiting_ubxId = ubxId;
	checksum.ck_a = 0;
	checksum.ck_b = 0;
	header.prefix = GPS_UBX_PREFIX;
	header.classId = BYTESWAP16(ubxId);
	header.size = payload_length;

	// Calculate checksum
	calc_checksum((const uint8_t *)&header.classId, sizeof(header) - 2,
		      &checksum); // skip 2 sync bytes
	if (pRawBuf != NULL) {
		calc_checksum(pRawBuf, payload_length, &checksum);
	}
	struct spi_buf rx_tx_buf[3];
	int buf_ind = 0;
	rx_tx_buf[buf_ind].buf = &header;
	rx_tx_buf[buf_ind].len = sizeof(header);
	buf_ind++;
	if (payload_length > 0) {
		rx_tx_buf[buf_ind].buf = pRawBuf;
		rx_tx_buf[buf_ind].len = payload_length;
		buf_ind++;
	}
	rx_tx_buf[buf_ind].buf = &checksum;
	rx_tx_buf[buf_ind].len = sizeof(checksum);
	buf_ind++;

	const struct spi_buf_set tx = { .buffers = rx_tx_buf, .count = buf_ind };

	struct spi_buf_set rx = { .buffers = rx_tx_buf, .count = buf_ind };
	//    for (int i = 0; i < ARRAY_SIZE(rx_tx_buf); i++) {
	//      LOG_HEXDUMP_INF(rx_tx_buf[i].buf,rx_tx_buf[i].len,"TX");
	//    }
	int ret = spi_transceive(dev, spi_config, &tx, &rx);
	if (ret) {
		LOG_ERR("SPI %d", ret);
		return UBX_ERR_SYSTEM;
	}
	for (int i = 0; i < buf_ind; i++) {
		LOG_HEXDUMP_DBG(rx_tx_buf[i].buf, rx_tx_buf[i].len, "RX");
		for (int j = 0; j < rx_tx_buf[i].len; j++) {
			parse_char(&m_controller, ((uint8_t *)rx_tx_buf[i].buf)[j], pRawBuf);
		}
	}

	// TODO, in the original AVR version, we were responsible for setting
	// the chip-select. In zephyr, this is done via transactions so in
	// order to stop sending once we got an ACK, we have to send
	// one byte at a time in the transaction. This is not optimal
	uint8_t c;
	rx_tx_buf[0].buf = &c;
	rx_tx_buf[0].len = 1;
	rx.count = 1;
	UBX_RET_t cbret;
	do {
		int ret1 = spi_read(dev, spi_config, &rx);
		if (ret1) {
			LOG_ERR("spi_read %d", ret1);
			return UBX_ERR_TIMEOUT; // TODO
		}
		cbret = parse_char(&m_controller, c, pRawBuf);
	} while (cbret != UBX_CB_HANDLED_STOP_TX && m_controller._rx_n_FF < 50);
	if (m_controller._rx_n_FF >= 50) {
		ret = UBX_ERR_TIMEOUT;
	} else {
		ret = m_controller.callback_ret;
	}
	m_controller._rx_n_FF = 0;

	return ret;
}

static UBX_CB_RET_t poll_and_stop_callback(uint16_t ubxId, ubx_payload_t *pPayload,
					   uint16_t payload_len)
{
	if (ubxId == m_controller._ack_waiting_ubxId) {
		m_controller.callback_payload_len = payload_len;
		m_controller.callback_ret = UBX_OK;
		return UBX_CB_HANDLED_STOP_TX;
	}
	return UBX_CB_CONTINUE;
}

static UBX_CB_RET_t wait_acknak_stop_tx_callback(uint16_t ubxId, ubx_payload_t *pPayload,
						 uint16_t payload_len)
{
	switch (ubxId) {
	case UBXID_ACK_ACK:
		if (pPayload->ack_ack.clsID == UBX_CLASS(m_controller._ack_waiting_ubxId) &&
		    pPayload->ack_ack.msgID == UBX_MSG(m_controller._ack_waiting_ubxId)) {
			// NF_TRACE("ACK");
			m_controller.callback_ret = UBX_OK;
			return UBX_CB_HANDLED_STOP_TX;
		} else {
			LOG_DBG("?ACK?");
		}
		break;
	case UBXID_ACK_NAK:
		if (pPayload->ack_nak.clsID == UBX_CLASS(m_controller._ack_waiting_ubxId) &&
		    pPayload->ack_nak.msgID == UBX_MSG(m_controller._ack_waiting_ubxId)) {
			LOG_DBG("NAK");
			m_controller.callback_ret = UBX_ERR_ACK_NAK;
			return UBX_CB_HANDLED_STOP_TX;

		} else {
			LOG_DBG("?NAK?");
		}
		break;

	default:
		LOG_DBG("Unknown %u", ubxId);
		break;
	}

	return UBX_CB_CONTINUE;
}

/**
 * Polls the given UBX message and waits for reply and accompanying ack
 */
static UBX_RET_t poll_and_stop_tx(const struct device *dev, const struct spi_config *spi_config,
				  uint16_t classId, ubx_payload_t *pBuf, ubx_payload_t *pAckBuf)
{
	UBX_RET_t ret =
		dispatch(dev, spi_config, classId, pBuf, sizeof(*pBuf), 0, poll_and_stop_callback);
	if (ret == UBX_OK) {
		ret = dispatch_FF(dev, spi_config, pAckBuf, sizeof(*pAckBuf),
				  wait_acknak_stop_tx_callback);
	}
	return ret;
}

static UBX_RET_t pollspec_and_stop_tx(const struct device *dev, const struct spi_config *spi_config,
				      uint16_t classId, uint8_t spec, ubx_payload_t *pBuf,
				      ubx_payload_t *pAckBuf)
{
	pBuf->raw[0] = spec;

	UBX_RET_t ret =
		dispatch(dev, spi_config, classId, pBuf, sizeof(*pBuf), 1, poll_and_stop_callback);

	if (ret == UBX_OK) {
		ret = dispatch_FF(dev, spi_config, pAckBuf, sizeof(*pAckBuf),
				  wait_acknak_stop_tx_callback);
	}

	return ret;
}

/**
 * Send a message and waits for ack, dispatching
 */
static UBX_RET_t UBX_send_and_wait_ack(const struct device *dev,
				       const struct spi_config *spi_config, uint16_t ubxId,
				       void *pBuf, size_t bufSize, size_t payloadSize)
{
	return dispatch(dev, spi_config, ubxId, pBuf, bufSize, payloadSize,
			wait_acknak_stop_tx_callback);
}

static UBX_RET_t UBX_send_nowait(const struct device *dev, const struct spi_config *spi_config,
				 uint16_t ubxId, ubx_payload_t *pBuf, size_t bufSize,
				 size_t payloadSize)
{
	UBX_RET_t ret = dispatch(dev, spi_config, ubxId, pBuf, bufSize, payloadSize, NULL);
	if (ret == UBX_ERR_TIMEOUT) {
		ret = UBX_OK;
	}
	return ret;
}

//////////////////////////
// Public API
//////////////////////////

void ubxc_init(UBX_USER_CALLBACK_t api_cb)
{
	memset(&m_controller, 0, sizeof(m_controller));
	m_controller.api_callback = api_cb;
	decode_init(&m_controller);
}

void ubxc_read_all(const struct device *dev, const struct spi_config *spi_config)
{
	ubx_payload_t buf;
	dispatch_FF(dev, spi_config, &buf, sizeof(buf), NULL);
}

UBX_RET_t UBX_CFG_CFG(const struct device *dev, const struct spi_config *spi_config, uint8_t type,
		      uint32_t mask)
{
	UBX_RET_t ret;
	ubx_payload_t buf;

	buf.cfg_cfg.clearMask = 0;
	buf.cfg_cfg.saveMask = 0;
	buf.cfg_cfg.loadMask = 0;

	if (type == 1)
		buf.cfg_cfg.clearMask = mask;
	else if (type == 2)
		buf.cfg_cfg.saveMask = mask;
	else if (type == 3)
		buf.cfg_cfg.loadMask = mask;

	ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_CFG, &buf, sizeof(buf),
				    sizeof(buf.cfg_cfg));

	return ret;
}

UBX_RET_t UBX_CFG_RXM(const struct device *dev, const struct spi_config *spi_config,
		      UBX_CFG_RXM_MODE_t mode)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_RXM, &buf, &ackBuf)) == UBX_OK) {
		if (buf.cfg_rxm.lpMode != mode) {
			buf.cfg_rxm.lpMode = mode;
			ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_RXM, &buf,
						    sizeof(buf), sizeof(buf.cfg_rxm));
		}
	}
	return ret;
}

UBX_RET_t UBX_CFG_RATE(const struct device *dev, const struct spi_config *spi_config, uint16_t Rate)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_RATE, &buf, &ackBuf)) == UBX_OK) {
		if (buf.cfg_rate.measRate != Rate) {
			buf.cfg_rate.measRate = Rate;
			ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_RATE, &buf,
						    sizeof(buf), sizeof(buf.cfg_rate));
#ifdef DEBUG_GPS
			if (ret == UBX_OK) {
				char tmp[20];
				mini_snprintf(tmp, sizeof(tmp), "RATE CHANGED %u", Rate);
				NF_TRACE(tmp);
			}
#endif
		}
	}

	return ret;
}

UBX_RET_t UBX_CFG_GNSS(const struct device *dev, const struct spi_config *spi_config)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_GNSS, &buf, &ackBuf)) == UBX_OK) {
		bool needsChange = false;
		for (uint8_t i = 0; i < buf.cfg_gnss.numConfigBlocks; i++) {
			if (buf.cfg_gnss.blocks[i].gnssId == GPS_UBX_CFG_GNSS_ID_GLONASS ||
			    buf.cfg_gnss.blocks[i].gnssId == GPS_UBX_CFG_GNSS_ID_SBAS ||
			    buf.cfg_gnss.blocks[i].gnssId == GPS_UBX_CFG_GNSS_ID_GALILEO) {
				X4 flags = buf.cfg_gnss.blocks[i].flags;
				bool isEnabled =
					(bool)(flags & GPS_UBX_CFG_GNSS_FLAG_ENABLE_BITMASK);
				if (!isEnabled) {
					needsChange = true;
					flags ^= GPS_UBX_CFG_GNSS_FLAG_ENABLE_BITMASK;
					buf.cfg_gnss.blocks[i].flags = flags;
				}
			}
		}
		if (needsChange) {
			ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_GNSS, &buf,
						    sizeof(buf), m_controller.callback_payload_len);
		}
	}

	return ret;
}

UBX_RET_t UBX_RXM_PMREQ(const struct device *dev, const struct spi_config *spi_config)
{
	UBX_RET_t ret;
	ubx_payload_t buf = { .rxm_pmreq = { .duration = 0,
					     .flags = GPS_UBX_RXM_PMREQ_FLAGS_BACKUP_MASK,
					     .wakeupsource =
						     BIT(GPS_UBX_RXM_PMREQ_WAKEUPSRC_EXTINT0) } };

	ret = UBX_send_nowait(dev, spi_config, UBXID_RXM_PMREQ, &buf, sizeof(buf),
			      sizeof(buf.rxm_pmreq));

	return ret;
}

UBX_RET_t UBX_CFG_PM2_SET_MINACQTIME(const struct device *dev, const struct spi_config *spi_config,
				     uint16_t mAT)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;
	bool needsChange = false;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_PM2, &buf, &ackBuf)) == UBX_OK) {
		if (buf.cfg_pm2.minAcqTime != mAT) {
			buf.cfg_pm2.minAcqTime = mAT;
			needsChange = true;
		} // s minimal search time
		if (needsChange) {
			ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_PM2, &buf,
						    sizeof(buf), sizeof(buf.cfg_pm2));
#ifdef DEBUG_GPS
			if (ret == UBX_OK) {
				NF_TRACE("MIN ACQ CHANGED");
			}
#endif
		}
	}

	return ret;
}

// UBX_RET_t UBX_CFG_PM2_UPDATERATE(uint32_t updatePeriod, uint8_t optTarget)
UBX_RET_t UBX_CFG_PM2_UPDATERATE(const struct device *dev, const struct spi_config *spi_config,
				 uint32_t updatePeriod)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;
	bool needsChange = false;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_PM2, &buf, &ackBuf)) == UBX_OK) {
		if (buf.cfg_pm2.updatePeriod != updatePeriod) {
			buf.cfg_pm2.updatePeriod = updatePeriod;
			needsChange = true;
		}
		if (needsChange) {
			ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_PM2, &buf,
						    sizeof(buf), sizeof(buf.cfg_pm2));
		}
#ifdef DEBUG_GPS
		if (ret == UBX_OK) {
			NF_TRACE("UPDATERATE CHANGED");
		}
#endif
	}

	return ret;
}

UBX_RET_t UBX_CFG_PM2(const struct device *dev, const struct spi_config *spi_config,
		      uint32_t updatePeriod, uint16_t minAcqTime)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;
	uint32_t Flags;
	bool needsChange = false;
	Flags = 0x00020000;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_PM2, &buf, &ackBuf)) == UBX_OK) {
		if (buf.cfg_pm2.flags != Flags) {
			buf.cfg_pm2.flags = Flags;
			needsChange = true;
		} // Flags
		if (buf.cfg_pm2.updatePeriod != updatePeriod) {
			buf.cfg_pm2.updatePeriod = updatePeriod;
			needsChange = true;
		} // ms Position update period. If set to 0, the receiver will never retry a
		// fix
		if (buf.cfg_pm2.minAcqTime != minAcqTime) {
			buf.cfg_pm2.minAcqTime = minAcqTime;
			needsChange = true;
		} // s minimal search time
		if (needsChange) {
			ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_PM2, &buf,
						    sizeof(buf), sizeof(buf.cfg_pm2));
		}
	}

	return ret;
}

UBX_RET_t UBX_CFG_PMS(const struct device *dev, const struct spi_config *spi_config, uint8_t psv,
		      uint16_t periode, uint16_t ontime)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;
	bool needsChange = false;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_PMS, &buf, &ackBuf)) == UBX_OK) {
		if (buf.cfg_pms.powersetupvalue != psv) {
			buf.cfg_pms.powersetupvalue = psv;
			needsChange = true;
		} // Set power setup value
		if (buf.cfg_pms.periode != periode) {
			buf.cfg_pms.periode = periode;
			needsChange = true;
		} // Set power setup value
		if (buf.cfg_pms.onTime != ontime) {
			buf.cfg_pms.onTime = ontime;
			needsChange = true;
		} // Set power setup value

		if (needsChange) {
			ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_PMS, &buf,
						    sizeof(buf), sizeof(buf.cfg_pms));
#ifdef DEBUG_GPS
			if (ret == UBX_OK) {
				NF_TRACE("PMS CHANGED");
			}
#endif
		}
	}

	return ret;
}

UBX_RET_t UBX_CFG_ITFM(const struct device *dev, const struct spi_config *spi_config)
{
	UBX_RET_t ret;
	ubx_payload_t buf;

	buf.cfg_itfm.config = 0;
	buf.cfg_itfm.config |= GPS_UBX_CFG_ITFM_CONFIG_RESERVED1_SET;
	buf.cfg_itfm.config |= GPS_UBX_CFG_ITFM_CONFIG_BBTRESHOLD_SET(3);
	buf.cfg_itfm.config |= GPS_UBX_CFG_ITFM_CONFIG_CWTRESHOLD_SET(15);
	buf.cfg_itfm.config |= GPS_UBX_CFG_ITFM_CONFIG_ENABLE_SET;

	buf.cfg_itfm.config2 = 0;
	buf.cfg_itfm.config2 |= GPS_UBX_CFG_ITFM_CONFIG2_RESERVED2_SET;
	buf.cfg_itfm.config2 |=
		GPS_UBX_CFG_ITFM_CONFIG2_ANTSETTING_SET(1); // 1 means setup for passive antenna

	ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_ITFM, &buf, sizeof(buf),
				    sizeof(buf.cfg_itfm));

	return ret;
}

UBX_RET_t UBX_CFG_MSG(const struct device *dev, const struct spi_config *spi_config,
		      uint16_t CLS_ID, uint8_t Rate)
{
	UBX_RET_t ret;
	ubx_payload_t buf;

	buf.cfg_msg_c.class = UBX_CLASS(CLS_ID);
	buf.cfg_msg_c.msgID = UBX_MSG(CLS_ID);
	buf.cfg_msg_c.rate = Rate;
	ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_MSG, &buf, sizeof(buf),
				    sizeof(buf.cfg_msg_c));

	return ret;
}

UBX_RET_t UBX_CFG_PRT(const struct device *dev, const struct spi_config *spi_config, uint8_t Target,
		      uint8_t ProtoMask)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;
	bool myNeedChange = false;
	uint16_t size = 0;

	// SPI_START();

	if ((ret = pollspec_and_stop_tx(dev, spi_config, UBXID_CFG_PRT, Target, &buf, &ackBuf)) ==
	    UBX_OK) {
		switch (buf.raw[0]) {
		case 0:
			size = sizeof(buf.cfg_prt_i2C_u5);
			if (buf.cfg_prt_i2C_u5.inProtoMask != ProtoMask) {
				buf.cfg_prt_i2C_u5.inProtoMask = ProtoMask;
				myNeedChange = 1;
			}
			if (buf.cfg_prt_i2C_u5.outProtoMask != ProtoMask) {
				buf.cfg_prt_i2C_u5.outProtoMask = ProtoMask;
				myNeedChange = 1;
			}
			break;
		case 1:
		case 2:
			size = sizeof(buf.cfg_prt_uart_u5);
			if (buf.cfg_prt_uart_u5.inProtoMask != ProtoMask) {
				buf.cfg_prt_uart_u5.inProtoMask = ProtoMask;
				myNeedChange = 1;
			}
			if (buf.cfg_prt_uart_u5.outProtoMask != ProtoMask) {
				buf.cfg_prt_uart_u5.outProtoMask = ProtoMask;
				myNeedChange = 1;
			}
			break;

		case 3:
			size = sizeof(buf.cfg_prt_usb_u5);
			if (buf.cfg_prt_usb_u5.inProtoMask != ProtoMask) {
				buf.cfg_prt_usb_u5.inProtoMask = ProtoMask;
				myNeedChange = 1;
			}
			if (buf.cfg_prt_usb_u5.outProtoMask != ProtoMask) {
				buf.cfg_prt_usb_u5.outProtoMask = ProtoMask;
				myNeedChange = 1;
			}
			break;

		case 4:
			size = sizeof(buf.cfg_prt_spi_u5);
			if (buf.cfg_prt_spi_u5.inProtoMask != ProtoMask) {
				buf.cfg_prt_spi_u5.inProtoMask = ProtoMask;
				myNeedChange = 1;
			}
			if (buf.cfg_prt_spi_u5.outProtoMask != ProtoMask) {
				buf.cfg_prt_spi_u5.outProtoMask = ProtoMask;
				myNeedChange = 1;
			}
			break;

		default:
			break;
		}
		if (myNeedChange) {
			ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_PRT, &buf,
						    sizeof(buf), size);
		}
	}

	// SPI_STOP();
	return ret;
}

UBX_RET_t UBX_CFG_NAV5(const struct device *dev, const struct spi_config *spi_config)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_NAV5, &buf, &ackBuf)) == UBX_OK) {
		buf.cfg_nav5.mask = 0;
		// Dynamic model : Pedestrian
		buf.cfg_nav5.mask |= BIT(0);
		buf.cfg_nav5.dynModel = 3;
		// Min elevation: 10 degrees
		//         buf.cfg_nav5.mask |= _BV(1);
		//         buf.cfg_nav5.minElev = 10;
		// fixMode : 3D only
		buf.cfg_nav5.mask |= BIT(2);
		buf.cfg_nav5.fixMode = 2;
		// cnoTresh = 7 dbHz, cnoThreshNumSVs = 6
		//        buf.cfg_nav5.mask |= _BV(8);
		//        buf.cfg_nav5.cnoThresh = NAV5_NAVX5_MIN_CN0;
		//        buf.cfg_nav5.cnoThreshNumSVs = NAV5_NAVX5_MIN_NUMSV;
		// pAcc mask = 50 m
		buf.cfg_nav5.mask |= BIT(4);
		buf.cfg_nav5.pAcc = 50;
		ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_NAV5, &buf, sizeof(buf),
					    sizeof(buf.cfg_nav5));
	}

	return ret;
}

UBX_RET_t UBX_CFG_NAVX5(const struct device *dev, const struct spi_config *spi_config)
{
	UBX_RET_t ret;
	ubx_payload_t buf, ackBuf;

	if ((ret = poll_and_stop_tx(dev, spi_config, UBXID_CFG_NAVX5, &buf, &ackBuf)) == UBX_OK) {
		// 2= minmax sv, 3 = minCn0, 10 = ackAid
		buf.cfg_navx5.mask1 = BIT(2) | BIT(3) | BIT(10);

		buf.cfg_navx5.ackAiding = 0x1; // Ack ANO messages
		buf.cfg_navx5.minSVs = NAV5_NAVX5_MIN_NUMSV;
		buf.cfg_navx5.minCNO = NAV5_NAVX5_MIN_CN0;
		ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_NAVX5, &buf, sizeof(buf),
					    sizeof(buf.cfg_navx5));
	}

	return ret;
}

UBX_RET_t UBX_CFG_INF(const struct device *dev, const struct spi_config *spi_config,
		      uint8_t Protocol, uint8_t mask)
{
	UBX_RET_t ret;
	ubx_payload_t buf;

	buf.cfg_inf.protocolID = Protocol; // 0=UBX Protocol, 1=NMEA Protocol
	buf.cfg_inf.infMsgMask[0] = 0x00; // A bit mask, saying which information
		// messages are enabled on DDC port
	buf.cfg_inf.infMsgMask[1] = 0x00; // A bit mask, saying which information
		// messages are enabled on Serial port 1
	buf.cfg_inf.infMsgMask[2] = 0x00; // A bit mask, saying which information
		// messages are enabled on Serial port 2
	buf.cfg_inf.infMsgMask[3] = 0x00; // A bit mask, saying which information
		// messages are enabled on USB port
	buf.cfg_inf.infMsgMask[4] = mask; // A bit mask, saying which information
		// messages are enabled on SPI port
	buf.cfg_inf.infMsgMask[5] = 0x00; // Reserved for future use

	ret = UBX_send_and_wait_ack(dev, spi_config, UBXID_CFG_INF, &buf, sizeof(buf),
				    sizeof(buf.cfg_inf));

	return ret;
}

UBX_RET_t UBX_CFG_RST(const struct device *dev, const struct spi_config *spi_config,
		      uint16_t BbrMask, uint8_t Mode)
{
	UBX_RET_t ret;
	ubx_payload_t buf;

	buf.cfg_rst.navBbrMask = BbrMask;
	buf.cfg_rst.resetMode = Mode;
	buf.cfg_rst.res = 0;

	ret = UBX_send_nowait(dev, spi_config, UBXID_CFG_RST, &buf, sizeof(buf),
			      sizeof(buf.cfg_rst));

	return ret;
}

/**
 * Sends a UBX-MGA-INI_TIME-UTC with header and checksum without waiting for
 * MGA-ACK
 * @param unixTime
 * @return
 */
UBX_RET_t UBX_MGA_INI_TIME_UTC(const struct device *dev, const struct spi_config *spi_config,
			       uint32_t unixTime)
{
	struct tm *tm;
	time_t time;

	time = unixTime;
	tm = gmtime(&time);
	UBX_RET_t ret;
	ubx_payload_t buf;
	buf.mga_ini_time_utc.type = 0x10;
	buf.mga_ini_time_utc.version = 0;
	buf.mga_ini_time_utc.ref = 0;
	buf.mga_ini_time_utc.leapSecs = -128;
	buf.mga_ini_time_utc.year = tm->tm_year;
	buf.mga_ini_time_utc.month = tm->tm_mon;
	buf.mga_ini_time_utc.day = tm->tm_mday;
	buf.mga_ini_time_utc.minute = tm->tm_min;
	buf.mga_ini_time_utc.second = tm->tm_sec;
	buf.mga_ini_time_utc.ns = 0;
	buf.mga_ini_time_utc.tAccS = 10; // ? TODO, if we got the time from server,
		// what is the smallest accuracy?
	buf.mga_ini_time_utc.tAccNs = 0;
#ifdef DEBUG_GPS
	char tmp[100];
	mini_snprintf(tmp, sizeof(tmp), "UBX_MGA_INI_TIME_UTC year %u  ux %lu",
		      buf.mga_ini_time_utc.year, unixTime);
	NF_TRACE(tmp);
#endif
	ret = UBX_send_nowait(dev, spi_config, UBXID_MGA_INI_TIME_UTC, &buf, sizeof(buf),
			      sizeof(buf.mga_ini_time_utc));
	return ret;
}

/**
 * Sends a UBX-MGA-ANO with header and checksum without waiting for MGA-ACK
 * @param pAno
 * @return
 */
UBX_RET_t UBX_MGA_ANO_RAW(const struct device *dev, const struct spi_config *spi_config,
			  UBX_MGA_ANO_RAW_t *pAno)
{
	UBX_RET_t ret;
	ubx_payload_t buf;
	buf.mga_ano = pAno->mga_ano;
	// Caller is responsible to parse the MGA-ACK from user callback, since it
	// might take several seconds
	ret = UBX_send_nowait(dev, spi_config, UBXID_MGA_ANO, &buf, sizeof(buf),
			      sizeof(buf.mga_ano));
	return ret;
}
