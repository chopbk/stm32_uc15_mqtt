
#include "Gsm.h"
#include "GsmConfig.h"
#include "ringbuf.h"
RINGBUF RxUart3RingBuff;
RINGBUF RxUart1RingBuff;
osThreadId GsmTaskHandle;
osSemaphoreId myBinarySem01Handle;
osMessageQId datQueueHandle;
osThreadId sttCheckRoutineHandle;
osMessageQId ledQueueHandle;
osThreadId gsmTaskNameHandle;
Gsm_t Gsm;
uint8_t connectID = 0;
uint8_t count_error = 0;


void * malloc(size_t size)
{
	void * ptr = NULL;

	if (size > 0)
	{
		// We simply wrap the FreeRTOS call into a standard form
		ptr = pvPortMalloc(size);
	} // else NULL if there was an error
	return ptr;
}


void free(void * ptr)
{
	if (ptr)
	{
		// We simply wrap the FreeRTOS call into a standard form
		vPortFree(ptr);
	}
}


void Gsm_InitValue(void)
{
	bool result;

	osDelay(1000);
	result = Gsm_SetPower(true);					//	turn on module
	if (result)
	{
		printf("power on success\r\n");
	}
	else 
	{
		printf("power on fail\r\n");
	}
	osDelay(1000);
	//TODO: add more init function
#if 0
	Gsm_UssdCancel();

#if (								_GSM_DUAL_SIM_SUPPORT==1)
	Gsm_SetDefaultSim(1);
#endif

	Gsm_MsgSetStoredOnSim(false);					//	select message sored to mudule
	Gsm_MsgGetStatus(); 							//	read message stored capacity and more
	Gsm_MsgSetTextMode(true);						//	set to text mode
	Gsm_SetEnableShowCallerNumber(true);			//	set enable show caller number
	Gsm_SetConnectedLineIdentification(true);		//	set enable Connected Line Identification
	Gsm_MsgGetSmsServiceCenter();
	Gsm_MsgGetTextModeParameter();
	Gsm_MsgGetTeCharacterset();
	if ((Gsm.MsgTextModeFo != 17) || (Gsm.MsgTextModeVp != 167) || (Gsm.MsgTextModePid != 0) ||
		 (Gsm.MsgTextModeDcs != 0))
		Gsm_MsgSetTextModeParameter(17, 167, 0, 0);

#if (								_GSM_DUAL_SIM_SUPPORT==1)
	Gsm_SetDefaultSim(2);
	Gsm_MsgSetTextMode(true);
	Gsm_MsgSetStoredOnSim(false);					//	select message sored to mudule
	Gsm_MsgGetSmsServiceCenterDS();
	Gsm_MsgGetTextModeParameter();
	if ((Gsm.MsgTextModeFoDS != 17) || (Gsm.MsgTextModeVpDS != 167) || (Gsm.MsgTextModePidDS != 0) ||
		 (Gsm.MsgTextModeDcsDS != 0))
		Gsm_MsgSetTextModeParameter(17, 167, 0, 0);
	Gsm_SetDefaultSim(1);
#endif

#endif
}


//#########################################################################################################
bool Gsm_SendRaw(uint8_t * data, uint16_t len)
{
	if (len <= _GSM_TX_SIZE)
	{
		memcpy(Gsm.TxBuffer, data, len);
		if (HAL_UART_Transmit(&_GSM_USART, data, len, 100) == HAL_OK)
			return true;

		else 
			return false;
	}
	else 
		return false;
}


//#########################################################################################################
bool Gsm_SendString(char * data)
{
	return Gsm_SendRaw((uint8_t *) data, strlen(data));
}


//#########################################################################################################
bool Gsm_SendStringAndWait(char * data, uint16_t DelayMs)
{
	if (Gsm_SendRaw((uint8_t *) data, strlen(data)) == false)
		return false;

	osDelay(DelayMs);
	return true;
}


//#########################################################################################################
bool Gsm_WaitForString(uint32_t TimeOut_ms, uint8_t * result, uint8_t CountOfParameter, ...)
{
	if (result == NULL)
		return false;

	if (CountOfParameter == 0)
		return false;

	*result = 0;
	va_list tag;
	va_start(tag, CountOfParameter);
	char * arg[CountOfParameter];
	for (uint8_t i = 0; i < CountOfParameter; i++)
		arg[i] = va_arg(tag, char *);
	va_end(tag);
	//////////////////////////////////	
	for (uint32_t t = 0; t < TimeOut_ms; t += 50)
	{
		osDelay(50);
		for (uint8_t mx = 0; mx < CountOfParameter; mx++)
		{
			if (strstr((char *) Gsm.RxBuffer, arg[mx]) != NULL)
			{
				*result = mx + 1;
				return true;
			}
		}
	}
	// timeout
	return false;
}


//#########################################################################################################
bool Gsm_ReturnString(char * result, uint8_t WantWhichOne, char * SplitterChars)
{
	if (result == NULL)
		return false;

	if (WantWhichOne == 0)
		return false;

	char * str = (char *)
	Gsm.RxBuffer;
	str = strtok(str, SplitterChars);
	if (str == NULL)
	{
		strcpy(result, "");
		return false;
	}
	while (str != NULL)
	{
		str = strtok(NULL, SplitterChars);
		if (str != NULL)
			WantWhichOne--;
		if (WantWhichOne == 0)
		{
			strcpy(result, str);
			return true;
		}
	}
	strcpy(result, "");
	return false;
}


//#########################################################################################################
bool Gsm_ReturnStrings(char * InputString, char * SplitterChars, uint8_t CountOfParameter, ...)
{
	if (CountOfParameter == 0)
		return false;

	va_list tag;
	va_start(tag, CountOfParameter);
	char * arg[CountOfParameter];
	for (uint8_t i = 0; i < CountOfParameter; i++)
		arg[i] = va_arg(tag, char *);
	va_end(tag);
	char * str;
	str = strtok(InputString, SplitterChars);
	if (str == NULL)
		return false;

	uint8_t i = 0;
	while (str != NULL)
	{
		str = strtok(NULL, SplitterChars);
		if (str != NULL)
			CountOfParameter--;
		strcpy(arg[i], str);
		i++;
		if (CountOfParameter == 0)
		{
			return true;
		}
	}
	return false;
}


