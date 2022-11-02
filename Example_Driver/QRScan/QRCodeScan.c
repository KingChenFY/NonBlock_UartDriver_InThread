#include "Units.h"

#define GETCMD_HEAD_LOW			0x7E
#define GETCMD_HEAD_HIGH		0x00

#define GETCMD_RE_HEAD_LOW		0x02
#define GETCMD_RE_HEAD_HIGH		0x00

#define GETCMD_TYPE				0x33
#define GETCMD_RE_TYPE			0x34
#define QRCODE_CMD_RE_TYPE		0x36

#define SETCMD_RE_OK			0x06
#define SETCMD_RE_FAIL			0x15

#define SCAN_FAIL_RE_CODE_LOW	0x4E
#define SCAN_FAIL_RE_CODE_HIGH	0x47

#define GETCMD_RE_EXCLUDE_DATA_TYPE_LEN			5		//头2字节+长度2字节+LRC1字节
#define GETCMD_RE_EXCLUDE_DATA_LEN				6		//头2字节+长度2字节+LRC1字节+type1字节

static unsigned char sdCmd[20] = {0};
static uint8_t sdLen = 0;
static uint8_t hasLen = 0;
MESSAGE_RECV_DATA_TO_ANALYSE rData = {0};
volatile uint8_t QRScan_isScan = 0;

static void QRScan_Syn_format(MESSAGE_SYN_S_DTAT_LIST *data);
static uint8_t QRScan_Syn_Analyze(MESSAGE_SYN_R_DTAT_LIST *data);

typedef uint8_t (*Fun_QRScan_Cmd_Analyze) (MESSAGE_SYN_R_DTAT_LIST *data);
typedef void (*Fun_QRScan_Cmd_Format) (MESSAGE_SYN_S_DTAT_LIST *data);

typedef struct
{
	emQRScanSynCmdId m_uCmdId;
    Fun_QRScan_Cmd_Analyze m_QRScan_Cmd_Analyze;
    Fun_QRScan_Cmd_Format m_QRScan_Cmd_Format;
}ST_QRScan_CMD_DEAL;

static ST_QRScan_CMD_DEAL s_QRScanCmdDeal[emQRScanId_END] =
{
    {emQRScanId_SYN, QRScan_Syn_Analyze, QRScan_Syn_format},
};
/*
 * 二维码扫描同步指令-解析函数
 */
uint8_t QRScan_SynCmd_Deal(MESSAGE_SYN_R_DTAT_LIST *r_list, uint8_t index)
{
    if((index >= emQRScanId_SYN) && (index < emQRScanId_END))
    {
        if(0 != s_QRScanCmdDeal[index].m_QRScan_Cmd_Analyze)
        {
            return ((Fun_QRScan_Cmd_Analyze)s_QRScanCmdDeal[index].m_QRScan_Cmd_Analyze)(&r_list[index]);
        }
    }
    return WK_FALSE;
}
/*
 * 二维码扫描同步指令-封装函数
 */
void QRScan_SynCmd_Format(MESSAGE_SYN_S_DTAT_LIST *s_list)
{
	for(uint8_t i=emQRScanId_SYN; i<emQRScanId_END; i++)
	{
		((Fun_QRScan_Cmd_Format)s_QRScanCmdDeal[i].m_QRScan_Cmd_Format)(&s_list[i]);
		s_list[i].m_state = UART_SEND_NO_ERR;
	}
}
/*
 * 二维码扫描接收数据-校验函数
 * r_size-实际接收数据长度
 * r_addr-接收数据的地址
 * min_size-发送时给进的参数，接收数据的最小长度
 * [out]offset-校验结果的偏移
 * [out]effective_len-校验后数据的有效长度
 */
emUart_recv_state QRScan_Cmd_Verify(uint8_t r_size, uint8_t min_size, uint8_t *r_addr, uint8_t *offset, uint8_t *effective_len)
{
	__Portx_DEBUG(0, "[QRScan_Cmd_Verify] r_size=%d, min_size=%d\r\n", r_size, min_size);
	if((r_size >= min_size) && (r_addr != NULL))
	{
		*offset = 0;
		*effective_len = r_size;
		return UART_RECV_NO_ERR;
	}
	return UART_RECV_DATA_ERR;
}

