#ifndef UART_MOCK_H_
#define UART_MOCK_H_

#include <zephyr.h>
#include <device.h>

/**
 * @brief Register semaphore to indicate new waiting data in receive buffer. 
 *
 * @param[in] dev Device pointer
 * @param[in] rx_sem Semaphore to signal new data
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mock_uart_register_sem(const struct device *dev, struct k_sem* rx_sem);

/**
 * @brief Send data to MCU
 *
 * @param[in] dev Device pointer
 * @param[in] data Data buffer to send
 * @param[in] size Size of data in buffer
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mock_uart_send(const struct device *dev, uint8_t* data, uint32_t size);

/**
 * @brief Receive data from MCU
 *
 * @param[in] dev Device pointer
 * @param[out] data Data buffer to copy data into
 * @param[out] size Size of copied data
 * @param[in] max_size Number of bytes that can fit in data buffer
 * @param[in] consume Delete data from buffer after copy
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mock_uart_receive(const struct device *dev, 
		      uint8_t* data, 
		      uint32_t* size, 
		      uint32_t max_size,
		      bool consume);
/**
 * @brief Delete data from buffer
 *
 * @param[in] dev Device pointer
 * @param[int] cnt Number of bytes to delete
 * 
 * @return 0 if everything was ok, error code otherwise
 */
int mock_uart_consume(const struct device *dev, uint32_t cnt);

#endif /* UART_MOCK_H_ */