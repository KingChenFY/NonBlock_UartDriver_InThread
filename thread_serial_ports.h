#ifndef __THREAD_SERIAL_PORTS_H__
#define __THREAD_SERIAL_PORTS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "UartThreadFIFO.h"
#include "UartSynData.h"
#include "stm32h7xx_hal_uart.h"

#define THREAD_PORTX_DEBUG
#ifdef THREAD_PORTX_DEBUG
#define __Portx_DEBUG 	SEGGER_RTT_printf
#else
#define __Portx_DEBUG(BufferIndex, ...)
#endif

typedef enum
{
	THREAD_PORT_START			= 0,
	THREAD_PORT_5_INDEX 		= 1,
	THREAD_PORT_7_INDEX			= 2,
	THREAD_PORT_END,
}emUartIndex;

typedef enum
{
	Drive_NO_ERR 				= 0,
	Drive_Wait_Resp 			= 1,
	Drive_NoLink 				= 2,
	Drive_Recv_Verify_Failed 	= 3,
	Drive_FIFO_FULL 			= 4,
}emDriveConctStatus;

typedef enum
{
	PORTx_Disconnect		= 0,
	PORTx_Connect			= 1,
}emPortxConct;

typedef enum
{
	PORTx_IDLE				= 0,
	PORTx_BUSY				= 1,
}emPortxBusy;

typedef enum
{
	TYPE_SYN				= 0,
	TYPE_ACTION				= 1,
}emPortxCmdType;

typedef enum
{
	TYPE_FULL_DUPLEX		= 0,		//全双工
	TYPE_HALF_DUPLEX		= 1,		//半双工
}emPortxDirType;

typedef enum
{
	FUNC_EN					= 0,		//使能
	FUNC_DISABLE			= 1,		//失能
}emPorxFucOnOff;

typedef struct
{
	uint8_t m_len;
	uint8_t  m_data[APP_SERIAL_PORTS_THREAD_DATA_LENGTH];		//数据存放的位置
}MESSAGE_RECV_DATA_TO_ANALYSE;

typedef struct
{
	volatile emPorxFucOnOff e_SynHard;			//是否使能同步指令的自动发送 1-使能，0-关闭，该功能在通讯异常时不可用
}ST_COM_EXTRA_FUC_EN;

typedef struct
{
	uint16_t busyCnt;			//串口忙等待计数
	uint16_t busyMax;			//窗口忙最大等待次数
	uint16_t synWaitCNT;		//发送等待计数
	uint16_t synInterval;		//同步指令的同步间隔，单位为uart thread的sleepTime
	uint16_t rxDataWaitCNT;		//第一次连接成功等待接收延时
	uint16_t rxDataWaitMax;
}ST_COM_PARAMETER;

typedef emUart_recv_state (*Fun_UartCmd_Recv_Verify)(uint8_t r_size, uint8_t min_size, uint8_t *r_addr, uint8_t *offset, uint8_t *effective_len);

typedef struct
{
	emUartIndex m_uPortxId;
	UART_HandleTypeDef *huart;
	emPortxDirType m_duplexType;
	uint16_t m_busyMax;
	uint16_t m_synInterval;
	uint16_t m_rxDataWaitMax;
	uint8_t m_synListNum;
    Fun_synCmd_format m_FunSynCmd_Format;
	Fun_synCmd_deal m_FunSynCmd_Deal;
    Fun_UartCmd_Recv_Verify m_FunUartCmd_Verify;
}ST_PORTX_PARAM_INIT;

typedef struct
{
	UART_HandleTypeDef *huart;
	emUartIndex portIndex;
	ST_COM_PARAMETER stComParam;
	ST_COM_EXTRA_FUC_EN stFucOnOff;								//串口线程功能开关
	volatile emPortxConct g_isConnect;							//串口连接状态（指针类型的标志量为全局变量）
	volatile emPortxConct g_isConnectOld;
	volatile emPortxBusy g_uartIsIdle;							//串口是否空闲，由中断处理函数修改
	volatile uint8_t g_isReadEn;								//两个线程间的读写保护
	volatile uint8_t g_isCmdBeSend;								//是否有数据已发送
	volatile uint8_t g_isNeedRecvClear;							//是否需要清理缓存
	uint8_t g_uartRxSize;										//DMA空闲中断接收的数据长度
	uint8_t g_uartRxSizeOld;
	uint8_t g_lastCmdLost;										//是否超时未收到回复 0-否 1-是
	emPortxCmdType g_cmdType;									//指令的类型
	emPortxDirType g_duplexType;								//串口类型（是否全双工）
	uint8_t s_comData[APP_SERIAL_PORTS_THREAD_DATA_LENGTH];		//串口接收的数据的原始来源
	ST_UART_THREAD_FIFO ActnFifo;								//任务指令队列
	ST_UART_SYN_DATA SynFifo;									//同步指令队列					
	Fun_UartCmd_Recv_Verify fun_UartRe_Verify;					//指令校验处理回调函数
}APP_PORT_HANDLE;


extern APP_PORT_HANDLE pHandle[THREAD_PORT_END];

//串口线程
VOID  App_SerialPorts_Thread (ULONG thread_input);
//驱动文件调用，指令压入发送队列，接收队列数据获取函数
emUart_fifo_state app_portx_sendANDrecv(uint8_t *data, uint8_t dlen, uint8_t rlen, ST_UART_THREAD_FIFO *fifo, MESSAGE_RECV_DATA_TO_ANALYSE *rdata);
//中断处理函数
void app_portx_IT_deal(APP_PORT_HANDLE *phandle);
//查询设备是否在线
uint8_t app_portx_Is_OnLine(emUartIndex index);
//端口号注册初始化函数
void app_PortInfo_register(emUartIndex index);
//设置是否开启自动硬件同步功能
void app_portx_FucSynHard_set(emUartIndex index, emPorxFucOnOff state);



#ifdef __cplusplus
}
#endif
#endif /* __THREAD_SERIAL_PORTS_H__ */
