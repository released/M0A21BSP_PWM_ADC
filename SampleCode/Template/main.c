/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"

#include "project_config.h"


/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/
volatile uint32_t BitFlag = 0;
volatile uint32_t counter_tick = 0;

uint16_t duty_range = 50;

enum
{
	ADC0_CH0 = 0 ,
	ADC0_CH1 , 	
	ADC0_CH2 , 
	ADC0_CH3 , 
	ADC0_CH4 , 
	
	ADC0_CH5 , 
	ADC0_CH6 , 
	ADC0_CH7 , 
	ADC0_CH8 , 
	ADC0_CH9 , 
	
	ADC0_CH10 , 
	ADC0_CH11 , 
	ADC0_CH12 ,
	ADC0_CH13 , 
	ADC0_CH14 , 
	ADC0_CH15 , 
	
	ADC_CH_DEFAULT 	
}ADC_CH_TypeDef;

typedef struct
{	
	uint8_t adc_ch;
}ADC_struct;

enum{
	State_avergage = 0 ,
	State_moving ,		
	
	State_DEFAULT	
}ADC_State;

const ADC_struct adc_measure[] =
{
	{ADC0_CH12},
	{ADC0_CH13},
	{ADC0_CH14},
	// {ADC0_CH3},
	// {ADC0_CH4},
	// {ADC0_CH5},
	// {ADC0_CH6},


	{ADC_CH_DEFAULT},	
};

#define ADC_RESOLUTION							((uint16_t)(4096u))
#define ADC_REF_VOLTAGE							((uint16_t)(3300u))	//(float)(3.3f)
#define ADC_DIGITAL_SCALE(void) 				(0xFFFU >> ((0) >> (3U - 1U)))		//0: 12 BIT 
#define ADC_CALC_DATA_TO_VOLTAGE(DATA,VREF) 	((DATA) * (VREF) / ADC_DIGITAL_SCALE())

#define ADC_AVG_TRAGET 						    (8)
#define ADC_AVG_POW	 					        (3)
#define ADC_CH_NUM	 						    (3) // (7)

volatile uint16_t ADC_TargetChannel = 0;
volatile uint16_t ADC_Datax = 0;
uint32_t AVdd = 0;
uint16_t ADC_DataArray[ADC_CH_NUM] = {0};


#define PWM0_CLK0_FREQ                          (1000)
#define PWM0_CLK1_FREQ                          (2000)
#define PWM0_CLK2_FREQ                          (3000)

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

void tick_counter(void)
{
	counter_tick++;
}

uint32_t get_tick(void)
{
	return (counter_tick);
}

void set_tick(uint32_t t)
{
	counter_tick = t;
}

void compare_buffer(uint8_t *src, uint8_t *des, int nBytes)
{
    uint16_t i = 0;	
	
    for (i = 0; i < nBytes; i++)
    {
        if (src[i] != des[i])
        {
            printf("error idx : %4d : 0x%2X , 0x%2X\r\n", i , src[i],des[i]);
			set_flag(flag_error , ENABLE);
        }
    }

	if (!is_flag_set(flag_error))
	{
    	printf("%s finish \r\n" , __FUNCTION__);	
		set_flag(flag_error , DISABLE);
	}

}

void reset_buffer(void *dest, unsigned int val, unsigned int size)
{
    uint8_t *pu8Dest;
//    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;

	#if 1
	while (size-- > 0)
		*pu8Dest++ = val;
	#else
	memset(pu8Dest, val, size * (sizeof(pu8Dest[0]) ));
	#endif
	
}

void copy_buffer(void *dest, void *src, unsigned int size)
{
    uint8_t *pu8Src, *pu8Dest;
    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;
    pu8Src  = (uint8_t *)src;


	#if 0
	  while (size--)
	    *pu8Dest++ = *pu8Src++;
	#else
    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
	#endif
}