//#########################################################################################################
bool Gsm_ReturnInteger(int32_t * result, uint8_t WantWhichOne, char * SplitterChars)
{
	if ((char *)
	Gsm.RxBuffer == NULL)
		return false;

	if (Gsm_ReturnString((char *) Gsm.RxBuffer, WantWhichOne, SplitterChars) == false)
		return false;

	*result = atoi((char *) Gsm.RxBuffer);
	return true;
}


//#########################################################################################################
bool Gsm_ReturnFloat(float * result, uint8_t WantWhichOne, char * SplitterChars)
{
	if ((char *)
	Gsm.RxBuffer == NULL)
		return false;

	if (Gsm_ReturnString((char *) Gsm.RxBuffer, WantWhichOne, SplitterChars) == false)
		return false;

	*result = atof((char *) Gsm.RxBuffer);
	return true;
}


//#########################################################################################################
void Gsm_RemoveChar(char * str, char garbage)
{
	uint16_t MaxBuffCounter = _GSM_RX_SIZE;
	char * src, *dst;
	for (src = dst = str; *src != '\0'; src++)
	{
		*dst = *src;
		if (*dst != garbage)
			dst++;
		MaxBuffCounter--;
		if (MaxBuffCounter == 0)
			break;
	}
	*dst = '\0';
}


//#########################################################################################################
void Gsm_RxClear(void)
{
	HAL_UART_Receive_IT(&_GSM_USART, &Gsm.usartBuff, 1);
	memset(Gsm.RxBuffer, 0, _GSM_RX_SIZE);
	Gsm.RxIndex = 0;
}


//#########################################################################################################
void Gsm_TxClear(void)
{
	memset(Gsm.TxBuffer, 0, _GSM_TX_SIZE);
}


//#########################################################################################################
void Gsm_RxCallBack(void)
{
	Gsm.RxBuffer[Gsm.RxIndex] = Gsm.usartBuff;
	RINGBUF_Put(&RxUart3RingBuff, Gsm.usartBuff);
	if (Gsm.RxIndex < _GSM_RX_SIZE)
		Gsm.RxIndex++;
	else 
		Gsm.usartRxError = true;
	HAL_UART_Receive_IT(&_GSM_USART, &Gsm.usartBuff, 1);
	Gsm.LastTimeRecieved = HAL_GetTick();
}


void uart1_RxCallBack(void)
{
	RINGBUF_Put(&RxUart1RingBuff, Gsm.usart1Buff);
	HAL_UART_Receive_IT(&_SR_USART, &Gsm.usart1Buff, 1);
}


void sttCheckRoutineTask(void const * argument)
{
	/*Power on*/
	bool result;
	uint32_t data = 0x0;
	osDelay(_GSM_WAIT_TIME_LOW*2);
	while (1)
	{
		data = 0;
		/*Check Power status */
		result = gsmCheckPower();
		if (result == false)
		{
			result = Gsm_SetPower(true);
			if (result == false)
			{
				data &= ~POW_FLAG;
				osThreadSuspend(gsmTaskNameHandle);
			}
			else 
				goto check_sim_inserted;
		}
		else 
		{
check_sim_inserted:
			osDelay(_GSM_WAIT_TIME_MED);
			data |= POW_FLAG;
			result = Gsm_CheckSimInsertedStatus();
			if (result == false)
			{
				data &= ~SIM_FLAG;
				osThreadSuspend(gsmTaskNameHandle);
			}
			else 
			{
Check_Network_Registration:
				osDelay(_GSM_WAIT_TIME_MED);
				data |= SIM_FLAG;
				result = Gsm_CheckNetworkRegistration();
				if (result == false)
				{
					data &= ~INTERNET_FLAG;
					osThreadSuspend(gsmTaskNameHandle);
				}
				else 
				{
					data |= INTERNET_FLAG;
					osThreadResume(gsmTaskNameHandle);
				}
			}
		}
		if (osMessageAvailableSpace(datQueueHandle))
			osMessagePut(ledQueueHandle, data, 100);
		/*will check quectel status in 3 seconds*/
		osDelay(3 * _GSM_WAIT_TIME_LOW);
	}
	while (1)
	{
		osDelay(2000);
		//osThreadSuspend(sttCheckRoutineHandle);
#if DETAILED_DEBUG
		//printf("POWER CHECK \r\n");
#endif

		data = 0;
		/*Check Power status */
		result = gsmCheckPower();
		if (result == false)
		{
			result = Gsm_SetPower(true);
			if (result == false)
			{
				osThreadSuspend(gsmTaskNameHandle);

#if DETAILED_DEBUG
				printf("UC15 POWER FAIL\r\n");
#endif

				data &= ~POW_FLAG;
				goto send_message;
			}
		}
		else 
		{
			osThreadResume(gsmTaskNameHandle);
			data |= POW_FLAG;
		}
		/*TODO: raise event POW_FLAG*/
		/*TODO : check sim signal status and raise event */
		data |= SIM_FLAG;
		/*TODO : check host status and raise event*/
		data |= INTERNET_FLAG;
send_message:
		if (osMessageAvailableSpace(datQueueHandle)) //osMessagePut(datQueueHandle, data, 100);
			osMessagePut(ledQueueHandle, data, 100);
	}
}


