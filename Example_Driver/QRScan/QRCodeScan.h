#ifndef __SCAN_H__
#define __SCAN_H__

#ifdef __cplusplus
 extern "C" {
#endif 


#include "stm32h7xx.h"
#include "HardDef.h"
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>

#define QR_DRIVER_DEBUG
#ifdef QR_DRIVER_DEBUG
#define __QR_Driver_DEBUG 	SEGGER_RTT_printf
#else
#define __QR_Driver_DEBUG(BufferIndex, ...)
#endif

#define QR_APP_DEBUG
#ifdef QR_APP_DEBUG
#define __QRapp_DEBUG 	SEGGER_RTT_printf
#else
#define __QRapp_DEBUG(BufferIndex, ...)
#endif

#define QRSCAN_READ_TIMEOUT				100
#define QRSCAN_SYN_INTERVAL				100
#define QRSCAN_RX_WAIT_MAX				100
#define QRSCAN_SYN_LIST_NUM				1

typedef enum
{
	emQRScanId_SYN		= 0,
	emQRScanId_END		= 1,
}emQRScanSynCmdId;

 typedef enum
{
	QR_WAIT_RESP 	= 0,
	QR_SCAN_FAIL 	= 1,
	QR_SCAN_SUCCEED	= 2,		
}emQRScanState;

void QRScan_SynCmd_Format(MESSAGE_SYN_S_DTAT_LIST *s_list);
uint8_t QRScan_SynCmd_Deal(MESSAGE_SYN_R_DTAT_LIST *r_list, uint8_t index);
emUart_recv_state QRScan_Cmd_Verify(uint8_t r_size, uint8_t min_size, uint8_t *r_addr, uint8_t *offset, uint8_t *effective_len);


emUart_fifo_state QRScan_On(void);
emUart_fifo_state QRScan_Off(void);
emUart_fifo_state QRScan_Syn(void);
emUart_fifo_state QRScan_AskReadMode(uint8_t modeData[3]);
emUart_fifo_state QRScan_SetReadMode(void);
emUart_fifo_state QRScan_AskPackMode(uint8_t packMode[4]);
emUart_fifo_state QRScan_SetPackMode(void);
emUart_fifo_state QRScan_AskSuffix(uint8_t suffix[1]);
emUart_fifo_state QRScan_SetSuffix(void);
emUart_fifo_state QRScan_AskDelay(uint32_t delayTime[1]);
emUart_fifo_state QRScan_SetDelay(uint32_t delayTime);
emQRScanState QRScan_AskScanData(uint8_t scanData[COM_FIFO_DATA_LENGHT_QRSCAN]);
#ifdef __cplusplus
}
#endif

#endif /*__SCAN_H*/


