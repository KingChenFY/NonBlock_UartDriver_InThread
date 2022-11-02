#ifndef __UART_THREAD_FIFO_H_
#define __UART_THREAD_FIFO_H_

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdint.h>

#define WK_MESSAGE_UART_SYN_CMD_LIST_NUM		1					//同步指令空间开辟的指令容量条数
#define APP_SERIAL_PORTS_THREAD_DATA_LIST_NUM	8					//任务指令空间开辟的指令容量条数
#define APP_SERIAL_PORTS_THREAD_DATA_LENGTH		128					//指令发送接收的最大字节数

typedef enum
{
	UART_FIFO_NO_ERR	 		= 0,
	UART_FIFO_INIT_ERR			= 1,
	UART_FIFO_EMPTY				= 2,
	UART_FIFO_FULL				= 3,
	UART_FIFO_SAME_CMD			= 4,
	UART_FIFO_WAIT_RESP			= 5,
	UART_FIFO_RECV_OUTTIME		= 6,
	UART_FIFO_RECV_DATA_ERR		= 7,
}emUart_fifo_state;

typedef enum
{
	UART_RECV_NO_ERR	 	= 0,
	UART_RECV_OUTTIME		= 1,
	UART_RECV_DATA_ERR		= 2,
}emUart_recv_state;

typedef enum
{
	UART_SEND_NO_ERR	 	= 0,
	UART_SEND_ERR			= 1,
}emUart_send_state;

typedef struct
{
	emUart_send_state m_state;
	uint8_t  m_id;
	uint8_t  m_actn_is_deal;
	uint16_t m_len;
	uint16_t m_recvlen;									//接收数据的正确长度
	uint8_t  m_data[APP_SERIAL_PORTS_THREAD_DATA_LENGTH];		//数据存放的位置
}MESSAGE_SEND_DTAT_LIST;

typedef struct
{
	emUart_recv_state m_state;
	uint8_t  m_id;
	uint16_t m_len;
	uint8_t  m_data[APP_SERIAL_PORTS_THREAD_DATA_LENGTH];		//数据存放的位置
}MESSAGE_RECV_DTAT_LIST;

typedef struct
{
	uint8_t	read_pos;
    uint8_t	write_pos;
    uint8_t	uart_pos;
    uint8_t	actn_pos;
    uint8_t	buf_size_mask;
    MESSAGE_SEND_DTAT_LIST sd_list[APP_SERIAL_PORTS_THREAD_DATA_LIST_NUM];
	MESSAGE_RECV_DTAT_LIST rd_list[APP_SERIAL_PORTS_THREAD_DATA_LIST_NUM];
}ST_UART_THREAD_FIFO;


//function
uint8_t uart_thread_FIFO_Init(ST_UART_THREAD_FIFO *u_fifo);
emUart_fifo_state uart_thread_FIFO_IsEmpty(ST_UART_THREAD_FIFO *u_fifo);
emUart_fifo_state App_uThread_FIFO_Actn_Get(ST_UART_THREAD_FIFO *u_fifo, uint8_t *o_data, uint8_t *o_len);
emUart_fifo_state App_uThread_FIFO_Actn_Put(ST_UART_THREAD_FIFO *u_fifo, uint8_t *data, uint8_t len, uint8_t r_len);
emUart_fifo_state App_uThread_LIFO_Uart_Get(ST_UART_THREAD_FIFO *u_fifo, uint8_t *o_pos);
void App_uThread_LIFO_Uart_Put(ST_UART_THREAD_FIFO *u_fifo, emUart_recv_state r_state, uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /*__UART_THREAD_FIFO_H_*/