//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
void GsmTask(void const * argument)
{
	uint16_t GsmResult;
	HAL_UART_Receive_IT(&_GSM_USART, &Gsm.usartBuff, 1);
	/*
	osEvent event;
	uint32_t dataqueue;
	*/
	Gsm_RxClear();
	Gsm_TxClear();
	HAL_UART_Receive_IT(&_SR_USART, &Gsm.usart1Buff, 1);
	MQTTString topicString = MQTTString_initializer;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	data.clientID.cstring = "lampl";
	topicString.cstring = "vpt";
	char payload[20];
	int count_payload = 0;

#if DETAILED_DEBUG
	printf("WAIT POWER\r\n");
#endif

	osThreadSuspend(gsmTaskNameHandle);
	osDelay(_GSM_WAIT_TIME_MED);
	if (Gsm_GetPDPContext() == false)
	{
		Gsm_ResetPDPContext(CONTEXT_ID);
	}
	while (1)
	{
		/*fix time interval for send - 30s*/
		osDelay(3 * _GSM_WAIT_TIME_MED);
		if (CONNECT_ID > MAX_CONNECT_ID)
			CONNECT_ID = 0;
		count_error = 0;
		sprintf(payload, "count: %d\n", count_payload);
		do 
		{
			GsmResult = Gsm_SocketGetStatus(CONNECT_ID);
			/*Current TCPIP connection status is not IP INITIAL, IP STATUS and IP CLOSE
			*(Query by command AT+QISTAT). If current status is TCP CONNECTING, execute
			command AT+QICLOSE to close current failed TCP connection. If current status is
			others, execute command AT+QIDEACT to deactivate current failed GPRS context*/
			if (GsmResult == STATE_CONNECTED)
			{
				goto mqtt_publish;
			}
			if (GsmResult == STATE_INITIAL)
				goto open_socket;
			if (GsmResult == STATE_OPENING)
			{
				Gsm_ResetPDPContext(CONTEXT_ID);
				continue;
			}
			if (GsmResult == STATE_ERROR)
			{
				/*some error, need to wait sim start SUCCESSfully*/
				osDelay(_GSM_WAIT_TIME_MED);
				continue;
			}
			if (Gsm_SocketClose(CONNECT_ID) == false)
				goto increase_connectID;
open_socket:
			GsmResult = Gsm_SocketOpen(CONNECT_ID);
			/*call gsm handle socket error*/
			if (GsmResult != 0)
			{
				if (count_error >= MAX_ERROR)
				{
					count_error = 0;
					Gsm_SendPowerOff();
					/*wait power on again*/
					osDelay(_GSM_WAIT_TIME_HIGH);
					continue;
				}
				Gsm_HandleError(GsmResult);
				goto close_point;
				;
			}
			osDelay(_GSM_WAIT_TIME_LOW);
			/*mqtt connect*/
			if (Gsm_MqttConnect(CONNECT_ID, &data) == false)
				goto close_point;
mqtt_publish:
			/*mqtt publish*/
#if DETAILED_DEBUG
			printf("Send publish count = %d\n\r", count_payload);
#endif

			if (Gsm_MqttPublish(CONNECT_ID, payload, &data, topicString) == false)
				goto close_point;
			/*if we can check MQTT CONNECT STATUS before mqtt_connect point
			, we don't need to call Gsm_MqttDisconnect or Gsm_SocketClose here*/
			//Gsm_MqttDisconnect(CONNECT_ID);
			/*now no need to close socket*/
			//Gsm_SocketClose(CONNECT_ID);
			break;
close_point:
			Gsm_SocketClose(CONNECT_ID);
increase_connectID:
			CONNECT_ID++;
		}
		while(CONNECT_ID < MAX_CONNECT_ID);
		count_payload++;

#if 0
		if (Gsm.MsgStoredUsed > 0) //	sms on memory
		{
			for (uint8_t i = 1; i <= 100; i++)
			{
				if (Gsm_MsgRead(i) == true)
				{
					Gsm_SmsReceiveProcess(Gsm.MsgNumber, Gsm.MsgMessage, Gsm.MsgDate);
					osDelay(100);
					if (Gsm_MsgDelete(i) == false)
					{
						osDelay(100);
						Gsm_MsgDelete(i);
					}
					osDelay(100);
				}
			}
		}
		Gsm_RxClear();
		if (Gsm_WaitForString(5000, &GsmResult, 2, "\r\nRING\r\n", "\r\nRINGDS\r\n") == true)
		{
			if ((GsmResult == 1) || (GsmResult == 2)) //	Recieve New Call
			{
				if (Gsm_GetCallerNumber() == true)
				{
					Gsm_CallProcess(Gsm.CallerNumber);
					if (GsmResult == 1)
					{
#if (													_GSM_DUAL_SIM_SUPPORT==1)
						Gsm_SetDefaultSim(1);
#endif

						Gsm_DisconnectVoiceCall();
					}
					if (GsmResult == 2)
					{
#if (													_GSM_DUAL_SIM_SUPPORT==1)
						Gsm_SetDefaultSim(2);
#endif

						Gsm_DisconnectVoiceCall();
					}
				}
			}
		}
		Gsm_MsgGetStatus();
		osDelay(100);
		Gsm_SignalQuality();
		osDelay(100);

#if (									_GSM_DUAL_SIM_SUPPORT==1)
		Gsm_SignalQualityDS();
		osDelay(100);
#endif

		Gsm_UserProcess();
		osDelay(100);
		if (HAL_GPIO_ReadPin(_GSM_POWER_STATUS_PORT, _GSM_POWER_STATUS_PIN) == GPIO_PIN_RESET)
		{
			osDelay(100);
			if (HAL_GPIO_ReadPin(_GSM_POWER_STATUS_PORT, _GSM_POWER_STATUS_PIN) == GPIO_PIN_RESET)
			{
				Gsm.PowerState = false;
				Gsm_SetPower(true);
			}
		}
		if (Gsm.SignalQuality == GsmSignalQuality_0)
		{
			Gsm.SignalQualityCounter++;
			if (Gsm.SignalQualityCounter > 10)
			{
				Gsm_SetPower(false);
				osDelay(5000);
				Gsm_InitValue();
				Gsm.SignalQualityCounter = 0;
			}
		}
		else 
			Gsm.SignalQualityCounter = 0;
#endif
	}
}