static uint8_t QRScan_GetCmd_Verify(MESSAGE_RECV_DATA_TO_ANALYSE *rData, uint8_t *offset, uint8_t min_size)
{
	uint8_t dataLens = 0;
	uint8_t LRC = 0xFF;

	for(uint8_t offset=0; offset<=(rData->m_len-min_size); offset++)
	{
		if( (GETCMD_RE_HEAD_LOW == rData->m_data[offset]) && (GETCMD_RE_HEAD_HIGH == rData->m_data[offset+1]) && (GETCMD_RE_TYPE == rData->m_data[offset+4]) )
		{
			dataLens = (((uint16_t)rData->m_data[offset+2]) << 8) | (rData->m_data[offset+3]);
			if((rData->m_len - offset) >= (dataLens + GETCMD_RE_EXCLUDE_DATA_TYPE_LEN))
			{
				for(uint8_t i=2; i<(2+2+dataLens); i++)		//计算校验数据 0xff^lens^types^data
                {
                    LRC ^= rData->m_data[offset+i];
                }
				if(LRC == rData->m_data[offset+4+dataLens])
                {
                    return 1;
                }
			}
		}
	}
	return 0;
}
static uint8_t QRScan_SetCmd_Verify(MESSAGE_RECV_DATA_TO_ANALYSE *rData, uint8_t min_size)
{
	for(uint8_t offset=0; offset<=(rData->m_len-min_size); offset++)
	{
		if(SETCMD_RE_OK == rData->m_data[offset])
			return 1;
		if(SETCMD_RE_FAIL == rData->m_data[offset])
			return 0;
	}
	return 0;
}

/*
 * 同步指令
 */
static void QRScan_Syn_format(MESSAGE_SYN_S_DTAT_LIST *data)
{
	data->m_data[0] = '?';		//0x3F
	data->m_len = 1;
	data->m_recvlen = 1;
}
static uint8_t QRScan_Syn_Analyze(MESSAGE_SYN_R_DTAT_LIST *data)
{
	if('!' == data->m_data[0])
		QRScan_isScan = 1;
	else
		QRScan_isScan = 0;
	return WK_TRUE;
}

/*
返回1表示成功 0表示失败
*/
emUart_fifo_state QRScan_On(void)
{
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;
    
    sdCmd[0] = 0x1b;
    sdCmd[1] = 0x31;
    sdLen = 2;
    hasLen = 1;

    f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_SetCmd_Verify(&rData, hasLen))
			return UART_FIFO_NO_ERR;
		else
			return UART_FIFO_RECV_DATA_ERR;
	}
	return f_state;
}
/*
返回1表示成功 0表示失败
*/
emUart_fifo_state QRScan_Off(void)
{
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;
    
    sdCmd[0] = 0x1b;
    sdCmd[1] = 0x30;
    sdLen = 2;
    hasLen = 1;

    f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_SetCmd_Verify(&rData, hasLen))
			return UART_FIFO_NO_ERR;
		else
			return UART_FIFO_RECV_DATA_ERR;
	}
	return f_state;
}


/*
读取返回1表示成功 0表示失败
modeData[3]
0x30+0x30+0x30：手动读码
0x30+0x30+0x31：自动读码
0x30+0x30+0x32：连续读码
*/
emUart_fifo_state QRScan_AskReadMode(uint8_t modeData[3])
{
    uint8_t offset = 0;
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;
    
    sdCmd[0] = 0x7E;
    sdCmd[1] = 0x00;
    sdCmd[2] = 0x00;
    sdCmd[3] = 0x05;
    sdCmd[4] = 0x33;
    sdCmd[5] = 0x44;
    sdCmd[6] = 0x30;
    sdCmd[7] = 0x30;
    sdCmd[8] = 0x30;
    sdCmd[9] = 0xBD;/* LRC */
    sdLen = 10;
    hasLen = 9;

	f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_GetCmd_Verify(&rData, &offset, hasLen))
		{
			modeData[0] = rData.m_data[offset+5];
			modeData[1] = rData.m_data[offset+6];
			modeData[2] = rData.m_data[offset+7];
			return UART_FIFO_NO_ERR;
		}
		else
		{
			__QR_Driver_DEBUG(0, "[QRScan_AskReadMode] QRScan_GetCmd_Verify--->UART_FIFO_RECV_DATA_ERR\r\n");
			return UART_FIFO_RECV_DATA_ERR;		
		}
	}
	return f_state;
}

/*
读取返回1表示成功 0表示失败
设置手动模式
0x30+0x30+0x30：手动读码//0302000
*/
emUart_fifo_state QRScan_SetReadMode(void)
{
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;
    
    sdCmd[0] = 0x4E;    	//N
    sdCmd[1] = 0x4C;		//L
    sdCmd[2] = 0x53;		//S
    sdCmd[3] = 0x30;		//0
    sdCmd[4] = 0x33;		//3
    sdCmd[5] = 0x30;		//0
    sdCmd[6] = 0x32;		//2
    sdCmd[7] = 0x30;		//0
    sdCmd[8] = 0x30;		//0
    sdCmd[9] = 0x30;		//0
    sdLen = 10;
    hasLen = 1;

    f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_SetCmd_Verify(&rData, hasLen))
			return UART_FIFO_NO_ERR;
		else
			return UART_FIFO_RECV_DATA_ERR;
	}
	return f_state;
}