void dump_buffer(uint8_t *pucBuff, int nBytes)
{
    uint16_t i = 0;
    
    printf("dump_buffer : %2d\r\n" , nBytes);    
    for (i = 0 ; i < nBytes ; i++)
    {
        printf("0x%2X," , pucBuff[i]);
        if ((i+1)%8 ==0)
        {
            printf("\r\n");
        }            
    }
    printf("\r\n\r\n");
}

void  dump_buffer_hex(uint8_t *pucBuff, int nBytes)
{
    int     nIdx, i;

    nIdx = 0;
    while (nBytes > 0)
    {
        printf("0x%04X  ", nIdx);
        for (i = 0; i < 16; i++)
            printf("%02X ", pucBuff[nIdx + i]);
        printf("  ");
        for (i = 0; i < 16; i++)
        {
            if ((pucBuff[nIdx + i] >= 0x20) && (pucBuff[nIdx + i] < 127))
                printf("%c", pucBuff[nIdx + i]);
            else
                printf(".");
            nBytes--;
        }
        nIdx += 16;
        printf("\n");
    }
    printf("\n");
}

void delay(uint16_t dly)
{
/*
	delay(100) : 14.84 us
	delay(200) : 29.37 us
	delay(300) : 43.97 us
	delay(400) : 58.5 us	
	delay(500) : 73.13 us	
	
	delay(1500) : 0.218 ms (218 us)
	delay(2000) : 0.291 ms (291 us)	
*/

	while( dly--);
}


void delay_ms(uint16_t ms)
{
	TIMER_Delay(TIMER0, 1000*ms);
}

__STATIC_INLINE uint32_t FMC_ReadBandGap(void)
{
    FMC->ISPCMD = FMC_ISPCMD_READ_UID;            /* Set ISP Command Code */
    FMC->ISPADDR = 0x70u;                         /* Must keep 0x70 when read Band-Gap */
    FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk;           /* Trigger to start ISP procedure */
#if ISBEN
    __ISB();
#endif                                            /* To make sure ISP/CPU be Synchronized */
    while(FMC->ISPTRG & FMC_ISPTRG_ISPGO_Msk) {}  /* Waiting for ISP Done */

    return FMC->ISPDAT & 0xFFF;
}

void ADC_IRQHandler(void)
{
    set_flag(flag_ADC_Data_Ready,ENABLE);

    // ADC_Datax = ADC_GET_CONVERSION_DATA(ADC, ADC_TargetChannel);

    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT); /* Clear the A/D interrupt flag */
}

void ADC_ReadAVdd(void)
{
    int32_t  i32ConversionData;
    int32_t  i32BuiltInData;

    ADC_Open(ADC, ADC_ADCR_DIFFEN_SINGLE_END, ADC_ADCR_ADMD_SINGLE, BIT29);
    
    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);
    ADC_ENABLE_INT(ADC, ADC_ADF_INT);
    NVIC_EnableIRQ(ADC_IRQn);
    
    CLK_SysTickDelay(10000);

    set_flag(flag_ADC_Data_Ready,DISABLE);
    ADC_START_CONV(ADC);
    while(!is_flag_set(flag_ADC_Data_Ready) == FALSE);
    ADC_DISABLE_INT(ADC, ADC_ADF_INT);
		
    i32ConversionData = ADC_GET_CONVERSION_DATA(ADC, 29);

    SYS_UnlockReg();
    FMC_Open();
    i32BuiltInData = FMC_ReadBandGap();	

	AVdd = 3072*i32BuiltInData/i32ConversionData;

	printf("%s : %4d,%4d,%4d\r\n",__FUNCTION__,AVdd, i32ConversionData,i32BuiltInData);

    NVIC_DisableIRQ(ADC_IRQn);
	
}

