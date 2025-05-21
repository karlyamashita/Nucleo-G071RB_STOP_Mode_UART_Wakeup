/*
 * PollingRoutine.c
 *
 *  Created on: Oct 24, 2023
 *      Author: karl.yamashita
 *
 *
 *      Template for projects.
 *
 *      The object of this PollingRoutine.c/h files is to not have to write code in main.c which already has a lot of generated code.
 *      It is cumbersome having to scroll through all the generated code for your own code and having to find a USER CODE section so your code is not erased when CubeMX re-generates code.
 *      
 *      Direction: Call PollingInit before the main while loop. Call PollingRoutine from within the main while loop
 * 
 *      Example;
        // USER CODE BEGIN WHILE
        PollingInit();
        while (1)
        {
            PollingRoutine();
            // USER CODE END WHILE

            // USER CODE BEGIN 3
        }
        // USER CODE END 3

 */


#include "main.h"


const char version[] = "STOP Mode 1.0.1";


#define UART2_DMA_RX_QUEUE_SIZE 10 // queue size
#define UART2_DMA_TX_QUEUE_SIZE 4
UART_DMA_Data uart2_dmaDataRxQueue[UART2_DMA_RX_QUEUE_SIZE] = {0};
UART_DMA_Data uart2_dmaDataTxQueue[UART2_DMA_TX_QUEUE_SIZE] = {0};

UART_DMA_Struct_t uart2_msg =
{
	.huart = &huart2,
	.rx.queueSize = UART2_DMA_RX_QUEUE_SIZE,
	.rx.msgQueue = uart2_dmaDataRxQueue,
	.tx.queueSize = UART2_DMA_TX_QUEUE_SIZE,
	.tx.msgQueue = uart2_dmaDataTxQueue,
	.dma.dmaPtr.SkipOverFlow = true
};

char msg_copy_command[UART_DMA_QUEUE_DATA_SIZE] = {0};


void PollingInit(void)
{

	UART_DMA_EnableRxInterruptIdle(&uart2_msg);

	TimerCallbackRegisterOnly(&timerCallback, LED_Toggle);
	TimerCallbackTimerStart(&timerCallback, LED_Toggle, 500, TIMER_REPEAT);

	TimerCallbackRegisterOnly(&timerCallback, EnterStopMode);
}

void PollingRoutine(void)
{
	TimerCallbackPoll(&timerCallback);

	UART_DMA_ParseCircularBuffer(&uart2_msg);

	UART_CheckForNewMessage(&uart2_msg);
}

void UART_CheckForNewMessage(UART_DMA_Struct_t *msg)
{
	int status = 0;
	char *ptr;
	char retStr[UART_DMA_QUEUE_DATA_SIZE] = "OK";

	if(UART_DMA_RxMsgRdy(msg))
	{
		memset(&msg_copy_command, 0, sizeof(msg_copy_command));// clear
		memcpy(&msg_copy_command, msg->rx.msgToParse->data, strlen((char*)msg->rx.msgToParse->data) - 2); // remove CR/LF

		ptr = (char*)msg->rx.msgToParse->data;
		RemoveSpaces(ptr);
		ToLower(ptr);

		if(strncmp(ptr, "version", strlen("version")) == 0)
		{
			sprintf(retStr, version);
		}
		else if(strncmp(ptr, "stopmode", strlen("stopmode")) == 0)
		{
			TimerCallbackTimerStart(&timerCallback, EnterStopMode, 1000, TIMER_NO_REPEAT);
		}
		else if(strncmp(ptr, "blinkrate:", strlen("blinkrate:")) == 0)
		{
			ptr += strlen("blinkrate:");
			TimerCallbackTimerStart(&timerCallback, LED_Toggle, atoi(ptr), TIMER_REPEAT);
		}
		else
		{
			status = COMMAND_UNKNOWN;
		}

		// check return status
		if(status == NO_ACK)
		{
			return;
		}
		else if(status != 0) // other return status other than NO_ACK or NO_ERROR
		{
			PrintError(msg, msg_copy_command, status);
		}
		else // NO_ERROR
		{
			PrintReply(msg, msg_copy_command, retStr);
		}

		memset(&msg->rx.msgToParse->data, 0, UART_DMA_QUEUE_DATA_SIZE); // clear current buffer index
	}
}

void PrintError(UART_DMA_Struct_t *msg, char *msg_copy, uint32_t error)
{
	char str[64] = {0};

	GetErrorString(error, str);

	strcat(msg_copy, "=");
	strcat(msg_copy, str);

	UART_DMA_NotifyUser(msg, msg_copy, strlen(msg_copy), true);
}

/*
 * Description: Returns the original message + string arguments
 * Input: DMA message data structure, the original string message, the string to add to the original message
 * Output: none
 * Return: none
 */
void PrintReply(UART_DMA_Struct_t *msg, char *msg_copy, char *msg2)
{
	strcat(msg_copy, "=");
	strcat(msg_copy, msg2);

	UART_DMA_NotifyUser(msg, msg_copy, strlen(msg_copy), true);
}


void EnterStopMode(void)
{
	UART_WakeUpTypeDef WakeUpSelection = {0};

	HAL_SuspendTick();

	WakeUpSelection.WakeUpEvent = UART_WAKEUP_ON_STARTBIT; // UART_WAKEUP_ON_READDATA_NONEMPTY
	if (HAL_UARTEx_StopModeWakeUpSourceConfig(&huart2, WakeUpSelection) != HAL_OK)
	{
		Error_Handler();
	}

	__HAL_UART_ENABLE_IT(&huart2, UART_IT_WUF);

	HAL_UARTEx_EnableStopMode(&huart2);

	HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);

	HAL_UARTEx_DisableStopMode(&huart2);

	RunClockConfig(); // calls SystemClock_Config()

	HAL_ResumeTick();
}

void LED_Toggle(void)
{
	HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
}
