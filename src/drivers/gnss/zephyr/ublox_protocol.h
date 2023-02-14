
#ifndef UBLOX_PROTOCOL_H_
#define UBLOX_PROTOCOL_H_

#include "ublox_types.h"
#include "ubx_cfg_ids.h"
#include "ubx_ids.h"

#include <stdint.h>
#include <stdbool.h>

/* Constants for U-blox message sync characters */
#define UBLOX_SYNC_CHAR_1 0xB5
#define UBLOX_SYNC_CHAR_2 0x62

/**
 * @brief Initializes protocol parser. 
 *
 */
void ublox_protocol_init(void);

/**
 * @brief Register handler for periodic message with specified class/id. 
 *
 * @param[in] msg_class Class of message. 
 * @param[in] msg_id ID of message.
 * @param[in] handle Function to call to handle the message.
 * @param[in] context Context to use when calling handler function. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_register_handler(uint8_t msg_class, uint8_t msg_id,
			   int (*handle)(void *, void *, uint32_t), void *context);

/**
 * @brief Function for parsing U-blox protocol data. 
 *
 * @param[in] data Data to parse. 
 * @param[in] size Size of data. 
 * 
 * @return Number of bytes parsed, which can be freed. 
 */
uint32_t ublox_parse(uint8_t *data, uint32_t size);

/**
 * @brief Resets stored response handlers for previous command.
 *        This is also done automatically when response has been handled. 
 *
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_reset_response_handlers(void);

/**
 * @brief Sets callback handlers for ack and poll responses for 
 *        provided message buffer.
 *
 * @param[in] buffer Buffer holding message expecting reponse. 
 * @param[in] poll_cb Callback for poll/data response. 
 * @param[in] ack_cb Callback for ack/nak response. 
 * @param[in] context Context to use when calling handler functions. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_set_response_handlers(uint8_t *buffer,
				int (*poll_cb)(void *, uint8_t, uint8_t, void *, uint32_t),
				int (*ack_cb)(void *, uint8_t, uint8_t, bool), void *context);

/**
 * @brief Gets data of response payload for configuration get. 
 *
 * @param[in] payload Payload poll response for configuration get. 
 * @param[in] size Size of payload. 
 * @param[in] val_size Size of data value in payload. 
 * @param[out] val Pointer to value object to fill with data.
 *                 This will get the raw bytes of the value.
 *                 Floating point will have to be converted. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_get_cfg_val(uint8_t *payload, uint32_t size, uint8_t val_size, uint64_t *val);

/**
 * @brief Gets data of response payload for ubx mon ver. 
 *
 * @param[in] payload . 
 * @param[in] size Size of payload. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_get_mon_ver(uint8_t *payload, uint32_t size, struct ublox_mon_ver *pmia_m10_versions);

/**
 * @brief Builds command for mon-ver
 *
 * @param[out] buffer Buffer to build command in. 
 * @param[out] size Size of built command. 
 * @param[in] max_size Maximum size of command. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_build_mon_ver(uint8_t *buffer, uint32_t *size, uint32_t max_size);

/**
 * @brief Gets uint8_t data of response payload for configuration get. 
 *
 * @param[in] payload Payload poll response for configuration get. 
 * @param[in] size Size of payload. 
 * 
 * @return Configuration value. 
 */
inline uint8_t ublox_get_cfg_val_u8(uint8_t *payload, uint32_t size)
{
	uint64_t val;
	ublox_get_cfg_val(payload, size, 1, &val);
	return val & 0xFF;
}

/**
 * @brief Gets uint16_t data of response payload for configuration get. 
 *
 * @param[in] payload Payload poll response for configuration get. 
 * @param[in] size Size of payload. 
 * 
 * @return Configuration value. 
 */
inline uint16_t ublox_get_cfg_val_u16(uint8_t *payload, uint32_t size)
{
	uint64_t val;
	ublox_get_cfg_val(payload, size, 2, &val);
	return val & 0xFFFF;
}

/**
 * @brief Gets uint32_t data of response payload for configuration get. 
 *
 * @param[in] payload Payload poll response for configuration get. 
 * @param[in] size Size of payload. 
 * 
 * @return Configuration value. 
 */
inline uint32_t ublox_get_cfg_val_u32(uint8_t *payload, uint32_t size)
{
	uint64_t val;
	ublox_get_cfg_val(payload, size, 4, &val);
	return val & 0xFFFFFFFF;
}

