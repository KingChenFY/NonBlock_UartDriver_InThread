#ifndef __UART_SYN_DATA_H_
#define __UART_SYN_DATA_H_

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdint.h>
#include "UartThreadFIFO.h"

typedef struct
{
	emUart_send_state m_state;
	uint16_t m_len;
	uint16_t m_recvlen;
	uint8_t  m_data[APP_SERIAL_PORTS_THREAD_DATA_LENGTH];		//数据存放的位置
}MESSAGE_SYN_S_DTAT_LIST;

typedef struct
{
	emUart_recv_state m_state;
	uint64_t m_tick;
	uint16_t m_len;
	uint8_t  m_data[APP_SERIAL_PORTS_THREAD_DATA_LENGTH];		//数据存放的位置
}MESSAGE_SYN_R_DTAT_LIST;

typedef void (*Fun_synCmd_format)(MESSAGE_SYN_S_DTAT_LIST *s_list);
typedef uint8_t (*Fun_synCmd_deal)(MESSAGE_SYN_R_DTAT_LIST *r_list, uint8_t index);			//对校验后的同步接收数据进行解析

typedef struct
{
	uint8_t index;
	uint8_t listNum;
	uint8_t listNum_mask;
	MESSAGE_SYN_S_DTAT_LIST sd_list[WK_MESSAGE_UART_SYN_CMD_LIST_NUM];
	MESSAGE_SYN_R_DTAT_LIST rd_list[WK_MESSAGE_UART_SYN_CMD_LIST_NUM];
	Fun_synCmd_format fun_synCmd_format;
	Fun_synCmd_deal fun_synCmd_deal;
}ST_UART_SYN_DATA;


//function
uint8_t uart_syn_DATA_Init(ST_UART_SYN_DATA *u_data, uint8_t u_num, Fun_synCmd_format f_format, Fun_synCmd_deal f_deal);
void App_uThread_SynData_Deal(ST_UART_SYN_DATA *u_data, emUart_recv_state r_state, uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /*__UART_SYN_DATA_H_*/

