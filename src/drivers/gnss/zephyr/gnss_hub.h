#ifndef GNSS_HUB_H_
#define GNSS_HUB_H_

#include <zephyr.h>
#include <device.h>
#include <drivers/uart.h>

#include <stdint.h>
#include <stdbool.h>

#define GNSS_HUB_ID_UART		0
#define GNSS_HUB_ID_DRIVER		1
#define GNSS_HUB_ID_DIAGNOSTICS		2

#define GNSS_HUB_MODE_DEFAULT		0
#define GNSS_HUB_MODE_SNIFFER		1
#define GNSS_HUB_MODE_CONTROLLER	2
#define GNSS_HUB_MODE_SIMULATOR		3

typedef void (*gnss_diag_data_cb_t)(void);

/**
 * @brief Initialize hub for GNSS communication
 *
 * @param[in] uart_dev Device for UART communication. 
 * @param[in] rx_sem Semaphore for signalling received data. 
 * @param[in] baudrate Baudrate to use.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_hub_init(const struct device *uart_dev, 
		  struct k_sem* rx_sem, 
		  uint32_t baudrate);

/**
 * @brief Set callback to signal received data for diagnostics. 
 *
 * @param[in] cb Callback to notify diagnostics
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_hub_set_diagnostics_callback(gnss_diag_data_cb_t cb);

/**
 * @brief Configure GNSS hub by setting mode, controls where data flows.
 *
 * @param[in] mode Mode of operation. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_hub_configure(uint8_t mode);

/**
 * @brief Sets baudrate for UART interface
 *
 * @param[in] baudrate New baudrate to set. 
 * @param[in] immediate Set to true to immediately change baudrate, otherwise
 *                      will wait for next command to be sent. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_hub_set_uart_baudrate(uint32_t baudrate, bool immediate);

/**
 * @brief Get currently set UART baudrate
 * 
 * @return Baudrate of UART. 
 */
uint32_t gnss_hub_get_uart_baudrate(void);

/**
 * @brief Send data through GNSS hub, mode will decide where data is received. 
 *
 * @param[in] hub_id ID of sender.
 * @param[in] buffer Buffer holding data.
 * @param[in] cnt Number of bytes being sent
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_hub_send(uint8_t hub_id, uint8_t* buffer, uint32_t cnt);

/**
 * @brief Check if RX buffer is empty for specified receiver ID
 *
 * @param[in] hub_id ID of receiver.
 * 
 * @return True when empty, false if data is available. 
 */
bool gnss_hub_rx_is_empty(uint8_t hub_id);

/**
 * @brief Get data buffers for specified receiver ID. 
 *
 * @param[in] hub_id ID of receiver.
 * @param[out] buffer Pointer to buffer holding data. 
 * @param[out] cnt Number of bytes in buffer. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_hub_rx_get_data(uint8_t hub_id, uint8_t** buffer, uint32_t* cnt);

/**
 * @brief Remove specified number of bytes from receive buffer. 
 *
 * @param[in] hub_id ID of receiver.
 * @param[in] cnt Number of bytes to remove. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_hub_rx_consume(uint8_t hub_id, uint32_t cnt);

/**
 * @brief Clears all buffers of the GNSS hub. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_hub_flush_all(void);

#endif /* GNSS_HUB_H_ */