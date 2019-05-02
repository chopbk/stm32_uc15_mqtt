#ifndef	_GSM_H
#define	_GSM_H
#include "stm32f1xx_it.h"
#include "gsmConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "MQTTPacket.h"


#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
//###################################################################################################
#define				GsmMsgTeCharacterSet_GSM					"GSM"
#define				GsmMsgTeCharacterSet_UCS2					"UCS2"
#define				GsmMsgTeCharacterSet_IEC10646			"(ISO/IEC10646"
#define				GsmMsgTeCharacterSet_IRA					"IRA"
#define				GsmMsgTeCharacterSet_HEX					"HEX"
#define				GsmMsgTeCharacterSet_PCCP					"PCCP"
#define				GsmMsgTeCharacterSet_PCDN					"PCDN"
#define				GsmMsgTeCharacterSet_8859_1				"8859-1"





//----------------------
typedef	enum
{
	GsmDial_Nothing				=		0,
	GsmDial_Answer,
	GsmDial_NoDialTone,
	GsmDial_Busy,
	GsmDial_NoCarrier,
	GsmDial_NoAnswer,
	GsmDial_Data,	
}GsmDial_t;

#define POW_FLAG                 ((uint32_t)0x0001)
#define SIM_FLAG                 ((uint32_t)0x0002)
#define INTERNET_FLAG            ((uint32_t)0x0004)

//----------------------
typedef	enum
{
	GsmSignalQuality_0	=	0,
	GsmSignalQuality_25,
	GsmSignalQuality_50,
	GsmSignalQuality_75,
	GsmSignalQuality_100,	
}GsmSignalQuality_t;
//----------------------
typedef struct
{
	uint32_t  							LastTimeRecieved;
	uint8_t 								usartBuff;
	uint8_t 								usart1Buff;
	bool										usartRxError;
	bool										usartTxError;
	uint8_t 								RxBuffer[_GSM_RX_SIZE];
	uint8_t 								TxBuffer[_GSM_TX_SIZE];
	uint16_t								RxIndex;
	

	bool										PowerState;
	bool										CallerID;
	GsmSignalQuality_t			SignalQuality;
	uint8_t									SignalQualityCounter;
	
	bool										MsgTextMode;
	uint8_t									MsgTextModeFo,MsgTextModeVp,MsgTextModePid,MsgTextModeDcs;
	bool										MsgStoredOnSim;
	uint16_t								MsgStoredUsed;
	uint16_t								MsgStoredCapacity;
	char										MsgMessage[_GSM_RX_SIZE/2];
	char										MsgDate[18];
	char										MsgNumber[15];
	bool										MsgSendDone;
	char										MsgServiceCenter[18];
	char										MsgTeCharacterSet[16];
	
	char										CallerNumber[15];
	GsmDial_t								DialAnswer;
	#if (_GSM_DUAL_SIM_SUPPORT==1)
 	bool										MsgTextModeDS;
	GsmSignalQuality_t			SignalQualityDS;
	uint8_t									SignalQualityCounterDS;
	uint8_t									DefaultSim;	
	char										MsgServiceCenterDS[18];
	uint8_t									MsgTextModeFoDS,MsgTextModeVpDS,MsgTextModePidDS,MsgTextModeDcsDS;
	#endif

}Gsm_t;
//----------------------

