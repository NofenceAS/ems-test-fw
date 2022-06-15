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
 * @brief Trigger UART transmitting. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int gnss_uart_start_send(void);

/**
 * @brief Block/unblock UART interrupts and scheduler.
 *
 * @param[in] block Block if true, unblock if false.
 */
void gnss_uart_block(bool block);

#endif /* GNSS_UART_H_ */