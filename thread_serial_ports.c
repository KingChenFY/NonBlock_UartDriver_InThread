#include "Units.h"

APP_PORT_HANDLE pHandle[THREAD_PORT_END];

typedef enum
{
	PORTX_DONT_SEND = 0,
	PORTX_ALLOW_SEND = 1,
}emPorxIsAllowSend;

static const ST_PORTX_PARAM_INIT stPortx_Func[THREAD_PORT_END] =
{
	{THREAD_PORT_START, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {THREAD_PORT_5_INDEX, &huart5, TYPE_FULL_DUPLEX, QRSCAN_READ_TIMEOUT, QRSCAN_SYN_INTERVAL, QRSCAN_RX_WAIT_MAX, QRSCAN_SYN_LIST_NUM, QRScan_SynCmd_Format, QRScan_SynCmd_Deal, QRScan_Cmd_Verify},
	{THREAD_PORT_7_INDEX, &huart7, TYPE_HALF_DUPLEX, CLAW_READ_TIMEOUT, CLAW_SYN_INTERVAL, CLAW_RX_WAIT_MAX, CLAW_SYN_LIST_NUM, Claw_SynCmd_Format, Claw_SynCmd_Deal, Claw_Cmd_Verify},
};

static emUart_send_state app_SerialPorts_send_process(APP_PORT_HANDLE *phandle);
static emUart_recv_state app_SerialPorts_recv_process(APP_PORT_HANDLE *phandle);
static emUart_send_state app_SerialPorts_Transmit_DMA(APP_PORT_HANDLE *phandle, emPortxCmdType cmd_type);
static emUart_send_state app_SerialPorts_SynCmd_SendDMA(APP_PORT_HANDLE *phandle);
static emUart_send_state app_SerialPorts_ActnCmd_SendDMA(APP_PORT_HANDLE *phandle);
static uint8_t  app_SerialPorts_RecvCache_Clear(APP_PORT_HANDLE *phandle);
static emPorxIsAllowSend app_SerialPorts_BeforeSend_deal(APP_PORT_HANDLE *phandle);

/*
 * 指令发送查询函数
 * data-发送的数据位置
 * dlen-发送的数据长度
 * rlen-接收数据的最小长度
 */
emUart_fifo_state app_portx_sendANDrecv(uint8_t *data, uint8_t dlen, uint8_t rlen, ST_UART_THREAD_FIFO *fifo, MESSAGE_RECV_DATA_TO_ANALYSE *rdata)
{
	emUart_fifo_state rstate;

    if(UART_FIFO_FULL == App_uThread_FIFO_Actn_Put(fifo, data, dlen, rlen))
    	return UART_FIFO_FULL;

    rstate = App_uThread_FIFO_Actn_Get(fifo, rdata->m_data, &rdata->m_len);

    return rstate;
}

/*
 * 要在串口初始化中打开相应的中断
 * UART_FLAG_IDLE-空闲中断，DMA接收数据处理函数
 */
void app_portx_IT_deal(APP_PORT_HANDLE *phandle)
{
	//空闲中断
    if((__HAL_UART_GET_FLAG(phandle->huart, UART_FLAG_IDLE) != RESET))
    {
        __HAL_UART_CLEAR_IDLEFLAG(phandle->huart);
        phandle->g_uartRxSize = COM_MIN_FIFO_DATA_LENGHT - __HAL_DMA_GET_COUNTER(phandle->huart->hdmarx);
		
//		if(&huart5 == phandle->huart)
//		{
//			__Portx_DEBUG(0, "[app_portx_IT_IdleDeal] uart5 idle,dma_size=%d\r\n", phandle->g_uartRxSize);
//		}
		if(phandle->g_uartRxSize > 0)
		{
			if(phandle->g_uartRxSize > APP_SERIAL_PORTS_THREAD_DATA_LENGTH)
			{
				phandle->g_uartRxSize = APP_SERIAL_PORTS_THREAD_DATA_LENGTH;
				__Portx_DEBUG(0, "[app_portx_IT_IdleDeal] g_uartRxSize > 128\r\n");
			}
			phandle->g_uartIsIdle = PORTx_IDLE;
			__Portx_DEBUG(0, "[app_portx_IT_IdleDeal] PORTx_IDLE, %lld\r\n", system_time);
		}
    }
	
	if(TYPE_HALF_DUPLEX == phandle->g_duplexType)
	{
		//发送完成中断，开启接收
		if((__HAL_UART_GET_FLAG(phandle->huart, UART_FLAG_TC) != RESET))
		{
			__HAL_UART_CLEAR_FLAG(phandle->huart, (UART_CLEAR_TCF | UART_CLEAR_IDLEF));
			HAL_UART_AbortReceive(phandle->huart);
			HAL_UART_Receive_DMA(phandle->huart, phandle->s_comData, COM_MIN_FIFO_DATA_LENGHT);
		}
	}
}

/*
 * 查询设备是否在线
 */
uint8_t app_portx_Is_OnLine(emUartIndex index)
{
	return ( (PORTx_Connect == pHandle[index].g_isConnect) ? WK_SECCESS : WK_FAILED );
}

/*
 * 设置是否开启自动硬件同步功能
 */
void app_portx_FucSynHard_set(emUartIndex index, emPorxFucOnOff state)
{
	pHandle[index].stFucOnOff.e_SynHard = state;
}

/*
 * 设置同步参数
 */
void app_portx_SynParam_set(emUartIndex index, uint16_t interval)
{
	pHandle[index].stComParam.synWaitCNT = 0;
	pHandle[index].stComParam.synInterval = interval;
}

/*
 * 端口号注册初始化函数
 */
void app_PortInfo_register(emUartIndex index)
{
	if((index <= THREAD_PORT_START) || (index >= THREAD_PORT_END))
		return;
	if(stPortx_Func[index].m_uPortxId != index)
		return;
	//配置参数
	pHandle[index].portIndex = stPortx_Func[index].m_uPortxId;								//串口ID
	pHandle[index].huart = stPortx_Func[index].huart;										//串口句柄
	pHandle[index].g_duplexType = stPortx_Func[index].m_duplexType;							//串口模式
	pHandle[index].stComParam.busyMax = stPortx_Func[index].m_busyMax;						//超时等待
	pHandle[index].stComParam.rxDataWaitMax = stPortx_Func[index].m_rxDataWaitMax;			//连接等待
	pHandle[index].stComParam.synInterval = stPortx_Func[index].m_synInterval;				//同步间隔
	pHandle[index].fun_UartRe_Verify = stPortx_Func[index].m_FunUartCmd_Verify;				//接收校验
	//初始化参数
	pHandle[index].stComParam.busyCnt = 0;
	pHandle[index].stComParam.rxDataWaitCNT = 0;
	pHandle[index].stComParam.synWaitCNT = 0;
	pHandle[index].g_isConnect = PORTx_Disconnect;
	pHandle[index].g_lastCmdLost = 1;
	pHandle[index].g_isReadEn = 1;
	pHandle[index].g_isCmdBeSend = 0;
	pHandle[index].g_isNeedRecvClear = 0;
	pHandle[index].g_uartIsIdle = PORTx_IDLE;
	pHandle[index].g_cmdType = TYPE_SYN;
	//初始化函数调用
	uart_thread_FIFO_Init(&pHandle[index].ActnFifo);
	uart_syn_DATA_Init(&pHandle[index].SynFifo, stPortx_Func[index].m_synListNum, stPortx_Func[index].m_FunSynCmd_Format, stPortx_Func[index].m_FunSynCmd_Deal);
	app_portx_FucSynHard_set(index, FUNC_EN);		//打开同步
}

VOID  App_SerialPorts_Thread (ULONG thread_input)
{
	(void)thread_input;
	emUart_send_state app_s_status;
	emUart_recv_state app_r_status;
	
	while (1)
	{
		for(uint8_t i=(THREAD_PORT_START+1); i<THREAD_PORT_END; i++)
		{
#if ((defined BOARD1_GRIP_CTRL)&&(BOARD1_GRIP_CTRL == 1))
			if(THREAD_PORT_7_INDEX == i)
				continue;
#endif
			if(PORTx_IDLE == pHandle[i].g_uartIsIdle)
			{
				app_r_status = app_SerialPorts_recv_process(&pHandle[i]);
				if(app_r_status != UART_RECV_NO_ERR)
					__Portx_DEBUG(0, "[App_SerialPorts_Thread] app_SerialPorts_recv_process[%d]=%d\r\n", i, app_r_status);
				app_s_status = app_SerialPorts_send_process(&pHandle[i]);
				if(app_s_status != UART_SEND_NO_ERR)
					__Portx_DEBUG(0, "[App_SerialPorts_Thread] app_SerialPorts_send_process[%d]=%d\r\n", i, app_s_status);
				continue;
			}
			if(PORTx_BUSY == pHandle[i].g_uartIsIdle)
			{
				pHandle[i].stComParam.busyCnt++;
				if(pHandle[i].stComParam.busyCnt > pHandle[i].stComParam.busyMax)
				{
					pHandle[i].g_lastCmdLost = 1;
					pHandle[i].g_uartIsIdle = PORTx_IDLE;		//允许再次发送
					__Portx_DEBUG(0, "[App_SerialPorts_Thread] pHandle[i].g_lastCmdLost = 1, PORTx_IDLE\r\n");
				}
				continue;
			}
			__Portx_DEBUG(0, "[App_SerialPorts_Thread] pHandle[%d].g_uartIsIdle=%d, impossible\r\n", i, pHandle[i].g_uartIsIdle);
		}
		tx_thread_sleep(1);
	}
}

static emUart_send_state app_SerialPorts_send_process(APP_PORT_HANDLE *phandle)
{
	emUart_send_state sd_status = UART_SEND_NO_ERR;
	
	if(PORTX_DONT_SEND == app_SerialPorts_BeforeSend_deal(phandle))
	{
		return sd_status;
	}

	if(PORTx_Disconnect == phandle->g_isConnect)
	{
		sd_status = app_SerialPorts_SynCmd_SendDMA(phandle);
	}
	else
	{
		if(UART_FIFO_EMPTY != uart_thread_FIFO_IsEmpty(&phandle->ActnFifo))
		{
			sd_status = app_SerialPorts_ActnCmd_SendDMA(phandle);
		}
		else
		{
			if(FUNC_EN == phandle->stFucOnOff.e_SynHard)
			{
				sd_status = app_SerialPorts_SynCmd_SendDMA(phandle);
			}
			else if(FUNC_DISABLE == phandle->stFucOnOff.e_SynHard)
			{
			}
		}
	}
	return sd_status;
}

static emUart_recv_state app_SerialPorts_recv_process(APP_PORT_HANDLE *phandle)
{
	emUart_recv_state rd_status = UART_RECV_NO_ERR;
	uint8_t offset, e_len;

	if(0 == phandle->g_isCmdBeSend)					//没有发送过指令，不需要处理接收数据
	{
		return rd_status;
	}

	if(1 == phandle->g_lastCmdLost)					//超时
	{
		rd_status = UART_RECV_OUTTIME;
		phandle->g_lastCmdLost = 0;
		if(TYPE_SYN == phandle->g_cmdType)			//接收数据处理，超时处理
		{
			App_uThread_SynData_Deal(&phandle->SynFifo, rd_status, NULL, 0);
		}
		else if(TYPE_ACTION == phandle->g_cmdType)
		{
			App_uThread_LIFO_Uart_Put(&phandle->ActnFifo, rd_status, NULL, 0);
		}
		else
		{
			__Portx_DEBUG(0, "[app_SerialPorts_recv_process-1] type=%d, impossible\r\n", phandle->g_cmdType);
		}
	}
	else
	{
		if(TYPE_SYN == phandle->g_cmdType)			//接收数据处理
		{
			rd_status = phandle->fun_UartRe_Verify(phandle->g_uartRxSize, phandle->SynFifo.sd_list[phandle->SynFifo.index].m_recvlen, phandle->s_comData, &offset, &e_len); //未超时，数据校验
			App_uThread_SynData_Deal(&phandle->SynFifo, rd_status, &phandle->s_comData[offset], e_len);
			__Portx_DEBUG(0, "[app_SerialPorts_recv_process] TYPE_SYN, rd_status=%d\r\n", rd_status);
		}
		else if(TYPE_ACTION == phandle->g_cmdType)
		{
			rd_status = phandle->fun_UartRe_Verify(phandle->g_uartRxSize, phandle->ActnFifo.sd_list[phandle->ActnFifo.uart_pos].m_recvlen, phandle->s_comData, &offset, &e_len); //未超时，数据校验
			App_uThread_LIFO_Uart_Put(&phandle->ActnFifo, rd_status, &phandle->s_comData[offset], e_len);
			__Portx_DEBUG(0, "[app_SerialPorts_recv_process] TYPE_ACTION, rd_status=%d\r\n", rd_status);
		}
		else
		{
			__Portx_DEBUG(0, "[app_SerialPorts_recv_process-2] type=%d, impossible\r\n", phandle->g_cmdType);
		}
	}

	phandle->g_isCmdBeSend = 0;				//发送指令的接收处理完成，清标志位
	phandle->g_isConnect = (UART_RECV_OUTTIME == rd_status) ? PORTx_Disconnect : PORTx_Connect;	//如果超时未接收到数据，置为无连接
	return rd_status;
}

static emUart_send_state app_SerialPorts_Transmit_DMA(APP_PORT_HANDLE *phandle, emPortxCmdType cmd_type)
{
	uint8_t sdIndex;
	uint8_t sd_status;

	phandle->stComParam.busyCnt = 0;
	phandle->g_uartIsIdle = PORTx_BUSY;
	phandle->g_isCmdBeSend = 1;

	if(TYPE_FULL_DUPLEX == phandle->g_duplexType)
	{
		HAL_UART_AbortReceive(phandle->huart);
		memset(phandle->s_comData, 0, APP_SERIAL_PORTS_THREAD_DATA_LENGTH);
		HAL_UART_Receive_DMA(phandle->huart, phandle->s_comData, COM_MIN_FIFO_DATA_LENGHT);
	}
	HAL_UART_AbortTransmit(phandle->huart);

	if(TYPE_SYN == cmd_type)
	{
		phandle->g_cmdType = TYPE_SYN;
		__Portx_DEBUG(0, "[app_SerialPorts_Transmit_DMA] TYPE_SYN, PORTx_BUSY, %lld\r\n", system_time);
		sdIndex = phandle->SynFifo.index;
		sd_status = HAL_UART_Transmit_DMA(phandle->huart, phandle->SynFifo.sd_list[sdIndex].m_data, phandle->SynFifo.sd_list[sdIndex].m_len);
		if(HAL_OK != sd_status)
			phandle->SynFifo.sd_list[sdIndex].m_state = UART_SEND_ERR;
		else
			phandle->SynFifo.sd_list[sdIndex].m_state = UART_SEND_NO_ERR;
		sd_status = phandle->SynFifo.sd_list[sdIndex].m_state;
	}
	else if(TYPE_ACTION == cmd_type)
	{
		phandle->g_cmdType = TYPE_ACTION;
		__Portx_DEBUG(0, "[app_SerialPorts_Transmit_DMA] TYPE_ACTION, PORTx_BUSY,  %lld\r\n", system_time);
		App_uThread_LIFO_Uart_Get(&phandle->ActnFifo, &sdIndex);
		sd_status = HAL_UART_Transmit_DMA(phandle->huart, phandle->ActnFifo.sd_list[sdIndex].m_data, phandle->ActnFifo.sd_list[sdIndex].m_len);
		if(HAL_OK != sd_status)
			phandle->ActnFifo.sd_list[sdIndex].m_state = UART_SEND_ERR;
		else
			phandle->ActnFifo.sd_list[sdIndex].m_state = UART_SEND_NO_ERR;
		sd_status = phandle->ActnFifo.sd_list[sdIndex].m_state;
	}
	else
	{
		__Portx_DEBUG(0, "[app_SerialPorts_Transmit_DMA] pHandle[%d].cmd_type=%d, impossible\r\n", phandle->portIndex, cmd_type);
	}
	return sd_status;
}

static emUart_send_state app_SerialPorts_SynCmd_SendDMA(APP_PORT_HANDLE *phandle)
{
	if(phandle->stComParam.synWaitCNT >= phandle->stComParam.synInterval)
	{
		phandle->stComParam.synWaitCNT = 0;
		return app_SerialPorts_Transmit_DMA(phandle, TYPE_SYN);
	}
	phandle->stComParam.synWaitCNT++;
	return UART_SEND_NO_ERR;
}

static emUart_send_state app_SerialPorts_ActnCmd_SendDMA(APP_PORT_HANDLE *phandle)
{
	phandle->stComParam.synWaitCNT = phandle->stComParam.synInterval;	//发送完后立即完成同步
	return app_SerialPorts_Transmit_DMA(phandle, TYPE_ACTION);
}

/*
 * 等待设备的响应完全结束
 * 在同步间隔为0时，部分设备在刚连上收到MCU的大量指令时，会分批依次回复这些指令，此函数用于延时接收这批响应，以保证后续发送与接受的空闲中断能一一对应
 * 返回0-清理未完成，1-清理结束
 */
static uint8_t  app_SerialPorts_RecvCache_Clear(APP_PORT_HANDLE *phandle)
{
	__Portx_DEBUG(0, "[app_SerialPorts_RecvCache_Clear] pHandle[%d].rxSizeOld=[%d],g_uartRxSize=[%d]\r\n", phandle->portIndex, phandle->g_uartRxSizeOld, phandle->g_uartRxSize);
	if(phandle->g_uartRxSizeOld == phandle->g_uartRxSize)
	{
		phandle->stComParam.rxDataWaitCNT++;
		if(phandle->stComParam.rxDataWaitCNT >= phandle->stComParam.rxDataWaitMax)
		{
			phandle->stComParam.rxDataWaitCNT = 0;		//计数完成清理计数
			return 1;
		}
	}
	else
	{
		phandle->stComParam.rxDataWaitCNT = 0;			//有一次不匹配就重新计数
	}
	phandle->g_uartRxSizeOld = phandle->g_uartRxSize;
	return 0;
}

/*
 * 发送前的环境检查
 */
static emPorxIsAllowSend app_SerialPorts_BeforeSend_deal(APP_PORT_HANDLE *phandle)
{
	if((PORTx_Disconnect == phandle->g_isConnectOld) && (PORTx_Connect == phandle->g_isConnect))	//从未连接到连接
	{
		phandle->g_isNeedRecvClear = 1;
	}
	phandle->g_isConnectOld = phandle->g_isConnect;
	
	if(1 == phandle->g_isNeedRecvClear)					//等待设备的响应完全结束
	{
		if(app_SerialPorts_RecvCache_Clear(phandle))
		{
			phandle->g_isNeedRecvClear = 0;
			return PORTX_ALLOW_SEND;
		}
		else
			return PORTX_DONT_SEND;
	}
	return PORTX_ALLOW_SEND;
}
