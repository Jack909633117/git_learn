/**@file
 *
 * @defgroup app_fifo FIFO implementation
 * @{
 * @ingroup app_common
 *
 * @brief FIFO implementation.
 */

#ifndef APP_FIFO_H__
#define APP_FIFO_H__

#include <stdint.h>
#include <stdlib.h>

#define FIFO_BUF_LEN 1024

enum {
    SUCCESS,
    ERROR_NULL,
    ERROR_INVALID_LENGTH,
    ERROR_NO_MEM,
    ERROR_NOT_FOUND
};

/**@brief A FIFO instance structure. Keeps track of which bytes to read and write next.
 *        Also it keeps the information about which memory is allocated for the buffer
 *        and its size. This needs to be initialized by app_fifo_init() before use.
 */
typedef struct
{
    uint8_t            p_buf[FIFO_BUF_LEN];           /**< Pointer to FIFO buffer memory.                      */
    uint16_t           buf_size_mask;   /**< Read/write index mask. Also used for size checking. */
    volatile uint32_t  read_pos;        /**< Next read position in the FIFO buffer.              */
    volatile uint32_t  write_pos;       /**< Next write position in the FIFO buffer.             */
} app_fifo_t;

uint32_t fifo_length(app_fifo_t * p_fifo);

/**@brief Function for initializing the FIFO.
 *
 * @param[out] p_fifo   FIFO object.
 * @param[in]  p_buf    FIFO buffer for storing data. The buffer size has to be a power of two.
 * @param[in]  buf_size Size of the FIFO buffer provided, has to be a power of 2.
 *
 * @retval     SUCCESS              If initialization was successful.
 * @retval     ERROR_NULL           If a NULL pointer is provided as buffer.
 * @retval     ERROR_INVALID_LENGTH If size of buffer provided is not a power of two.
 */
//uint32_t app_fifo_init(app_fifo_t * p_fifo, uint8_t * p_buf, uint16_t buf_size);
uint32_t app_fifo_init(app_fifo_t * p_fifo, uint16_t buf_size);

/**@brief Function for adding an element to the FIFO.
 *
 * @param[in]  p_fifo   Pointer to the FIFO.
 * @param[in]  byte     Data byte to add to the FIFO.
 *
 * @retval     SUCCESS              If an element has been successfully added to the FIFO.
 * @retval     ERROR_NO_MEM         If the FIFO is full.
 */
uint32_t app_fifo_put(app_fifo_t * p_fifo, uint8_t byte);

/**@brief Function for getting the next element from the FIFO.
 *
 * @param[in]  p_fifo   Pointer to the FIFO.
 * @param[out] p_byte   Byte fetched from the FIFO.
 *
 * @retval     SUCCESS              If an element was returned.
 * @retval     ERROR_NOT_FOUND      If there is no more elements in the queue.
 */
uint32_t app_fifo_get(app_fifo_t * p_fifo, uint8_t * p_byte);

/**@brief Function for adding multiple element to the FIFO.
 *
 * @param[in]  p_fifo   Pointer to the FIFO.
 * @param[in]  data     Data byte to add to the FIFO.
 * @param[in]  len      data len.
 * @retval     SUCCESS              If an element has been successfully added to the FIFO.
 * @retval     ERROR_NO_MEM         If the FIFO is full.

 */
uint32_t app_fifo_put_multiple(app_fifo_t * p_fifo, uint8_t* data, uint8_t len);

/**@brief Function for getting the multiple element from the FIFO.

 *
 * @param[in]  p_fifo   Pointer to the FIFO.
 * @param[out] data     data fetched from the FIFO.
 * @param[in]  len      data len.
 * @retval     SUCCESS              If an element was returned.
 * @retval     ERROR_NOT_FOUND      If there is no more elements in the queue.
 */
uint32_t app_fifo_get_multiple(app_fifo_t * p_fifo, uint8_t * data, uint8_t len);

/**@brief Function for flushing the FIFO.
 *
 * @param[in]  p_fifo   Pointer to the FIFO.
 *
 * @retval     SUCCESS              If the FIFO flushed successfully.
 */
uint32_t app_fifo_flush(app_fifo_t * p_fifo);

#endif // APP_FIFO_H__

/** @} */
