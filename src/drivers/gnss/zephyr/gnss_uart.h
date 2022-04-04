#ifndef GNSS_UART_H_
#define GNSS_UART_H_

#include <zephyr.h>
#include <device.h>
#include <drivers/uart.h>

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize UART communication.
 *
 * @param[in] uart_dev Device for UART communication. 
 * @param[in] rx_sem Semaphore for signalling received data. 
 * @param[in] baudrate Baudrate to use.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_uart_init(const struct device *uart_dev, 
		   struct k_sem* rx_sem, 
		   uint32_t baudrate);

/**
 * @brief Set baudrate for UART communication.
 *
 * @param[in] baudrate Baudrate to use.
 * @param[in] immediate If true, baudrate will be set immediately.
 *                      If false, baudrate will be set after next 
 *                      UART send has been completed. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_uart_set_baudrate(uint32_t baudrate, bool immediate);

/**
 * @brief Get baudrate for UART communication.
 * 
 * @return Baudrate in use. 
 */
uint32_t gnss_uart_get_baudrate(void);

/**
 * @brief Send data to UART. 
 *
 * @param[in] buffer Buffer with data to send. 
 * @param[in] cnt Number of bytes of data in buffer. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_uart_send(uint8_t* buffer, uint32_t cnt);

/**
 * @brief Get received data. Data is available from index 0 up to 
 *        cnt number of bytes. Use gnss_uart_rx_consume to delete data 
 *        that has been handled. 
 *
 * @param[out] buffer Buffer with received data. 
 * @param[out] cnt Number of bytes of data in buffer. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
void gnss_uart_rx_get_data(uint8_t** buffer, uint32_t* cnt);

/**
 * @brief Consume data from receive buffer. Data will be deleted from start 
 *        of buffer, and following data will be moved to start of buffer. 
 *
 * @param[in] cnt Number of bytes to consume. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
void gnss_uart_rx_consume(uint32_t cnt);

#endif /* GNSS_UART_H_ */