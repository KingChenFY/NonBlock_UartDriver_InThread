/**
 * 起因：机械臂中多处用到了485和232串口通讯，且其所有的通讯方式都采用了阻塞的方式，即在发送完成后用delay或sleep手段等待数据接收。而机械臂的框架：所有的action在一个线程中轮询，这样在其中一个
 * 模块的串口通讯出现问题（长时间sleep）时，其它模块的响应就会受其影响而延时，在电机运行时不能及时做出判断，这样是很危险的。
 * 目的：将机械臂的串口通讯从action中分离出来，action线程只是向uart线程传递要串口发送的数据，uart线程负责发送和接收指令，并把接收的数据返给action线程进行处理。
 * UartThreadFIFO.c文件介绍：
 * 这个文件提供了一个缓存空间，它给action和uart线程提供信息传递的桥梁。action线程把数据放进这个空间，uart拿取数据放入从串口接收的数据，action线程再拿取接收的数据进行使用。
 */

#include "Units.h"

static uint16_t rand_id_seed = 0; //随机数种子

emUart_fifo_state uart_thread_FIFO_Init(ST_UART_THREAD_FIFO *u_fifo)
{
	if(NULL == u_fifo)
		return UART_FIFO_INIT_ERR;

    // Check that the buffer size is a power of two.
    if( !IS_POWER_OF_TWO(APP_SERIAL_PORTS_THREAD_DATA_LIST_NUM) )
        return UART_FIFO_INIT_ERR;

    u_fifo->buf_size_mask = APP_SERIAL_PORTS_THREAD_DATA_LIST_NUM - 1;
    u_fifo->read_pos = 0;
    u_fifo->write_pos = 0;
    u_fifo->uart_pos = 0;
    u_fifo->actn_pos = 0;
    for(uint8_t i=0; i<APP_SERIAL_PORTS_THREAD_DATA_LIST_NUM; i++)
    {
    	u_fifo->sd_list[i].m_actn_is_deal = 1;
    	u_fifo->sd_list[i].m_state = UART_SEND_NO_ERR;
    	u_fifo->rd_list[i].m_state = UART_RECV_NO_ERR;
    }

    return UART_FIFO_NO_ERR;
}

static uint8_t uart_thread_FIFO_id_Create(void)
{
	rand_id_seed = (uint8_t)((rand_id_seed + 1) & 255);
	return rand_id_seed;
}

emUart_fifo_state uart_thread_FIFO_IsEmpty(ST_UART_THREAD_FIFO *u_fifo)
{
	return ( u_fifo->read_pos == u_fifo->write_pos ) ? UART_FIFO_EMPTY : UART_FIFO_NO_ERR;
}

static emUart_fifo_state uart_thread_FIFO_IsFull(ST_UART_THREAD_FIFO *u_fifo)
{
	return ( u_fifo->read_pos == ((u_fifo->write_pos + 1) & u_fifo->buf_size_mask) ) ? UART_FIFO_FULL : UART_FIFO_NO_ERR;
}

/*
 * Atcn线程使用
 * 判断Actn请求的指令在uart线程中处理的状态，返回处理状态
 * 若处理结果无异常，通过o_data和o_len返回请求的指令的处理数据
 */
emUart_fifo_state App_uThread_FIFO_Actn_Get(ST_UART_THREAD_FIFO *u_fifo, uint8_t *o_data, uint8_t *o_len)
{
	if( u_fifo->rd_list[u_fifo->actn_pos].m_id !=  u_fifo->sd_list[u_fifo->actn_pos].m_id )
		return UART_FIFO_WAIT_RESP;

	u_fifo->sd_list[u_fifo->actn_pos].m_actn_is_deal = 1;

	if( UART_RECV_NO_ERR == u_fifo->rd_list[u_fifo->actn_pos].m_state )
	{
//		o_data = u_fifo->rd_list[u_fifo->actn_pos].m_data;
		memcpy(o_data, u_fifo->rd_list[u_fifo->actn_pos].m_data, u_fifo->rd_list[u_fifo->actn_pos].m_len);
		*o_len = u_fifo->rd_list[u_fifo->actn_pos].m_len;
		return UART_FIFO_NO_ERR;
	}
	else if( UART_RECV_OUTTIME == u_fifo->rd_list[u_fifo->actn_pos].m_state )
	{
		return UART_FIFO_RECV_OUTTIME;
	}
	else
	{
		return UART_FIFO_RECV_DATA_ERR;
	}
}