/*
读取返回1表示成功 0表示失败
 packMode[4] 
Byte1： 0x30
Byte2： 0x30
Byte3： 0x30
Byte4： 0x30 - 不打包
        0x31 - 普通打包
*/
emUart_fifo_state QRScan_AskPackMode(uint8_t packMode[4])
{
    uint8_t offset = 0;
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;
    
    sdCmd[0] = 0x7E;
    sdCmd[1] = 0x00;
    sdCmd[2] = 0x00;
    sdCmd[3] = 0x04;
    sdCmd[4] = 0x33;
    sdCmd[5] = 0x46;
    sdCmd[6] = 0x30;
    sdCmd[7] = 0x30;
    sdCmd[8] = 0x8E;/* LRC */
    sdLen = 9;
    hasLen = 4 + GETCMD_RE_EXCLUDE_DATA_LEN;

	f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_GetCmd_Verify(&rData, &offset, hasLen))
		{
            packMode[0] = rData.m_data[offset+5];
            packMode[1] = rData.m_data[offset+6];
            packMode[2] = rData.m_data[offset+7];
            packMode[3] = rData.m_data[offset+8];
			return UART_FIFO_NO_ERR;
		}
		else
		{
			return UART_FIFO_RECV_DATA_ERR;
		}
	}
	return f_state;
}
/*
读取返回1表示成功 0表示失败
设置数据模式为普通打包方式//     0314010
*/
emUart_fifo_state QRScan_SetPackMode(void)
{
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;
    
    sdCmd[0] = 0x4E;
    sdCmd[1] = 0x4C;
    sdCmd[2] = 0x53;
    sdCmd[3] = 0x30;
    sdCmd[4] = 0x33;
    sdCmd[5] = 0x31;
    sdCmd[6] = 0x34;
    sdCmd[7] = 0x30;
    sdCmd[8] = 0x31;
    sdCmd[9] = 0x30;
    sdLen = 10;
    hasLen = 1;

    f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_SetCmd_Verify(&rData, hasLen))
			return UART_FIFO_NO_ERR;
		else
			return UART_FIFO_RECV_DATA_ERR;
	}
	return f_state;
}
/*
读取返回1表示成功 0表示失败
suffix[0]==0x01，说明数据后缀现在是使能状态，需要设置后缀失能
*/
emUart_fifo_state QRScan_AskSuffix(uint8_t suffix[1])
{
    uint8_t offset = 0;
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;

    sdCmd[0] = 0x7E;
    sdCmd[1] = 0x00;
    sdCmd[2] = 0x00;
    sdCmd[3] = 0x02;
    sdCmd[4] = 0x33;
    sdCmd[5] = 0x40;
    sdCmd[6] = 0x8E;/* LRC */
    sdLen = 7;
    hasLen = 8;

	f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_GetCmd_Verify(&rData, &offset, hasLen))
		{
			suffix[0] = rData.m_data[offset+5];
			return UART_FIFO_NO_ERR;
		}
		else
		{
			return UART_FIFO_RECV_DATA_ERR;
		}
	}
	return f_state;
}
/*
读取返回1表示成功 0表示失败
设置后缀失能//     0309000
*/
emUart_fifo_state QRScan_SetSuffix(void)
{
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;
    
    sdCmd[0] = 0x4E;
    sdCmd[1] = 0x4C;
    sdCmd[2] = 0x53;
    sdCmd[3] = 0x30;
    sdCmd[4] = 0x33;
    sdCmd[5] = 0x30;
    sdCmd[6] = 0x39;
    sdCmd[7] = 0x30;
    sdCmd[8] = 0x30;
    sdCmd[9] = 0x30;
    sdLen = 10;
    hasLen = 1;

    f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_SetCmd_Verify(&rData, hasLen))
			return UART_FIFO_NO_ERR;
		else
			return UART_FIFO_RECV_DATA_ERR;
	}
	return f_state;
}