void ADC_InitChannel(uint8_t ch)
{
	set_flag(flag_ADC_Data_Ready,DISABLE);

    /* Set input mode as single-end, and Single mode*/
    ADC_Open(ADC, ADC_ADCR_DIFFEN_SINGLE_END, ADC_ADCR_ADMD_SINGLE,(uint32_t) 0x1 << ch);

    /* Select ADC input channel */
    // ADC_SET_INPUT_CHANNEL(ADC, 0x1 << ch);

	ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);
	ADC_ENABLE_INT(ADC, ADC_ADF_INT);
	NVIC_EnableIRQ(ADC_IRQn);

    /* Start ADC conversion */
    ADC_START_CONV(ADC);
}


void ADC_Process(uint8_t state)
{
	uint8_t idx = 0;
	volatile	uint32_t sum = 0;
	uint16_t tmp = 0;
	
	switch(state)
	{
		case State_avergage:	
			for ( idx = 0 ; idx < ADC_CH_NUM ; idx++)
			{
				ADC_TargetChannel = adc_measure[idx].adc_ch;
				for ( tmp = 0 ; tmp < ADC_AVG_TRAGET ; tmp++)
				{
					ADC_InitChannel(ADC_TargetChannel);
					while(!is_flag_set(flag_ADC_Data_Ready));
					ADC_DISABLE_INT(ADC, ADC_ADF_INT);

                    ADC_Datax = ADC_GET_CONVERSION_DATA(ADC, ADC_TargetChannel);

					sum += ADC_Datax;											//sum the first 8 ADC data
				}
				ADC_DataArray[idx] = (uint16_t) (sum >> ADC_AVG_POW);			//do average
			}

			break;

		case State_moving:
			for ( idx = 0 ; idx < ADC_CH_NUM ; idx++)
			{
				ADC_TargetChannel = adc_measure[idx].adc_ch;
				ADC_InitChannel(ADC_TargetChannel);
				while(!is_flag_set(flag_ADC_Data_Ready));
				ADC_DISABLE_INT(ADC, ADC_ADF_INT);

                ADC_Datax = ADC_GET_CONVERSION_DATA(ADC, ADC_TargetChannel);

				sum = ADC_DataArray[idx] << ADC_AVG_POW;					    //extend the original average data
				sum -= ADC_DataArray[idx];									    //subtract the old average data
				sum += ADC_Datax;												//add the new adc data
				ADC_DataArray[idx] = (uint16_t) (sum >> ADC_AVG_POW);		    //do average again
			}

			#if 1	// debug
            printf("AVdd:%4dmv," , AVdd);
			for ( idx = 0 ; idx < ADC_CH_NUM ; idx++)
			{
				tmp = ADC_DataArray[idx];
				// convertDecToBin(tmp);//ADC_DataArray[idx]
				// printf("%d:%4dmv," , idx ,ADC_CALC_DATA_TO_VOLTAGE(ADC_DataArray[idx],ADC_REF_VOLTAGE));
				// printf("%d:%3X,%4d ," , idx ,ADC_DataArray[idx],ADC_CALC_DATA_TO_VOLTAGE(ADC_DataArray[idx],ADC_REF_VOLTAGE));
				// printf("%d:0x%3X," , 4 , ADC_DataArray[idx]);
				// printf("0x%3X:%4d ," , tmp ,ADC_CALC_DATA_TO_VOLTAGE(tmp,ADC_REF_VOLTAGE));
				printf("ch %2d:0x%3X(hex),%4d(mv)," , ADC_TargetChannel , tmp ,ADC_CALC_DATA_TO_VOLTAGE(tmp,AVdd));         
				// printf("%2X:%2X ," , adc_measure[idx].adc_ch,ADC_DataArray[idx]);
				
				if (idx == (ADC_CH_NUM -1) )
				{
					printf("\r\n");
				}				
			}
			#endif	
			
			break;		
	}	
}