//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
void Gsm_Init(osPriority Priority)
{
	//TODO: must have this
	HAL_UART_Receive_IT(&_GSM_USART, &Gsm.usartBuff, 1);
	Gsm_RxClear();
	Gsm_TxClear();
	//TODO: why semaphore here ?
	//	osSemaphoreDef(myBinarySem01Handle);
	//	myBinarySem01Handle = osSemaphoreCreate(osSemaphore(myBinarySem01Handle), 1);
	osThreadDef(GsmTaskName, GsmTask, Priority, 0, _GSM_TASK_SIZE);
	GsmTaskHandle = osThreadCreate(osThread(GsmTaskName), NULL);
	if (GsmTaskHandle == NULL)
	{
		printf("gsmtask init fail \r\n");
	}
}


//#########################################################################################################
bool Gsm_SetPower(bool ON_or_OFF)
{
	Gsm_RxClear();
	Gsm_TxClear();
	if (ON_or_OFF == false) // Need Power OFF
	{
		if (HAL_GPIO_ReadPin(WISMO_RDY_GPIO_Port, WISMO_RDY_Pin) == GPIO_PIN_SET)
		{ //POWER ALREADY OFF
			Gsm.PowerState = true;
			return true;
		}
		else 
		{ //power is on, need to off
			HAL_GPIO_WritePin(WISMO_ON_GPIO_Port, WISMO_ON_Pin, GPIO_PIN_SET);
			osDelay(1200);
			HAL_GPIO_WritePin(WISMO_ON_GPIO_Port, WISMO_ON_Pin, GPIO_PIN_RESET);
			osDelay(3000);
			if (HAL_GPIO_ReadPin(WISMO_RDY_GPIO_Port, WISMO_RDY_Pin) == GPIO_PIN_RESET)
			{
				Gsm.PowerState = true;
				return true;
			}
			else 
			{
				Gsm.PowerState = false;
				return false;
			}
		}
	}
	else //	Need Power ON
	{
		if (HAL_GPIO_ReadPin(WISMO_RDY_GPIO_Port, WISMO_RDY_Pin) == GPIO_PIN_SET)
		{ //power is off now
			HAL_GPIO_WritePin(WISMO_ON_GPIO_Port, WISMO_ON_Pin, GPIO_PIN_SET);
			osDelay(1200);
			HAL_GPIO_WritePin(WISMO_ON_GPIO_Port, WISMO_ON_Pin, GPIO_PIN_RESET);
			osDelay(3000);
			if (HAL_GPIO_ReadPin(WISMO_RDY_GPIO_Port, WISMO_RDY_Pin) == GPIO_PIN_SET)
			{
				Gsm.PowerState = false;
				return false;
			}
			osDelay(3000);
			//	init AtCommands
			Gsm_SendStringAndWait("AT\r\n", 500);
			Gsm_SendStringAndWait("AT\r\n", 500);
			uint8_t result;
			if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 1, "OK") == false)
			{
				Gsm.PowerState = false;
				return false;
			}
			return true;
		}
		else 
		{ //power is on now
			HAL_GPIO_WritePin(WISMO_ON_GPIO_Port, WISMO_ON_Pin, GPIO_PIN_RESET);
			Gsm.PowerState = true;
			return true;
		}
	}
}


bool gsmCheckPower(void)
{
	if (HAL_GPIO_ReadPin(WISMO_RDY_GPIO_Port, WISMO_RDY_Pin) == GPIO_PIN_SET)
		return false;

	else 
		return true;
}


void Gsm_SendPowerOff(void)
{
	sprintf((char *) Gsm.TxBuffer, "AT+QPOWD\r\n");

#if DETAILED_DEBUG
	printf("command: %s", Gsm.TxBuffer);
#endif

	Gsm_SendString((char *) Gsm.TxBuffer);
}