/*
读取返回1表示成功 0表示失败
delayTime 返回延时时间
*/
emUart_fifo_state QRScan_AskDelay(uint32_t delayTime[1])
{
    uint8_t offset = 0;
	emUart_fifo_state f_state = UART_FIFO_NO_ERR;

    sdCmd[0] = 0x7E;
    sdCmd[1] = 0x00;
    sdCmd[2] = 0x00;
    sdCmd[3] = 0x05;
    sdCmd[4] = 0x33;
    sdCmd[5] = 0x44;
    sdCmd[6] = 0x30;
    sdCmd[7] = 0x33;
    sdCmd[8] = 0x30;
    sdCmd[9] = 0xBE;/* LRC */
    sdLen = 10;
    hasLen = 11 + GETCMD_RE_EXCLUDE_DATA_LEN;//0x30+0x33+0x30+0x30+延迟值（7bytes:0~3600000）

	f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_GetCmd_Verify(&rData, &offset, hasLen))
		{
			delayTime[0] = str2int(&rData.m_data[offset+9], 7);
			return UART_FIFO_NO_ERR;
		}
		else
		{
			return UART_FIFO_RECV_DATA_ERR;
		}
	}
	return f_state;
}

/*
读取返回1表示成功 0表示失败
delayTime 延时时间
*/
emUart_fifo_state QRScan_SetDelay(uint32_t delayTime)
{
    emUart_fifo_state f_state = UART_FIFO_NO_ERR;
    
    sdCmd[0] = 0x4E;
    sdCmd[1] = 0x4C;
    sdCmd[2] = 0x53;
    sdCmd[3] = 0x30;
    sdCmd[4] = 0x33;
    sdCmd[5] = 0x31;
    sdCmd[6] = 0x33;
    sdCmd[7] = 0x30;
    sdCmd[8] = 0x30;
    sdCmd[9] = 0x30;
    sdCmd[10] = 0x3D;
    sdLen = 11;
    sdLen += int2str(delayTime, (char *)(&sdCmd[11]));
    hasLen = 1;
    
    f_state = app_portx_sendANDrecv(sdCmd, sdLen, hasLen, &pHandle[THREAD_PORT_5_INDEX].ActnFifo, &rData);

	if(UART_FIFO_NO_ERR == f_state)
	{
		if(QRScan_SetCmd_Verify(&rData, hasLen))
			return UART_FIFO_NO_ERR;
		else
			return UART_FIFO_RECV_DATA_ERR;
	}
	return f_state;
}

uint8_t dataTemp[COM_FIFO_DATA_LENGHT_QRSCAN] = { 0 };
/*
读取返回1表示成功 0表示失败
scanData  扫描数据
*/
emQRScanState QRScan_AskScanData(uint8_t scanData[COM_FIFO_DATA_LENGHT_QRSCAN])
{
    uint8_t r_size = pHandle[THREAD_PORT_5_INDEX].g_uartRxSize;
    uint8_t *r_data = pHandle[THREAD_PORT_5_INDEX].s_comData;
    uint8_t r_minSize = 15;
    uint16_t rdDataLens = 0;			//读取到的数据长度（data）
    uint8_t rdDataTotalLen = 0;			//根据读到的数据长度+5 = 计算返回数据的总长度
    uint8_t LRC = 0xFF;

	if(r_size >= 2)
	{
		for(uint8_t i_offset=0; i_offset<=(r_size-2); i_offset++)
		{
			if((SCAN_FAIL_RE_CODE_LOW == r_data[i_offset]) && (SCAN_FAIL_RE_CODE_HIGH == r_data[i_offset+1]))
			{
				return QR_SCAN_FAIL;
			}
		}
	}
    if(r_size >= r_minSize)
    {
    	for(uint8_t i_offset=0; i_offset<=(r_size-r_minSize); i_offset++)
    	{
			if((GETCMD_RE_HEAD_LOW == r_data[i_offset]) && (GETCMD_RE_HEAD_HIGH == r_data[i_offset+1]) && (QRCODE_CMD_RE_TYPE == r_data[i_offset+4]))
			{
				rdDataLens = (((uint16_t)r_data[i_offset+2]) << 8) | (r_data[i_offset+3]);
				rdDataTotalLen = rdDataLens + 6;			//2字节头+2字节长度+1字节类型+1字节LRC
				if((r_size - i_offset) >= rdDataTotalLen)
				{
					for(uint8_t i =2; i < 2+2+1+rdDataLens; i++)//计算校验数据
					{
						LRC ^= r_data[i_offset+i];
					}
					if(LRC == r_data[i_offset+rdDataTotalLen-1])
					{
						memcpy(scanData, &r_data[i_offset+5], rdDataLens);
						return QR_SCAN_SUCCEED;
					}
				}
			}
    	}
    }
    return QR_WAIT_RESP;
}









