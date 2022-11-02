#include "Units.h"

uint8_t uart_syn_DATA_Init(ST_UART_SYN_DATA *u_data, uint8_t u_num, Fun_synCmd_format f_format, Fun_synCmd_deal f_deal)
{
	if(NULL == u_data)
		return UART_FIFO_INIT_ERR;

	if(WK_MESSAGE_UART_SYN_CMD_LIST_NUM < u_num)
		return UART_FIFO_INIT_ERR;

	u_data->index = 0;
	u_data->listNum = u_num;
	u_data->listNum_mask = u_data->listNum - 1;
	u_data->fun_synCmd_format = f_format;
	u_data->fun_synCmd_deal = f_deal;
	u_data->fun_synCmd_format(u_data->sd_list);
    for(uint8_t i=0; i<u_data->listNum; i++)
    {
    	u_data->rd_list[i].m_state = UART_RECV_NO_ERR;
    	u_data->rd_list[i].m_tick = 0;
    }

    return UART_FIFO_NO_ERR;
}

/*
 * Uart线程使用
 * 处理uart硬件返回的数据
 * 根据线程对返回数据的处理，更新接收队列中的数据信息
 */
void App_uThread_SynData_Deal(ST_UART_SYN_DATA *u_data, emUart_recv_state r_state, uint8_t *data, uint8_t len)
{
	u_data->rd_list[u_data->index].m_state = r_state;
	u_data->rd_list[u_data->index].m_tick = system_time;
	if( UART_RECV_NO_ERR== r_state )
	{
		memcpy(u_data->rd_list[u_data->index].m_data, data, len);
		u_data->rd_list[u_data->index].m_len = len;
		u_data->fun_synCmd_deal(u_data->rd_list, u_data->index);
	}
	u_data->index = (u_data->index + 1) & (u_data->listNum_mask);
}