void PWM_Out_Change_Duty(uint8_t ch , uint16_t duty)	
{
    switch(ch)
    {
        case 0:
            PWM_ConfigOutputChannel(PWM0, 0, PWM0_CLK0_FREQ , duty);   
            break;
        case 1:
            PWM_ConfigOutputChannel(PWM0, 1, PWM0_CLK0_FREQ , duty);
           break;
        case 2:
            PWM_ConfigOutputChannel(PWM0, 2, PWM0_CLK1_FREQ , duty);   
            break;
        case 3:
            PWM_ConfigOutputChannel(PWM0, 3, PWM0_CLK1_FREQ , duty);
            break;
        case 4:
            PWM_ConfigOutputChannel(PWM0, 4, PWM0_CLK2_FREQ , duty);
            break;
        case 5:
            PWM_ConfigOutputChannel(PWM0, 5, PWM0_CLK2_FREQ , duty);
            break; 
    }

}

/* 
    PWM output for test
    PC6 , PWM0_CH0 , 
    PC7 , PWM0_CH1 , 

    PC2 , PWM0_CH2 , 
    PC1 , PWM0_CH3 , 

    PC0 , PWM0_CH4 , 
    PA2 , PWM0_CH5 , 

*/
void PWM_Out_Init(uint8_t ch)	
{
    switch(ch)
    {
        case 0:
            PWM_ConfigOutputChannel(PWM0, 0, PWM0_CLK0_FREQ , 0);
            PWM_EnableOutput(PWM0, PWM_CH_0_MASK);
            PWM_Start(PWM0, PWM_CH_0_MASK);        
            break;
        case 1:
            PWM_ConfigOutputChannel(PWM0, 1, PWM0_CLK0_FREQ , 0);
            PWM_EnableOutput(PWM0, PWM_CH_1_MASK);
            PWM_Start(PWM0, PWM_CH_1_MASK);        
            break;
        case 2:
            PWM_ConfigOutputChannel(PWM0, 2, PWM0_CLK1_FREQ , 0);
            PWM_EnableOutput(PWM0, PWM_CH_2_MASK);
            PWM_Start(PWM0, PWM_CH_2_MASK);        
            break;
        case 3:
            PWM_ConfigOutputChannel(PWM0, 3, PWM0_CLK1_FREQ , 0);
            PWM_EnableOutput(PWM0, PWM_CH_3_MASK);
            PWM_Start(PWM0, PWM_CH_3_MASK);        
            break;
        case 4:
            PWM_ConfigOutputChannel(PWM0, 4, PWM0_CLK2_FREQ , 0);
            PWM_EnableOutput(PWM0, PWM_CH_4_MASK);
            PWM_Start(PWM0, PWM_CH_4_MASK);        
            break;
        case 5:
            PWM_ConfigOutputChannel(PWM0, 5, PWM0_CLK2_FREQ , 0);
            PWM_EnableOutput(PWM0, PWM_CH_5_MASK);
            PWM_Start(PWM0, PWM_CH_5_MASK);        
            break; 
    }
}

void GPIO_Init (void)
{
    // SYS->GPA_MFP0 = (SYS->GPA_MFP0 & ~(SYS_GPA_MFP0_PA2MFP_Msk)) | (SYS_GPA_MFP0_PA2MFP_GPIO);
    // SYS->GPC_MFP0 = (SYS->GPC_MFP0 & ~(SYS_GPC_MFP0_PC2MFP_Msk)) | (SYS_GPC_MFP0_PC2MFP_GPIO);
		
	// // EVM LED_R
    // GPIO_SetMode(PA, BIT2, GPIO_MODE_OUTPUT);

	// // EVM button
    // GPIO_SetMode(PC, BIT2, GPIO_MODE_OUTPUT);	
}


void TMR1_IRQHandler(void)
{
	static uint32_t LOG = 0;

	
    if(TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
		tick_counter();

		if ((get_tick() % 1000) == 0)
		{
        	printf("%s : %4d\r\n",__FUNCTION__,LOG++);
			// PA2 ^= 1;
		}

		if ((get_tick() % 50) == 0)
		{

		}	
    }
}


void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);	
    TIMER_Start(TIMER1);
}

