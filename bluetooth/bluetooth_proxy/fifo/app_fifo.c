#include <stdbool.h>
#include "app_fifo.h"

bool is_power_of_two(uint32_t n)
{
    if(n<=0)
        return false;
    return (n&(n-1))==0;
}

uint32_t fifo_length(app_fifo_t * p_fifo)
{
  uint32_t tmp = p_fifo->read_pos;
  return p_fifo->write_pos - tmp;
}

#define FIFO_LENGTH fifo_length(p_fifo)  /**< Macro for calculating the FIFO length. */


uint32_t app_fifo_init(app_fifo_t * p_fifo, uint16_t buf_size)
{
    //p_fifo->p_buf         = p_buf;
    p_fifo->buf_size_mask = buf_size - 1;
    p_fifo->read_pos      = 0;
    p_fifo->write_pos     = 0;

    return SUCCESS;
}


uint32_t app_fifo_put(app_fifo_t * p_fifo, uint8_t byte)
{
    if (FIFO_LENGTH <= p_fifo->buf_size_mask)
    {
        p_fifo->p_buf[p_fifo->write_pos & p_fifo->buf_size_mask] = byte;
        p_fifo->write_pos++;
        return SUCCESS;
    }

    return ERROR_NO_MEM;
}

uint32_t app_fifo_get(app_fifo_t * p_fifo, uint8_t * p_byte)
{
    if (FIFO_LENGTH != 0)
    {
        *p_byte = p_fifo->p_buf[p_fifo->read_pos & p_fifo->buf_size_mask];
        p_fifo->read_pos++;
        return SUCCESS;
    }

    return ERROR_NOT_FOUND;

}

uint32_t app_fifo_put_multiple(app_fifo_t * p_fifo, uint8_t* data, uint8_t len)
{
    if (FIFO_LENGTH <= p_fifo->buf_size_mask)
    {   /*
        memcpy(&(p_fifo->p_buf[p_fifo->write_pos & p_fifo->buf_size_mask]),data,len);
        //p_fifo->p_buf[p_fifo->write_pos & p_fifo->buf_size_mask] = byte;
        p_fifo->write_pos+len;
        */
        int i;
        for(i=0;i<len;i++)
            app_fifo_put(p_fifo, data[i]);
        return SUCCESS;
    }

    return ERROR_NO_MEM;
}

uint32_t app_fifo_get_multiple(app_fifo_t * p_fifo, uint8_t * data, uint8_t len)
{
    if (FIFO_LENGTH >= len)
    {   /*
        memcpy(data,&(p_fifo->p_buf[p_fifo->write_pos & p_fifo->buf_size_mask]),len);
        //*p_byte = p_fifo->p_buf[p_fifo->read_pos & p_fifo->buf_size_mask];
        p_fifo->read_pos+len;
        */
        int i;
        for(i=0;i<len;i++)
            app_fifo_get(p_fifo, &data[i]);
        return SUCCESS;
    }

    return ERROR_NOT_FOUND;

}

uint32_t app_fifo_flush(app_fifo_t * p_fifo)
{
    p_fifo->read_pos = p_fifo->write_pos;
    return SUCCESS;
}