//----------------------
//###################################################################################################
void	Gsm_UserProcess(void);
void	Gsm_SmsReceiveProcess(char *Number,char *Message,char *date);
void	Gsm_CallProcess(char *Number);
void	Gsm_RxCallBack(void);
void	uart1_RxCallBack(void);
void	Gsm_RxClear(void);
void	Gsm_TxClear(void);
void GsmTask(void const * argument);
void sttCheckRoutineTask(void const * argument);
bool gsmCheckPower(void);
//###################################################################################################
void	Gsm_Init(osPriority	Priority);
bool	Gsm_SetPower(bool ON_or_OFF);
//###################################################################################################
bool	Gsm_SetDefaultProfile(void);
bool	Gsm_SetDefaultFactory(void);
bool	Gsm_SaveConfig(void);
bool	Gsm_SignalQuality(void);
#if (_GSM_DUAL_SIM_SUPPORT==1)
bool	Gsm_SignalQualityDS(void);
#endif
bool	Gsm_SetTime(uint8_t Year,uint8_t Month,uint8_t Day,uint8_t Hour,uint8_t Min,uint8_t Sec,int8_t GMT_inQuarter);
bool	Gsm_GetTime(uint8_t *Year,uint8_t *Month,uint8_t *Day,uint8_t *Hour,uint8_t *Min,uint8_t *Sec);
#if (_GSM_DUAL_SIM_SUPPORT==1)
bool	Gsm_SetDefaultSim(uint8_t	SelectedSim_1_or_2);
#endif
bool  Gsm_Ussd(char *SendUssd,char *Answer);
bool  Gsm_UssdCancel(void);
//###################################################################################################
bool	Gsm_Answer(void);
bool	Gsm_WaitForDisconnectCall(uint16_t WaitSecond);
bool	Gsm_Dial(char *DialNumber,uint8_t WaitForAnswer_Second);
bool	Gsm_DisconnectAll(void);
bool	Gsm_DisconnectVoiceCall(void);
bool	Gsm_DtmfGereration(char *SendingNumber);
bool	Gsm_GetCallerNumber(void);
bool	Gsm_SetEnableShowCallerNumber(bool	Enable);
bool	Gsm_SetConnectedLineIdentification(bool Enable);
//###################################################################################################
bool	Gsm_MsgSetTextMode(bool	TextMode);
bool	Gsm_MsgGetTextMode(void);
bool	Gsm_MsgDeleteAll(void);
bool	Gsm_MsgDelete(uint8_t MsgIndex);
bool	Gsm_MsgSetStoredOnSim(bool	OnSimCard);
bool	Gsm_MsgGetStatus(void);
bool	Gsm_MsgRead(uint16_t MsgIndex);
bool	Gsm_MsgSend(char *Number,char *message);
bool	Gsm_MsgSetTextModeParameter(uint8_t fo,uint8_t vp,uint8_t pid,uint8_t dcs);
bool	Gsm_MsgGetTextModeParameter(void);
bool	Gsm_MsgGetSmsServiceCenter(void);
bool	Gsm_MsgSetSmsServiceCenter(char *ServiceCenter);
bool	Gsm_MsgSetTeCharacterset(char *GsmMsgTeCharacterSet);
bool	Gsm_MsgGetTeCharacterset(void);
bool	Gsm_MsgRestoreSettings(uint8_t selectProfile_0_to_3);
bool	Gsm_MsgSaveSettings(uint8_t selectProfile_0_to_3);
bool Gsm_SocketDeAct(uint8_t connectID);
bool Gsm_SocketAct(uint8_t connectID);
bool Gsm_ListActContext();
#if (_GSM_DUAL_SIM_SUPPORT==1)
bool	Gsm_MsgGetSmsServiceCenterDS(void);
#endif
//###################################################################################################
bool	Gsm_SetWhiteNumberOff(void);
bool	Gsm_SetWhiteNumberEmpty(void);
bool	Gsm_SetWhiteNumber(uint8_t	Index_1_to_30,char *PhoneNumber);
bool	Gsm_GetWhiteNumber(uint8_t	Index_1_to_30,char *PhoneNumber);
void Gsm_HandleError(uint16_t error);

//###################################################################################################
//###################################################################################################
//##############Define TCPIP GSM function################################
bool Gsm_SocketClose(uint8_t connectID);
uint8_t Gsm_SocketReadData(uint8_t connectID, uint8_t * buf, uint8_t length);
bool Gsm_SocketSendData(uint8_t connectID, uint8_t * buf, uint8_t len);
uint16_t Gsm_SocketOpen(uint8_t connectID);
int Gsm_SocketGetStatus(uint8_t connectID);
bool Gsm_MqttDisconnect(uint8_t connectID);
bool Gsm_MqttConnect(uint8_t connectID, MQTTPacket_connectData *data);
bool Gsm_MqttPublish(uint8_t connectID,char * payload, MQTTPacket_connectData *data,MQTTString topicString);
bool Gsm_MqttSubscribe(uint8_t connectID, MQTTPacket_connectData *data,MQTTString topicString);
#endif