void UARTx_Process(void)
{
	uint8_t res = 0;
	res = UART_READ(UART0);

	if (res == 'x' || res == 'X')
	{
		NVIC_SystemReset();
	}

	if (res > 0x7F)
	{
		printf("invalid command\r\n");
	}
	else
	{
		switch(res)
		{
			case '0':
                PWM_Out_Change_Duty(0 , duty_range);
                printf("PWM CH %d : %2d\r\n" , res-0x30 , duty_range);
				break;
			case '1':
                PWM_Out_Change_Duty(1 , duty_range);
                printf("PWM CH %d : %2d\r\n" , res-0x30 , duty_range);
				break;
			case '2':
                PWM_Out_Change_Duty(2 , duty_range);
                printf("PWM CH %d : %2d\r\n" , res-0x30 , duty_range);
				break;
			case '3':
                PWM_Out_Change_Duty(3 , duty_range);
                printf("PWM CH %d : %2d\r\n" , res-0x30 , duty_range);
				break;
			case '4':
                PWM_Out_Change_Duty(4 , duty_range);
                printf("PWM CH %d : %2d\r\n" , res-0x30 , duty_range);
				break;
			case '5':
                PWM_Out_Change_Duty(5 , duty_range);
                printf("PWM CH %d : %2d\r\n" , res-0x30 , duty_range);
				break;


			case 'a':
			case 'A':
                duty_range = (duty_range >= 100) ? (100) : (duty_range + 10) ;
                printf("duty change : %2d\r\n" , duty_range);
				break;

			case 'd':
			case 'D': 
                duty_range = (duty_range <= 10) ? (10) : (duty_range - 10) ;
                printf("duty change : %2d\r\n" , duty_range);                       
				break;

			case 'X':
			case 'x':
			case 'Z':
			case 'z':
				NVIC_SystemReset();		
				break;
		}
	}
}

void UART0_IRQHandler(void)
{

    if(UART_GET_INT_FLAG(UART0, UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk))     /* UART receive data available flag */
    {
        while(UART_GET_RX_EMPTY(UART0) == 0)
        {
            UARTx_Process();
        }
    }

    if(UART0->FIFOSTS & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk))
    {
        UART_ClearIntFlag(UART0, (UART_INTSTS_RLSINT_Msk| UART_INTSTS_BUFERRINT_Msk));
    }	
}

void UART0_Init(void)
{
    SYS_ResetModule(UART0_RST);

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(UART0, 115200);
    UART_EnableInt(UART0, UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk);
    NVIC_EnableIRQ(UART0_IRQn);
	
	#if (_debug_log_UART_ == 1)	//debug
	printf("\r\nCLK_GetCPUFreq : %8d\r\n",CLK_GetCPUFreq());
	printf("CLK_GetHXTFreq : %8d\r\n",CLK_GetHXTFreq());
	printf("CLK_GetLXTFreq : %8d\r\n",CLK_GetLXTFreq());	
	printf("CLK_GetPCLK0Freq : %8d\r\n",CLK_GetPCLK0Freq());
	printf("CLK_GetPCLK1Freq : %8d\r\n",CLK_GetPCLK1Freq());	
	#endif	

}

void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

//	CLK_EnableXtalRC(CLK_PWRCTL_LIRCEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk);	