/*
 * Atcn线程使用
 * 判断Actn请求的指令是否已经在发送队列之中
 * 若不在队列中或队列中相同的指令已经处理完毕，则将请求指令加入发送队列
 * r_len-预测接收数据的长度
 */
emUart_fifo_state App_uThread_FIFO_Actn_Put(ST_UART_THREAD_FIFO *u_fifo, uint8_t *data, uint8_t len, uint8_t r_len)
{
	if( 0 == memcmp(&u_fifo->sd_list[u_fifo->actn_pos].m_data, data, len) && (0 == u_fifo->sd_list[u_fifo->actn_pos].m_actn_is_deal) )
	{
		return UART_FIFO_SAME_CMD;	//指令相同，且当前指令还未收到回复
	}

    if( UART_FIFO_NO_ERR == uart_thread_FIFO_IsFull(u_fifo) )
    {
    	u_fifo->sd_list[u_fifo->write_pos].m_actn_is_deal = 0;
		u_fifo->sd_list[u_fifo->write_pos].m_id = uart_thread_FIFO_id_Create();
		u_fifo->sd_list[u_fifo->write_pos].m_len = len;
		u_fifo->sd_list[u_fifo->write_pos].m_recvlen = r_len;
		memcpy(&u_fifo->sd_list[u_fifo->write_pos].m_data, data, len);


		u_fifo->actn_pos = u_fifo->write_pos;
    	u_fifo->write_pos = (u_fifo->write_pos + 1) & u_fifo->buf_size_mask;
        return UART_FIFO_NO_ERR;
    }

    return UART_FIFO_FULL;
}

/*
 * Uart线程使用
 * 判断发送队列中有无待发送的数据
 * 若有数据，则选取最新放入的数据发送，通过o_pos返回最新待发数据在发送队列中的位置
 */
emUart_fifo_state App_uThread_LIFO_Uart_Get(ST_UART_THREAD_FIFO *u_fifo, uint8_t *o_pos)
{
    if( UART_FIFO_NO_ERR == uart_thread_FIFO_IsEmpty(u_fifo) )
    {
    	u_fifo->read_pos = (u_fifo->write_pos - 1) & u_fifo->buf_size_mask;
    	u_fifo->uart_pos = u_fifo->read_pos;
    	*o_pos = u_fifo->uart_pos;
    	u_fifo->read_pos = (u_fifo->read_pos + 1) & u_fifo->buf_size_mask;
        return UART_FIFO_NO_ERR;
    }

    return UART_FIFO_EMPTY;
}

/*
 * Uart线程使用
 * 处理uart硬件返回的数据
 * 根据线程对返回数据的处理，更新接收队列中的数据信息
 */
void App_uThread_LIFO_Uart_Put(ST_UART_THREAD_FIFO *u_fifo, emUart_recv_state r_state, uint8_t *data, uint8_t len)
{
	if( UART_RECV_NO_ERR== r_state )
	{
		memset(u_fifo->rd_list[u_fifo->uart_pos].m_data, 0, len);
		memcpy(u_fifo->rd_list[u_fifo->uart_pos].m_data, data, len);
		u_fifo->rd_list[u_fifo->uart_pos].m_len = len;
	}
	u_fifo->rd_list[u_fifo->uart_pos].m_state = r_state;
	u_fifo->rd_list[u_fifo->uart_pos].m_id = u_fifo->sd_list[u_fifo->uart_pos].m_id;
}