//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
bool Gsm_SetDefaultProfile(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "ATZ0\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_SetDefaultFactory(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT&F0\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_SaveConfig(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT&W\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_SignalQuality(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSQ\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		if (Gsm_ReturnInteger((int32_t *) &result, 1, ":") == false)
			break;
		if (result == 99)
			Gsm.SignalQuality = GsmSignalQuality_0;
		else if (result < 8)
			Gsm.SignalQuality = GsmSignalQuality_25;
		else if ((result >= 8) && (result < 16))
			Gsm.SignalQuality = GsmSignalQuality_50;
		else if ((result >= 16) && (result < 24))
			Gsm.SignalQuality = GsmSignalQuality_75;
		else if ((result >= 24) && (result < 32))
			Gsm.SignalQuality = GsmSignalQuality_100;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
#if (							_GSM_DUAL_SIM_SUPPORT==1)
bool Gsm_SignalQualityDS(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSQDS\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		if (Gsm_ReturnInteger((int32_t *) &result, 1, ":") == false)
			break;
		if (result == 99)
			Gsm.SignalQualityDS = GsmSignalQuality_0;
		else if (result < 8)
			Gsm.SignalQualityDS = GsmSignalQuality_25;
		else if ((result >= 8) && (result < 16))
			Gsm.SignalQualityDS = GsmSignalQuality_50;
		else if ((result >= 16) && (result < 24))
			Gsm.SignalQualityDS = GsmSignalQuality_75;
		else if ((result >= 24) && (result < 32))
			Gsm.SignalQualityDS = GsmSignalQuality_100;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


#endif

//#########################################################################################################
bool Gsm_SetTime(uint8_t Year, uint8_t Month, uint8_t Day, uint8_t Hour, uint8_t Min, uint8_t Sec, int8_t GMT_inQuarter)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CCLK=\"%02d/%02d/%02d,%02d:%02d:%02d%+02d\"\r\n", Year, Month, Day, Hour, Min, Sec, GMT_inQuarter);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_GetTime(uint8_t * Year, uint8_t * Month, uint8_t * Day, uint8_t * Hour, uint8_t * Min, uint8_t * Sec)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CCLK?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		sscanf((char *) Gsm.RxBuffer, "AT+CCLK?\r\r\n+CCLK: \"%d/%d/%d,%d:%d:%d\":", (int *) Year, (int *) Month, (int *) Day, (int *) Hour, (int *) Min, (int *) Sec);
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
#if (							_GSM_DUAL_SIM_SUPPORT==1)
bool Gsm_SetDefaultSim(uint8_t SelectedSim_1_or_2)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CDSDS=%d\r\n", SelectedSim_1_or_2);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		Gsm.DefaultSim = SelectedSim_1_or_2;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_GetDefaultSim(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CDSDS?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		if (strstr((char *) Gsm.RxBuffer, "SIM1") != NULL)
			Gsm.DefaultSim = 1;
		if (strstr((char *) Gsm.RxBuffer, "SIM2") != NULL)
			Gsm.DefaultSim = 2;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


#endif

//#########################################################################################################
bool Gsm_Ussd(char * SendUssd, char * Answer)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CUSD=1,\"%s\"\r\n", SendUssd);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_VERYHIGH, &result, 2, "+CUSD:", "ERROR") == false)
		{
			sprintf((char *) Gsm.TxBuffer, "AT+CUSD=2\r\n");
			Gsm_SendString((char *) Gsm.TxBuffer);
			Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR");
			break;
		}
		if (result == 2)
			break;
		Gsm_RemoveChar((char *) Gsm.RxBuffer, '"');
		char * s = strstr((char *) Gsm.RxBuffer, "+CUSD: ");
		if (s == NULL)
			break;
		s += strlen("+CUSD: ");
		s += 2;
		strcpy(Answer, s);
		returnVal = true;
	}
	while(0);
	sprintf((char *) Gsm.TxBuffer, "AT+CUSD=2\r\n");
	Gsm_SendString((char *) Gsm.TxBuffer);
	Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR");
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_UssdCancel(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CUSD=2\r\n");
		Gsm_SendString((char *) Gsm.TxBuffer);
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
bool Gsm_Answer(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "ATA\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_WaitForDisconnectCall(uint16_t WaitSecond)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		if (Gsm_WaitForString(WaitSecond * 1000, &result, 1, "NO CARRIER") == false)
		{
			sprintf((char *) Gsm.TxBuffer, "AT+HVOIC\r\n");
			if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
				break;
			if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
				break;
			break;
		}
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_Dial(char * DialNumber, uint8_t WaitForAnswer_Second)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		if (WaitForAnswer_Second > 65)
			WaitForAnswer_Second = 65;
		if (DialNumber == NULL)
			break;
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "ATD%s;\r\n", DialNumber);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(WaitForAnswer_Second * 1000, &result, 7, "ERROR", "OK", "NO DIALTONE", "BUSY", "NO CARRIER", "NO ANSWER", "CONNECT") ==
			 false)
		{
			sprintf((char *) Gsm.TxBuffer, "AT+HVOIC\r\n");
			Gsm_SendString((char *) Gsm.TxBuffer);
			break;
		}
		if (result == 1)
			break;
		switch (result)
		{
			case 2:
				Gsm.DialAnswer = GsmDial_Answer;
				break;

			case 3:
				Gsm.DialAnswer = GsmDial_NoDialTone;
				break;

			case 4:
				Gsm.DialAnswer = GsmDial_Busy;
				break;

			case 5:
				Gsm.DialAnswer = GsmDial_NoCarrier;
				break;

			case 6:
				Gsm.DialAnswer = GsmDial_NoAnswer;
				break;

			case 7:
				Gsm.DialAnswer = GsmDial_Data;
				break;
		}

		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_DisconnectAll(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "ATH\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_DisconnectVoiceCall(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+HVOIC\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_DtmfGereration(char * SendingNumber)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+VTS=%s,2\r\n", SendingNumber);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW +200 * strlen(SendingNumber), &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_GetCallerNumber(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	bool returnVal = false;
	do 
	{
		if (Gsm.CallerID == true)
		{
			char * str = strstr((char *) Gsm.RxBuffer, "\r\n+CLIP");
			if (str == NULL)
				break;
			Gsm_RemoveChar(str, '"');
			if (Gsm_ReturnString(Gsm.CallerNumber, 1, ":,") == false)
				break;
			Gsm_RemoveChar(Gsm.CallerNumber, ' ');
			returnVal = true;
		}
		else 
			returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_SetEnableShowCallerNumber(bool Enable)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CLIP=%d\r\n", Enable);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
		Gsm.CallerID = Enable;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_SetConnectedLineIdentification(bool Enable)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+COLP=%d\r\n", Enable);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
		Gsm.CallerID = Enable;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
bool Gsm_MsgSetTextMode(bool TextMode)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CMGF=%d\r\n", TextMode);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;

#if (									_GSM_DUAL_SIM_SUPPORT==0)
		Gsm.MsgTextMode = TextMode;

#else

		if (Gsm.DefaultSim == 1)
			Gsm.MsgTextMode = TextMode;
		else 
			Gsm.MsgTextModeDS = TextMode;
#endif
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgGetTextMode(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CMGF?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		if (Gsm_ReturnInteger((int32_t *) &result, 1, ":") == false)
			break;
		Gsm.MsgTextMode = (bool)
		result;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgDeleteAll(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CMGD=1,4\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_HIGH, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	Gsm_MsgGetStatus();
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgDelete(uint8_t MsgIndex)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CMGD=%d,0\r\n", MsgIndex);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_HIGH, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	Gsm_MsgGetStatus();
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgSetStoredOnSim(bool OnSimCard)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		if (OnSimCard == false)
			sprintf((char *) Gsm.TxBuffer, "AT+CPMS=\"ME\",\"ME\",\"ME\"\r\n");
		else 
			sprintf((char *) Gsm.TxBuffer, "AT+CPMS=\"SM\",\"SM\",\"SM\"\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgGetStatus(void)
{
	//osSemaphoreWait(myBinarySem01Handle,osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CPMS?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		char str[3];
		sscanf((char *) Gsm.RxBuffer, "AT+CPMS?\r\r\n+CPMS: \"%2s\",%d,%d,", str, (int *) &Gsm.MsgStoredUsed, (int *) &Gsm.MsgStoredCapacity);
		if (strstr(str, "SM") != NULL)
			Gsm.MsgStoredOnSim = true;
		else 
			Gsm.MsgStoredOnSim = false;
		returnVal = true;
	}
	while(0);
	//osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgRead(uint16_t MsgIndex)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	char * str;
	bool returnVal = false;
	do 
	{
		strcpy(Gsm.MsgMessage, "");
		strcpy(Gsm.MsgDate, "");
		strcpy(Gsm.MsgNumber, "");
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CMGR=%d\r\n", MsgIndex);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		osDelay(100);
		Gsm_RemoveChar((char *) Gsm.RxBuffer, '"');
		str = strchr((char *) Gsm.RxBuffer, ',');
		if (str == NULL)
			break;
		str++;
		uint16_t msglen;
		sscanf(str, "%[^,],,%[^+]%d\r\n%[^\r]", Gsm.MsgNumber, Gsm.MsgDate, (int *) &msglen, Gsm.MsgMessage);
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgSend(char * Number, char * message)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	Gsm.MsgSendDone = false;
	do 
	{
#if (									_GSM_DUAL_SIM_SUPPORT==0) 
		if (Gsm.MsgTextMode == false)
		{
		}

#else

		if ((Gsm.DefaultSim == 1) && (Gsm.MsgTextMode == false))
		{
		}
		if ((Gsm.DefaultSim == 2) && (Gsm.MsgTextModeDS == false))
		{
		}
#endif

		else 
		{
			Gsm_RxClear();
			sprintf((char *) Gsm.TxBuffer, "AT+CMGS=\"%s\"\r\n", Number);
			if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
				break;
			if (Gsm_WaitForString(_GSM_WAIT_TIME_VERYHIGH, &result, 2, ">", "ERROR") == false)
				break;
			if (result == 2)
				break;
			Gsm_RxClear();
			sprintf((char *) Gsm.TxBuffer, "%s%c%c", message, 26, 0);
			if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
				break;
			if (Gsm_WaitForString(_GSM_WAIT_TIME_VERYHIGH, &result, 2, "OK", "ERROR") == false)
				break;
			if (result == 2)
				break;
			Gsm.MsgSendDone = true;
			returnVal = true;
		}
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//########################################################################################################
bool Gsm_MsgSetTextModeParameter(uint8_t fo, uint8_t vp, uint8_t pid, uint8_t dcs)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSMP=%d,%d,%d,%d\r\n", fo, vp, pid, dcs);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;

#if (									_GSM_DUAL_SIM_SUPPORT==0)
		Gsm.MsgTextModeFo = fo;
		Gsm.MsgTextModeVp = vp;
		Gsm.MsgTextModePid = pid;
		Gsm.MsgTextModeDcs = dcs;

#else

		if (Gsm.DefaultSim == 1)
		{
			Gsm.MsgTextModeFo = fo;
			Gsm.MsgTextModeVp = vp;
			Gsm.MsgTextModePid = pid;
			Gsm.MsgTextModeDcs = dcs;
		}
		else 
		{
			Gsm.MsgTextModeFoDS = fo;
			Gsm.MsgTextModeVpDS = vp;
			Gsm.MsgTextModePidDS = pid;
			Gsm.MsgTextModeDcsDS = dcs;
		}
#endif

		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//########################################################################################################
bool Gsm_MsgGetTextModeParameter(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSMP?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		char * str = strchr((char *) Gsm.RxBuffer, ':');
		if (str == NULL)
			break;
		str++;

#if (									_GSM_DUAL_SIM_SUPPORT==0)
		sscanf(str, "%d,%d,%d,%d", (int *) &Gsm.MsgTextModeFo, (int *) &Gsm.MsgTextModeVp, (int *) &Gsm.MsgTextModePid, (int *) &Gsm.MsgTextModeDcs);

#else

		if (Gsm.DefaultSim == 1)
			sscanf(str, "%d,%d,%d,%d", (int *) &Gsm.MsgTextModeFo, (int *) &Gsm.MsgTextModeVp, (int *) &Gsm.MsgTextModePid, (int *) &Gsm.MsgTextModeDcs);
		else 
			sscanf(str, "%d,%d,%d,%d", (int *) &Gsm.MsgTextModeFoDS, (int *) &Gsm.MsgTextModeVpDS, (int *) &Gsm.MsgTextModePidDS, (int *) &Gsm.MsgTextModeDcsDS);
#endif

		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgGetSmsServiceCenter(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSCA?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		Gsm_RemoveChar((char *) Gsm.RxBuffer, '"');
		Gsm_RemoveChar((char *) Gsm.RxBuffer, ' ');
		Gsm_ReturnString(Gsm.MsgServiceCenter, 1, ":,");
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
#if (							_GSM_DUAL_SIM_SUPPORT==1)
bool Gsm_MsgGetSmsServiceCenterDS(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSCA?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		Gsm_RemoveChar((char *) Gsm.RxBuffer, '"');
		Gsm_RemoveChar((char *) Gsm.RxBuffer, ' ');
		Gsm_ReturnString(Gsm.MsgServiceCenterDS, 1, ":,");
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


#endif

//#########################################################################################################
bool Gsm_MsgSetSmsServiceCenter(char * ServiceCenter)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSCA=\"%s\"\r\n", ServiceCenter);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		sprintf(Gsm.MsgServiceCenter, "%s", ServiceCenter);
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgSetTeCharacterset(char * TeCharacterSet)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSCS=\"%s\"\r\n", TeCharacterSet);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		sprintf(Gsm.MsgTeCharacterSet, "%s", TeCharacterSet);
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgGetTeCharacterset(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSCS?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		Gsm_RemoveChar((char *) Gsm.RxBuffer, '"');
		Gsm_RemoveChar((char *) Gsm.RxBuffer, ' ');
		Gsm_ReturnString(Gsm.MsgTeCharacterSet, 2, ":\r\n,");
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgRestoreSettings(uint8_t selectProfile_0_to_3)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CRES=%d\r\n", selectProfile_0_to_3);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_MsgSaveSettings(uint8_t selectProfile_0_to_3)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CSAS=%d\r\n", selectProfile_0_to_3);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
bool Gsm_SetWhiteNumberOff(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CWHITELIST=0\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_SetWhiteNumberEmpty(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CWHITELIST=3,1," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_SetWhiteNumber(uint8_t Index_1_to_30, char * PhoneNumber)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CWHITELIST=3,%d,%s\r\n", Index_1_to_30, PhoneNumber);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


//#########################################################################################################
bool Gsm_GetWhiteNumber(uint8_t Index_1_to_30, char * PhoneNumber)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	do 
	{
		if (PhoneNumber == NULL)
			break;
		strcpy(PhoneNumber, "");
		if ((Index_1_to_30 > 30) || (Index_1_to_30 == 0))
			break;
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+CWHITELIST?\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
		if (Gsm_ReturnString(PhoneNumber, Index_1_to_30, ",") == false)
			break;
		Gsm_RemoveChar(PhoneNumber, '"');
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


/*tamnd12 and lampl define function start */
/*handle Quectel error*/
bool Gsm_HandleError(uint16_t error)
{
	bool retVal = false;
	count_error++;

#if DETAILED_DEBUG
	printf("error %d \n\r", error);
#endif

	switch (error)
	{
		case SPECIFIED_SOCKET_INDEX_USED:
#if DETAILED_DEBUG
			printf("go to SPECIFIED_SOCKET_INDEX_USED\n\r");
#endif

			/*return and go to increse_connectID point*/
			retVal = true;
			break;

		case DNS_PARSE_FAILED:
#if DETAILED_DEBUG
			printf("go to DNS_PARSE_FAILED\n\r");
#endif

			Gsm_ResetPDPContext(CONTEXT_ID);
			retVal = Gsm_ConfigureDNSServer();
			break;
	}

	return retVal;
}


/*
 * check sim inserted
 */
bool Gsm_CheckSimInsertedStatus(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool retVal = false;
	uint16_t inserted_status = 0;
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+QSIMSTAT?\r\n");

#if DETAILED_DEBUG
		printf("command: %s ", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
		{
			break;
		}
		if (result == SECOND_PARAMETER)
		{
			break;
		}
		Gsm_ReturnInteger((int32_t *) &inserted_status, 1, ",");
		if (inserted_status == SIM_INSERTED)
			retVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return retVal;
}


/*
 * check network registration 
 */
bool Gsm_CheckNetworkRegistration(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool retVal = false;
	uint8_t result_command[15];
	uint16_t status_network = 0;
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+CREG?\r\n");

#if DETAILED_DEBUG
		printf("command: %s ", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
		{
			break;
		}
		if (result == SECOND_PARAMETER)
		{
			break;
		}
		sprintf((char *) result_command, "+CREG");
		if (Gsm_WaitForString(_GSM_WAIT_TIME_HIGH, &result, 1, "+CREG") == false)
			break;
		Gsm_ReturnInteger((int32_t *) &status_network, 1, ",");
		if (status_network == NETWORK_REGISTERED_HOME ||\
				status_network == NETWORK_REGISTERED_ROAMING)
			retVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return retVal;
}


/*
 * configure Address of DNS server. Hash Code DNS Google 
 */
bool Gsm_ConfigureDNSServer(void)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool retVal = false;
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+QIDNSCFG=1,\"8.8.8.8\",\"8.8.4.4\"\r\n");

#if DETAILED_DEBUG
		printf("command: %s ", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
		{
			break;
		}
		if (result == SECOND_PARAMETER)
		{
			break;
		}
		retVal = true;
		/*for test*/
		sprintf((char *) Gsm.TxBuffer, "AT+QIDNSCFG=1\r\n");
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
		{
			break;
		}
		if (result == SECOND_PARAMETER)
		{
			break;
		}
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return retVal;
}


/*
 * current state of socket
 */
int Gsm_SocketGetStatus(uint8_t connectID)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	int state = STATE_INITIAL;
	Gsm_RxClear();
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+QISTATE=1,%d\r\n", connectID);

#if DETAILED_DEBUG
		printf("command: %s ", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
		{
			state = STATE_ERROR;
			break;
		}
		if (strstr((char *) Gsm.RxBuffer, "+QISTATE") == NULL)
			break;
		Gsm_RemoveChar((char *) Gsm.RxBuffer, '"');
		Gsm_RemoveChar((char *) Gsm.RxBuffer, ' ');
		Gsm_RemoveChar((char *) Gsm.RxBuffer, '\n');
		Gsm_ReturnInteger((int32_t *) &state, 5, ",");
	}
	while(0);

#if DETAILED_DEBUG
	printf(" state: %d\n\r", state);
#endif

	osSemaphoreRelease(myBinarySem01Handle);
	return state;
}


/*
 * This function open a socket tcp on server
 */
uint16_t Gsm_SocketOpen(uint8_t connectID)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	uint8_t result_command[15];
	uint16_t returnVal = -1;
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+QIOPEN=%d,%d,\"TCP\",%s,%d,0,0\r\n", CONTEXT_ID, connectID, HOSTNAME_TCP, PORT_TCP);

#if DETAILED_DEBUG
		printf("command: %s\n\r", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
		{
			break;
		}
		if (result == SECOND_PARAMETER)
		{
			break;
		}
		else 
		{
			sprintf((char *) result_command, "+QIOPEN");
			if (Gsm_WaitForString(_GSM_WAIT_TIME_HIGH, &result, 1, result_command) == false)
				break;
			Gsm_ReturnInteger((int32_t *) &returnVal, 1, ",");
		}
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


/*
 * This function will send data
 */
bool Gsm_SocketSendData(uint8_t connectID, uint8_t * buf, uint8_t len)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	Gsm_RxClear();
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+QISEND=%d,%d\r\n", connectID, len);

#if DETAILED_DEBUG
		//printf("command: %s\n\r", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_HIGH, &result, 2, ">", "ERROR") == false)
			break;
		if (result == SECOND_PARAMETER)
			break;
		if (Gsm_SendRaw(buf, len) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_HIGH, &result, 2, "SEND OK", "ERROR") == false)
			break;
		if (result == SECOND_PARAMETER)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


/*
 * This function read data from socket
 */
uint8_t Gsm_SocketReadData(uint8_t connectID, uint8_t * buf, uint8_t length)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	/*return read byte*/
	uint8_t result = 0;
	//TODO
	do 
	{
		Gsm_RxClear();
		sprintf((char *) Gsm.TxBuffer, "AT+QIRD=%d,20\r\n", connectID);
		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_LOW, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == 2)
			break;
	}
	while(0);
	//TODO
	osSemaphoreRelease(myBinarySem01Handle);
	return result;
}


/*
 * This function close a socket connection with given connectID
 */
bool Gsm_SocketClose(uint8_t connectID)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	Gsm_RxClear();
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+QICLOSE=%d\r\n", connectID);

#if DETAILED_DEBUG
		printf("command: %s", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == SECOND_PARAMETER)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}

/*
 * This function reset PDP Context
 */
void Gsm_ResetPDPContext(uint8_t contextID)
{
	Gsm_DeActPDPContext(contextID);
	osDelay(_GSM_WAIT_TIME_LOW);	
	Gsm_ActPDPContext(contextID);
	osDelay(_GSM_WAIT_TIME_LOW);
}

/*
 * This function deactive PDP Context
 */
bool Gsm_DeActPDPContext(uint8_t contextID)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	Gsm_RxClear();
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+QIDEACT=%d\r\n", connectID);

#if DETAILED_DEBUG
		printf("command: %s", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == SECOND_PARAMETER)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}

/*
 * This function active PDP Context
 */
bool Gsm_ActPDPContext(uint8_t contextID)
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	Gsm_RxClear();
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+QIACT=%d\r\n", connectID);

#if DETAILED_DEBUG
		printf("command: %s", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_HIGH, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == SECOND_PARAMETER)
			break;
		returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}

/*
 * This function get status of PDP Context
 */
bool Gsm_GetPDPContext()
{
	osSemaphoreWait(myBinarySem01Handle, osWaitForever);
	uint8_t result;
	bool returnVal = false;
	uint16_t context_state = 0;
	Gsm_RxClear();
	do 
	{
		sprintf((char *) Gsm.TxBuffer, "AT+QIACT?\r\n");

#if DETAILED_DEBUG
		printf("command: %s", Gsm.TxBuffer);
#endif

		if (Gsm_SendString((char *) Gsm.TxBuffer) == false)
			break;
		if (Gsm_WaitForString(_GSM_WAIT_TIME_MED, &result, 2, "OK", "ERROR") == false)
			break;
		if (result == SECOND_PARAMETER)
			break;
		if (strstr((char *) Gsm.RxBuffer, "+QIACT") == NULL)
			break;
		Gsm_RemoveChar((char *) Gsm.RxBuffer, ' ');
		Gsm_ReturnInteger((int32_t *) &context_state, 1, ",");
		if (context_state == true)
			returnVal = true;
	}
	while(0);
	osSemaphoreRelease(myBinarySem01Handle);
	return returnVal;
}


/*
 * This function send a CONNECT MQTT packet to	a socket with given connectID
 */
bool Gsm_MqttConnect(uint8_t connectID, MQTTPacket_connectData * data)
{
#if DETAILED_DEBUG
	printf("Gsm_MqttConnect\n\r");
#endif

	uint8_t buf[200];
	uint8_t buflen = sizeof(buf);
	uint8_t result;
	uint8_t len;
	/* //no need to check connection status
	result = Gsm_SocketGetStatus(connectID);
	if(result != STATE_CONNECTED)
		Gsm_SocketOpen(connectID); */
	/*generate connect packet*/
	len = MQTTSerialize_connect(buf, buflen, data);
	/*send data*/
	result = Gsm_SocketSendData(connectID, buf, len);
	return result;
}


/*
 * This function send a publish packet to  a socket with given connectID
 */
bool Gsm_MqttPublish(uint8_t connectID, char * payload, MQTTPacket_connectData * data, MQTTString topicString)
{
#if DETAILED_DEBUG
	printf("Gsm_MqttPublish\n\r");
#endif

	int payloadlen = strlen(payload);
	uint8_t buf[200];
	uint8_t buflen = sizeof(buf);
	uint8_t result;
	uint8_t len;
	/* //no need to check connection status
	result = Gsm_SocketGetStatus(connectID);
	if(result != STATE_CONNECTED)
	{
		Gsm_SocketOpen(connectID);
		Gsm_MqttConnect(connectID, data);
	} */
	/*generate connect packet*/
	len = MQTTSerialize_publish(buf, buflen, 0, 0, 0, 0, topicString, (unsigned char *) payload, payloadlen);
	/*send data*/
	result = Gsm_SocketSendData(connectID, buf, len);
	return result;
}


/*
 * This function send a subscribe packet to  a socket with given connectID
 */
bool Gsm_MqttSubscribe(uint8_t connectID, MQTTPacket_connectData * data, MQTTString topicString)
{
	uint8_t buf[200];
	uint8_t buflen = sizeof(buf);
	uint8_t result;
	uint8_t len;
	int msgid = 1;
	int req_qos = 0;
	/* //no need to check connection status
	result = Gsm_SocketGetStatus(connectID);
	if(result != STATE_CONNECTED)
	{
		Gsm_SocketOpen(connectID);
		Gsm_MqttConnect(connectID, data);
	} */
	/*generate connect packet*/
	len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);
	/*send data*/
	result = Gsm_SocketSendData(connectID, buf, len);
	return result;
}


/*
 * This function send a DISCONNECT MQTT packet to  a socket with given connectID
 */
bool Gsm_MqttDisconnect(uint8_t connectID)
{
#if DETAILED_DEBUG
	printf("Gsm_MqttDisconnect\r\n");
#endif

	uint8_t buf[200];
	uint8_t buflen = sizeof(buf);
	uint8_t result;
	uint8_t len;
	result = Gsm_SocketGetStatus(connectID);
	/*if not connected, no need to send disconnect*/
	if (result == STATE_CONNECTED)
	{
		/*generate Disconnect packet*/
		len = MQTTSerialize_disconnect(buf, buflen);
		/*send data*/
		result = Gsm_SocketSendData(connectID, buf, len);
		return result;
	}
	return true;
}
/*tamnd12 and lampl define function end */


//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
//#########################################################################################################