//	CLK_EnableXtalRC(CLK_PWRCTL_LXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);	

    /* Select HCLK clock source as HIRC and HCLK source divider as 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));

    CLK_EnableModuleClock(TMR1_MODULE);
  	CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HIRC, 0);

    CLK_EnableModuleClock(PWM0_MODULE);
    SYS_ResetModule(PWM0_RST);

    CLK_EnableModuleClock(ADC_MODULE);
    /* Switch ADC clock source to HIRC, set divider to 2, ADC clock is 48/2 MHz */
    CLK_SetModuleClock(ADC_MODULE, CLK_CLKSEL2_ADCSEL_PCLK1, CLK_CLKDIV0_ADC(2));    

    /* Set PB multi-function pins for UART0 RXD=PB.6 and TXD=PB.4 */
    SYS->GPB_MFP1 = (SYS->GPB_MFP1 & ~(SYS_GPB_MFP1_PB4MFP_Msk | SYS_GPB_MFP1_PB6MFP_Msk)) |        \
                    (SYS_GPB_MFP1_PB4MFP_UART0_TXD | SYS_GPB_MFP1_PB6MFP_UART0_RXD);

    /*
        PC6 , PWM0_CH0 , 
        PC7 , PWM0_CH1 , 

        PC2 , PWM0_CH2 , 
        PC1 , PWM0_CH3 , 

        PC0 , PWM0_CH4 , 
        PA2 , PWM0_CH5 , 
    */
    SYS->GPC_MFP1 = (SYS->GPC_MFP1 & ~(SYS_GPC_MFP1_PC6MFP_Msk | SYS_GPC_MFP1_PC7MFP_Msk )) |
                    (SYS_GPC_MFP1_PC6MFP_PWM0_CH0 | SYS_GPC_MFP1_PC7MFP_PWM0_CH1);

    SYS->GPC_MFP0 = (SYS->GPC_MFP0 & ~(SYS_GPC_MFP0_PC2MFP_Msk | SYS_GPC_MFP0_PC1MFP_Msk | SYS_GPC_MFP0_PC0MFP_Msk)) |
                    (SYS_GPC_MFP0_PC2MFP_PWM0_CH2 | SYS_GPC_MFP0_PC1MFP_PWM0_CH3 |SYS_GPC_MFP0_PC0MFP_PWM0_CH4);


    SYS->GPA_MFP0 = (SYS->GPA_MFP0 & ~(SYS_GPA_MFP0_PA2MFP_Msk )) |
                    (SYS_GPA_MFP0_PA2MFP_PWM0_CH5 );

    /*
        PC.3 : ADC0_CH12
        PC.4 : ADC0_CH13 
        PC.5 : ADC0_CH14
    */
    /* Set PC.3 , PC.4 , PC.5 to input mode */
    GPIO_SetMode(PC, BIT3|BIT4|BIT5, GPIO_MODE_INPUT);

    /* Configure the PC.3 , PC.4 , PC.5 ADC analog input pins.  */
    SYS->GPC_MFP0 = (SYS->GPC_MFP0 & ~(SYS_GPC_MFP0_PC3MFP_Msk )) |
                    (SYS_GPC_MFP0_PC3MFP_ADC0_CH12 );

    SYS->GPC_MFP1 = (SYS->GPC_MFP1 & ~(SYS_GPC_MFP1_PC4MFP_Msk| SYS_GPC_MFP1_PC5MFP_Msk)) |
                    (SYS_GPC_MFP1_PC4MFP_ADC0_CH13 | SYS_GPC_MFP1_PC5MFP_ADC0_CH14);

    /* Disable the PC.3 , PC.4 , PC.5 digital input path to avoid the leakage current. */
    GPIO_DISABLE_DIGITAL_PATH(PC, BIT3|BIT4|BIT5);

   /* Update System Core Clock */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}

/*
 * This is a template project for M031 series MCU. Users could based on this project to create their
 * own application without worry about the IAR/Keil project settings.
 *
 * This template application uses external crystal as HCLK source and configures UART0 to print out
 * "Hello World", users may need to do extra system configuration based on their system design.
 */

int main()
{
    SYS_Init();

	UART0_Init();
	GPIO_Init();
	TIMER1_Init();

    PWM_Out_Init(0);
    PWM_Out_Init(1);
    PWM_Out_Init(2);
    PWM_Out_Init(3);
    PWM_Out_Init(4);
    PWM_Out_Init(5);

    ADC_POWER_ON(ADC);
    CLK_SysTickDelay(10000);

	ADC_ReadAVdd();
    ADC_Process(State_avergage);

    /* Got no where to go, just loop forever */
    while(1)
    {
        ADC_Process(State_moving);

    }
}

/*** (C) COPYRIGHT 2017 Nuvoton Technology Corp. ***/