/**
 * @brief Gets uint64_t data of response payload for configuration get. 
 *
 * @param[in] payload Payload poll response for configuration get. 
 * @param[in] size Size of payload. 
 * 
 * @return Configuration value. 
 */
inline uint64_t ublox_get_cfg_val_u64(uint8_t *payload, uint32_t size)
{
	uint64_t val;
	ublox_get_cfg_val(payload, size, 8, &val);
	return val;
}

/**
 * @brief Gets float data of response payload for configuration get. 
 *
 * @param[in] payload Payload poll response for configuration get. 
 * @param[in] size Size of payload. 
 * 
 * @return Configuration value. 
 */
inline float ublox_get_cfg_val_f32(uint8_t *payload, uint32_t size)
{
	uint64_t val;
	ublox_get_cfg_val(payload, size, 4, &val);
	float *result = (void *)&val;
	return *result;
}

/**
 * @brief Gets double data of response payload for configuration get. 
 *
 * @param[in] payload Payload poll response for configuration get. 
 * @param[in] size Size of payload. 
 * 
 * @return Configuration value. 
 */
inline double ublox_get_cfg_val_f64(uint8_t *payload, uint32_t size)
{
	uint64_t val;
	ublox_get_cfg_val(payload, size, 8, &val);
	double *result = (void *)&val;
	return *result;
}

/** 
 * Signed definitions of cfg value get
 */
#define ublox_get_cfg_val_i8(p, s) ((int8_t)ublox_get_cfg_val_u8(p, s))
#define ublox_get_cfg_val_i16(p, s) ((int16_t)ublox_get_cfg_val_u16(p, s))
#define ublox_get_cfg_val_i32(p, s) ((int32_t)ublox_get_cfg_val_u32(p, s))
#define ublox_get_cfg_val_i64(p, s) ((int64_t)ublox_get_cfg_val_u64(p, s))

/**
 * @brief Builds command for cfg-valget
 *
 * @param[out] buffer Buffer to build command in. 
 * @param[out] size Size of built command. 
 * @param[in] max_size Maximum size of command. 
 * @param[in] layer Layer to retrieve from; RAM, BBR, flash. 
 * @param[in] position Positions to skip, set to 0. 
 * @param[in] key Configuration key to get. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_build_cfg_valget(uint8_t *buffer, uint32_t *size, uint32_t max_size,
			   enum ublox_cfg_val_layer layer, uint16_t position, uint32_t key);

/**
 * @brief Builds command for nav-status
 *
 * @param[out] buffer Buffer to build command in. 
 * @param[out] size Size of built command. 
 * @param[in] max_size Maximum size of command. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_build_nav_status(uint8_t *buffer, uint32_t *size, uint32_t max_size);

/**
 * @brief Builds command for cfg-valset
 *
 * @param[out] buffer Buffer to build command in. 
 * @param[out] size Size of built command. 
 * @param[in] max_size Maximum size of command. 
 * @param[in] layer Layer to set in; RAM, BBR, flash. 
 * @param[in] key Configuration key to set. 
 * @param[in] value Configuration value. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_build_cfg_valset(uint8_t *buffer, uint32_t *size, uint32_t max_size,
			   enum ublox_cfg_val_layer layer, uint32_t key, uint64_t value);

/**
 * @brief Builds command for cfg-rst
 *
 * @param[out] buffer Buffer to build command in. 
 * @param[out] size Size of built command. 
 * @param[in] max_size Maximum size of command. 
 * @param[in] mask Mask of what to reset. 
 * @param[in] mode Mode of reset, e.g. hardware or software. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_build_cfg_rst(uint8_t *buffer, uint32_t *size, uint32_t max_size, uint16_t mask,
			uint8_t mode);

/**
 * @brief Builds command for mga-ano, assistance data
 *
 * @param[out] buffer Buffer to build command in. 
 * @param[out] size Size of built command. 
 * @param[in] max_size Maximum size of command. 
 * @param[in] data Data blob to upload for assistance data. 
 * @param[in] data_size Number of bytes in data. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int ublox_build_mga_ano(uint8_t *buffer, uint32_t *size, uint32_t max_size, uint8_t *data,
			uint32_t data_size);

/**
 * @brief Puts the receiver into backup-mode, waken up by interrupt
 * @param[out] buffer Buffer to build complete command in
 * @param[out] size Size of built command
 * @param[in] max_size of buffer
 * @return 0 if OK, error code otherwise
 */
int ublox_build_rxm_pmreq(uint8_t *buffer, uint32_t *size, uint32_t max_size);
#endif /* UBLOX_PROTOCOL_H_ */
