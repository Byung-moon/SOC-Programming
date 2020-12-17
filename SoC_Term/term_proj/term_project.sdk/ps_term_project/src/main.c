#include "xil_types.h"
#include "xparameters.h"
#include "xiicps.h"
#include "seven_seg.h"
#include "textlcd.h"
#include "xil_io.h"
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#include "xil_exception.h"
#include "xscugic.h"
#include "pushbutton.h"
//
#include "xil_cache.h"
#include "xil_printf.h"
#include "xscutimer.h"
#include "xscutimer.h"

#define	IIC_SLAVE_ADDR	0x51
#define	IIC_SCLK_RATE	100000
//
#define INTC_DEVICE_ID		XPAR_SCUGIC_0_DEVICE_ID
#define INTC_DEVICE_INT_ID	31
//
#define SEC		0x02
#define MIN		0x03
#define HOUR	0x04

#define onesec 63000000

#define LED_mReadReg(BaseAddress, RegOffset) \
	Xil_In32((BaseAddress) + (RegOffset))

#define LED_mWriteReg(BaseAddress, RegOffset, Data) \
	Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

#define XPAR_LED_0_DEVICE_ID 0
#define XPAR_LED_0_S00_AXI_BASEADDR 0x43C30000
#define XPAR_LED_0_S00_AXI_HIGHADDR 0x43C3FFFF

int WriteRTC(XIicPs Iic, u8 *SendBuffer, char set, char val);
int	ReadRTC(XIicPs Iic, u8 *SendBuffer, u8 *RecvBuffer);
char bin2ascii(char bin);
void WriteTLCDReg(char *pRegVal, int val);

int GicConfigure(u16 DeviceId);
void ServiceRoutine(void *CallbackRef); //
int SetUpInterruptSystem(XScuGic *XScuGicInstancePtr);

XScuGic InterruptController; 	     // Instance of the Interrupt Controller
static XScuGic_Config *GicConfig;    // The configuration parameters of the controller

void clk_set(XIicPs	Iic, u8* WriteBuffer);
void clock(XIicPs	Iic, u8* SendBuffer, u8* RecvBuffer);
void update(XIicPs	Iic, u8* WriteBuffer);

void timer();
void timer_set();
void timer_on();
void timer_off();
void timer_clear();
void timer_de();

void alarm();
void alarm_set();
void alarm_on();
void alarm_off();
char alarm_check(XIicPs	Iic, u8* SendBuffer, u8* RecvBuffer);
void alarm_clear();

void bit_up();
void bit_down();

void bit_up_timer();
void bit_down_timer();

void bit_up_alarm();
void bit_down_alarm();

volatile static int InterruptProcessed = FALSE;

////////////////////////////////////////////////////
char mode=0;//mode =0 : clock      mode=1 : timer	mode=2 : Alarm
///////////////////////////
volatile char set_time = 0;
volatile char hour = 0;
volatile char minute = 0;
volatile char second = 0;
char set_where = 1;
char clk_update=0;;
/////////////////////////
volatile char set_timer = 0;
volatile char timer_hour = 0;
volatile char timer_minute = 0;
volatile char timer_second = 0;
char set_where_timer = 1;
char timer_onoff=0;
/////////////////////////////
volatile char set_alarm = 0;
volatile char alarm_hour = 0;
volatile char alarm_minute = 0;
volatile char alarm_second = 0;
char set_where_alarm = 1;
char alarm_onoff=0;
///////////////////////////////
unsigned int delay=onesec;


int main(void)
{
	XIicPs	Iic;			/**< Instance of the IIC Device */


	u8		*SendBuffer;	/**< Buffer for Transmitting Data */
	u8		RecvBuffer[3];	/**< Buffer for Receiving Data */

	///////////////
	int Status;
	///////////////
	Status = GicConfigure(INTC_DEVICE_ID);

	if (Status != XST_SUCCESS) {
		xil_printf("GIC Configure Failed\r\n");
		return XST_FAILURE;
	}
	//////////////////


	while(TRUE)
	{
		if(alarm_onoff==1){
			if(alarm_check(Iic, SendBuffer,RecvBuffer)==1){
			mode=2;
			set_alarm=3;
			alarm_onoff=0;
			LED_mWriteReg(XPAR_LED_0_S00_AXI_BASEADDR,0,1); //Alarming Wake up
			}
		}

		if(mode==0){
			u8 *WriteBuffer = (u8*)malloc(sizeof(u8));

			if(set_time==0){

				if(clk_update==1)	update(Iic, WriteBuffer);

				clock(Iic, SendBuffer,RecvBuffer);
			}

			else if(set_time==1)
			{
				clk_set(Iic, WriteBuffer);
			}

			free(WriteBuffer);

		}
		else if(mode==1){

			if(set_timer==0){
				timer();
			}

			else if(set_timer==1){
				timer_set();

			}
			else if(set_timer==2){

				timer_on();

			}
			else if(set_timer==3){
				timer_off();
			}

		}
		else if(mode==2){
			if(set_alarm==0){

				alarm();

			}
			else if(set_alarm==1){

				alarm_set();

			}
			else if(set_alarm==2){
				alarm_on();

			}
			else if(set_alarm==3){
				alarm_off();

			}

		}

		for(int wait = 0; wait < 1200; wait++);
	}
}

void WriteTLCDReg(char *pRegVal, int val)
{
	int		i = 0;
	char	temp;

	for(i = 0; i < 8; i++){

		temp = bin2ascii((val>>(28-4*i))&0x0F);
		pRegVal[i] = temp;
		pRegVal[i+8] = 0x20;	//0x20 = space
	}
}

char bin2ascii(char bin)
{
	////////////////////
	return (0x30|bin);
	////////////////////
}

int WriteRTC(XIicPs Iic, u8 *WriteBuffer, char set, char val)

{
	int Status;
	XIicPs_Config *Config;

	Config = XIicPs_LookupConfig(XPAR_XIICPS_0_DEVICE_ID);
	if(Config == NULL)
	{
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if(Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);
	
	WriteBuffer[0] = set;
	WriteBuffer[1] = val;

	Status = XIicPs_MasterSendPolled(&Iic, WriteBuffer, 2, IIC_SLAVE_ADDR);

	if(Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	while(XIicPs_BusIsBusy(&Iic))
	{
		/* NOP */
	}

	return XST_SUCCESS;
}

int ReadRTC(XIicPs Iic, u8 *SendBuffer, u8 *RecvBuffer)
{
	int				Status;
	XIicPs_Config	*Config;

	Config = XIicPs_LookupConfig(XPAR_XIICPS_0_DEVICE_ID);
	if (Config == NULL)
	{
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);

	*SendBuffer		= 0x02;
	RecvBuffer[0]	= 0x00;
	RecvBuffer[1]	= 0x00;
	RecvBuffer[2]	= 0x00;

	Status = XIicPs_MasterSendPolled(&Iic, SendBuffer, 1, IIC_SLAVE_ADDR);
	if(Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	while(XIicPs_BusIsBusy(&Iic))
	{
		/* NOP */
	}

	Status = XIicPs_MasterRecvPolled(&Iic, RecvBuffer, 3, IIC_SLAVE_ADDR);
	if (Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

int GicConfigure(u16 DeviceId)
{
	int Status;

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	GicConfig = XScuGic_LookupConfig(DeviceId);
	if (NULL == GicConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(&InterruptController, GicConfig,
					GicConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XScuGic_SelfTest(&InterruptController);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

	/*
	 * Setup the Interrupt System
	 */
	Status = SetUpInterruptSystem(&InterruptController);

		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}


	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the ARM processor.
	 */
	//Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
	//		(Xil_ExceptionHandler) XScuGic_InterruptHandler,
	//		&InterruptController);

	/*
	 * Enable interrupts in the ARM
	 */

	//Xil_ExceptionEnable();

	/*
	 * Connect a device driver handler that will be called when an
	 * interrupt for the device occurs, the device driver handler performs
	 * the specific interrupt processing for the device
	 */
	Status = XScuGic_Connect(&InterruptController, INTC_DEVICE_INT_ID,
			   (Xil_ExceptionHandler)ServiceRoutine,
			   (void *)&InterruptController);

	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the device and then cause (simulate) an
	 * interrupt so the handlers will be called
	 */
	XScuGic_Enable(&InterruptController, INTC_DEVICE_INT_ID);

	return XST_SUCCESS;
}

int SetUpInterruptSystem(XScuGic *XScuGicInstancePtr)
{
	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the ARM processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler) XScuGic_InterruptHandler,
			XScuGicInstancePtr);
	/*
	 * Enable interrupts in the ARM
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

void ServiceRoutine(void *CallbackRef)
{
	char pb;

	pb = PUSHBUTTON_mReadReg(XPAR_PUSHBUTTON_0_S00_AXI_BASEADDR, 0);

	if(pb == 1)
	{
		if(mode==0){
			set_time++;
			if(set_time>1){
				set_time = 0;
				clk_update=1;
			}
		}
		else if(mode==1){
			if((timer_onoff==0)&&(set_timer!=3)){
				set_timer++;
				if(set_timer>1){
					set_timer = 0;
				}
			}
		}
		else if(mode==2){
			if((alarm_onoff==0)&&(set_alarm!=3)){
				set_alarm++;
				if(set_alarm>1){
					set_alarm = 0;
				}
			}
		}
	}

	else if(pb == 2)
	{
		if(mode==0){
			if(set_time==1){
				set_where++;
				if(set_where>3)	set_where = 1;
			}
			else if(set_time==0){
				mode=1;
			}
		}
		else if(mode==1){
			if(timer_onoff==0){
				if(set_timer==1){
					set_where_timer++;
					if(set_where_timer>3)	set_where_timer = 1;
				}
				else if(set_timer==0){
					mode=2;
				}
			}
		}
		else if(mode==2){
			if(alarm_onoff==0){
				if(set_alarm==1){
					set_where_alarm++;
					if(set_where_alarm>3)	set_where_alarm = 1;
				}
				else if(set_alarm==0){
					mode=0;
				}
			}
			else if(alarm_onoff==1){
				if(set_alarm==2){
					mode=0;
				}
			}

		}
	}

	else if(pb == 4)
	{

		if(mode==0){
			if(set_time == 1)
			{
				bit_up();
			}
		}
		else if(mode==1){
			if(set_timer == 1){

				bit_up_timer();
			}
			else if(set_timer==0){
				if(timer_onoff==0){
					timer_onoff=1;
					set_timer=2;
				}
			}
			else if(set_timer==2){
				if(timer_onoff==1){
					timer_onoff=0;
					set_timer=0;
				}

			}
			else if(set_timer==3){
				if(timer_onoff==0){
					set_timer=0;
					LED_mWriteReg(XPAR_LED_0_S00_AXI_BASEADDR,0,0);
					//Timer off
				}
			}

		}
		else if(mode==2){
			if(set_alarm == 1){

				bit_up_alarm();
			}
			else if(set_alarm==0){
				if(alarm_onoff==0){
					alarm_onoff=1;
					set_alarm=2;
					//Alarm mode on
				}
			}
			else if(set_alarm==2){
				if(alarm_onoff==1){
					alarm_onoff=0;
					set_alarm=0;
					//Alarm mode off
				}

			}
			else if(set_alarm==3){
				if(alarm_onoff==0){
					set_alarm=0;
					LED_mWriteReg(XPAR_LED_0_S00_AXI_BASEADDR,0,0);
					//Alarming off
				}
			}
		}
	}

	else if(pb == 8)
	{
		if(mode==0){
			if(set_time == 1)
			{
				bit_down();
			}
		}
		else if(mode==1){
			if(timer_onoff==0){
				if(set_timer == 1)
				{
					bit_down_timer();
				}
				else if(set_timer==0){
					timer_clear();
				}
			}
		}
		else if(mode==2){
			if(alarm_onoff==0){
				if(set_alarm == 1)
				{
					bit_down_alarm();
				}
				else if(set_alarm==0){
					alarm_clear();
				}
			}
		}
	}

	PUSHBUTTON_mWriteReg(XPAR_PUSHBUTTON_0_S00_AXI_BASEADDR, 0, 0);
	InterruptProcessed = TRUE;
}

void clock(XIicPs	Iic, u8* SendBuffer, u8* RecvBuffer){

	int 	IicStatus;
	int		SegReg;

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_upline, "Digital Clock   ");

	IicStatus = ReadRTC(Iic, SendBuffer, RecvBuffer);
	if (IicStatus != XST_SUCCESS)
	{
		//return XST_FAILURE;
		xil_printf("XST_failure\n");
	}

	SegReg = ((RecvBuffer[2] & 0x3f)<<24) + (0xA << 20) + (( RecvBuffer[1] & 0x7f)<<12) + (0xA << 8) + (RecvBuffer[0] & 0x7f);

	hour = RecvBuffer[2]&0x3F;
	minute = RecvBuffer[1]&0x7F;
	second = RecvBuffer[0]&0x7F;

	SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);
	WriteTLCDReg(TlcdReg_downline, SegReg);

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}
}

void clk_set(XIicPs	Iic, u8* WriteBuffer)
{

	int		SegReg;

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_downline, "Time Setting    ");
	sprintf(TlcdReg_upline, "Digital Clock   ");

	if( set_where == 1 )
	{
		SegReg = ((hour & 0x3F)<<24) | 0xAAAAAA;
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);

	}
	else if( set_where == 2 )
	{
		SegReg = 0xAAA00000 | ((minute & 0x7F)<<12) | 0xAAA;
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);

	}

	else if( set_where == 3 )
	{
		SegReg = 0xAAAAAA00 | ((second & 0x7F));
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);


	}

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}
}

void update(XIicPs	Iic, u8* WriteBuffer){

	int 	IicStatus;

	IicStatus = WriteRTC(Iic, WriteBuffer, HOUR, hour);
	if(IicStatus != XST_SUCCESS)
	{
		//return XST_FAILURE;
		xil_printf("XST_failure\n");
	}

	IicStatus = WriteRTC(Iic, WriteBuffer, MIN, minute);
	if(IicStatus != XST_SUCCESS)
	{
		//return XST_FAILURE;
		xil_printf("XST_failure\n");
	}

	IicStatus = WriteRTC(Iic, WriteBuffer, SEC, second);
	if(IicStatus != XST_SUCCESS)
	{
		//return XST_FAILURE;
		xil_printf("XST_failure\n");
	}

	clk_update=0;

}

void timer(){

	int		SegReg;

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_upline, "Timer           ");

	SegReg = ((timer_hour)<<24) + (0xA << 20) + (( timer_minute)<<12) + (0xA << 8) + (timer_second);

	SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);
	WriteTLCDReg(TlcdReg_downline, SegReg);

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}
}

void timer_set()
{
	int		SegReg;

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_downline, "Setting mode    ");
	sprintf(TlcdReg_upline, "Timer           ");

	if( set_where_timer == 1 )
	{
		SegReg = ((timer_hour & 0x3F)<<24) | 0xAAAAAA;
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);

	}
	else if( set_where_timer == 2 )
	{
		SegReg = 0xAAA00000 | ((timer_minute & 0x7F)<<12) | 0xAAA;
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);

	}

	else if( set_where_timer == 3 )
	{
		SegReg = 0xAAAAAA00 | ((timer_second & 0x7F));
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);

	}

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}
}

void timer_on(){

	int		SegReg;

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_upline, "Timer On        ");

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
	}

	while((timer_onoff==1)&&(set_timer==2)){

		while(delay!=0){
			delay--;
		}

		delay=onesec;

		timer_de();

		SegReg = ((timer_hour)<<24) + (0xA << 20) + (( timer_minute)<<12) + (0xA << 8) + (timer_second);

		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);
		WriteTLCDReg(TlcdReg_downline, SegReg);

		for(int i = 0; i < 4; i++)
		{
			TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
		}
	}

}

void timer_de(){

	char tmp_10 = 0;
	char tmp_1 = 0;

	if((timer_hour|timer_minute|timer_second)!=0){

		if(timer_second!=0){
			tmp_10 = ((timer_second>>4)&0xF);
			tmp_1 = timer_second & 0xF;
			if(tmp_1 == 0)
			{
				tmp_1 = 9;
				if(tmp_10 == 0)
				{
					tmp_10 = 5;
				}
				else	tmp_10 = tmp_10 - 0x1;
			}
			else
			{
				tmp_1 = tmp_1 - 0x1;
			}

			timer_second = (tmp_10 << 4) | tmp_1;
		}
		else if(timer_minute!=0){

			tmp_10 = ((timer_minute>>4)&0xF);
			tmp_1 = timer_minute & 0xF;
			if(tmp_1 == 0)
			{
				tmp_1 = 9;
				if(tmp_10 == 0)
				{
					tmp_10 = 5;
				}
				else	tmp_10 = tmp_10 - 1;
			}
			else
			{
			tmp_1 = tmp_1 - 1;
			}

			timer_minute = (tmp_10 << 4) | tmp_1;
			timer_second=0x59;

		}
		else if(timer_hour!=0){

			tmp_10 = ((timer_hour>>4)&0xF);
			tmp_1 = timer_hour & 0xF;
			if(tmp_10 == 2 || tmp_10 == 1)
			{
				if(tmp_1 == 0)
				{
					tmp_1 = 9;
					tmp_10 = tmp_10 - 1;
				}
				else
				{
					tmp_1 = tmp_1 - 1;
				}
			}
			else if(tmp_10 == 0)
			{
				if(tmp_1 == 0)
				{
					tmp_1 = 3;
					tmp_10 = 2;
				}
				else
				{
					tmp_1 = tmp_1 - 1;
				}
			}

			timer_hour = (tmp_10 << 4) | tmp_1;
			timer_minute=0x59;
			timer_second=0x59;
		}
	}
	else{
		timer_onoff=0;
		set_timer=3;
		LED_mWriteReg(XPAR_LED_0_S00_AXI_BASEADDR,0,1);
		//Timer count 00:00:00
	}

}

void timer_off(){

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_downline, "PUSH Button     ");
	sprintf(TlcdReg_upline, "Timer End       ");


	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}
}

void timer_clear(){

	timer_hour=0;
	timer_minute=0;
	timer_second=0;
}

void alarm(){

	int		SegReg;

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_upline, "Alarm           ");

	SegReg = ((alarm_hour)<<24) + (0xA << 20) + (( alarm_minute)<<12) + (0xA << 8) + (alarm_second);

	SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);
	WriteTLCDReg(TlcdReg_downline, SegReg);

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}
}

void alarm_set()
{
	int		SegReg;

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_downline, "Setting mode    ");
	sprintf(TlcdReg_upline, "Alarm           ");

	if( set_where_alarm == 1 )
	{
		SegReg = ((alarm_hour & 0x3F)<<24) | 0xAAAAAA;
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);

	}
	else if( set_where_alarm == 2 )
	{
		SegReg = 0xAAA00000 | ((alarm_minute & 0x7F)<<12) | 0xAAA;
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);

	}

	else if( set_where_alarm == 3 )
	{
		SegReg = 0xAAAAAA00 | ((alarm_second & 0x7F));
		SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);

	}

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}
}

void alarm_on(){

	int		SegReg;

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_upline, "Alarm On        ");
	SegReg = ((alarm_hour)<<24) + (0xA << 20) + (( alarm_minute)<<12) + (0xA << 8) + (alarm_second);

	SEVEN_SEG_mWriteReg(XPAR_SEVEN_SEG_0_S00_AXI_BASEADDR, SEVEN_SEG_S00_AXI_SLV_REG0_OFFSET, SegReg);
	WriteTLCDReg(TlcdReg_downline, SegReg);

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}
}

void alarm_off(){

	char	TlcdReg_upline[16];
	char	TlcdReg_downline[16];

	sprintf(TlcdReg_downline, "Wake Up         ");
	sprintf(TlcdReg_upline, "Alarming!!      ");

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4, TlcdReg_upline[i*4+3] + (TlcdReg_upline[i*4+2]<<8) + (TlcdReg_upline[i*4+1]<<16) + (TlcdReg_upline[i*4]<<24));
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}

	delay=delay/4;
	while(delay!=0){
		delay--;
	}

	delay=onesec;

	sprintf(TlcdReg_downline, "                ");

	for(int i = 0; i < 4; i++)
	{
		TEXTLCD_mWriteReg(XPAR_TEXTLCD_0_S00_AXI_BASEADDR, i*4+16, TlcdReg_downline[i*4+3] + (TlcdReg_downline[i*4+2]<<8) + (TlcdReg_downline[i*4+1]<<16) + (TlcdReg_downline[i*4]<<24));
	}

	delay=delay/4;
	while(delay!=0){
			delay--;
	}

	delay=onesec;

}

char alarm_check(XIicPs	Iic, u8* SendBuffer, u8* RecvBuffer){

	int 	IicStatus;
	volatile char tmp_hour;
	volatile char tmp_minute;
	volatile char tmp_second;

	xil_printf("before readRTC\n\n");

	IicStatus = ReadRTC(Iic, SendBuffer, RecvBuffer);
	if (IicStatus != XST_SUCCESS)
	{
		//return XST_FAILURE;
		xil_printf("XST_failure\n");
	}

	xil_printf("after readRTC\n\n");
	tmp_hour = RecvBuffer[2]&0x3F;
	tmp_minute = RecvBuffer[1]&0x7F;
	tmp_second = RecvBuffer[0]&0x7F;

	if((alarm_hour==tmp_hour)&&(alarm_minute==tmp_minute)&&(alarm_second==tmp_second)){
		return 1;
	}

	else return 0;
}

void alarm_clear(){

	alarm_hour=0;
	alarm_minute=0;
	alarm_second=0;
}

void bit_up(){

	char tmp_10 = 0;
	char tmp_1 = 0;

	switch(set_where)
	{
		case 1:

			tmp_10 = ((hour>>4) & 0xF);
			tmp_1 = hour & 0xF;
			if(tmp_10 == 0 || tmp_10 == 1)
			{
				tmp_1 = tmp_1 + 1;
				if(tmp_1 >9)
				{
					tmp_1 = 0;
					tmp_10 = tmp_10 + 1;
				}
			}

			else if(tmp_10 == 2)
			{
				tmp_1 = tmp_1 + 1;
				if(tmp_1 > 3)
				{
					tmp_1 = 0;
					tmp_10 = 0;
				}
			}

			hour = (tmp_10 << 4) | tmp_1;
			break;

		case 2:

			tmp_10 = ((minute>>4)&0xF);
			tmp_1 = minute & 0xF;
			tmp_1 = tmp_1 + 1;
			if(tmp_1>9)
			{
				tmp_1 = 0;
				tmp_10 = tmp_10 + 1;
				if(tmp_10>5) tmp_10 = 0;
			}

			minute = (tmp_10 << 4) | tmp_1;
			break;

		case 3:

			tmp_10 = ((second>>4)&0xF);
			tmp_1 = second & 0xF;
			tmp_1 = tmp_1 + 1;
			if(tmp_1>9)
			{
				tmp_1 = 0;
				tmp_10 = tmp_10 + 1;
				if(tmp_10>5)	tmp_10 = 0;
			}

			second = (tmp_10 << 4) | tmp_1;
			break;

	}
}

void bit_down(){

	char tmp_10 = 0;
	char tmp_1 = 0;

	switch(set_where)
	{
		case 1:
			tmp_10 = ((hour>>4)&0xF);
			tmp_1 = hour & 0xF;
			if(tmp_10 == 2 || tmp_10 == 1)
			{
				if(tmp_1 == 0)
				{
					tmp_1 = 9;
					tmp_10 = tmp_10 - 1;
				}
				else
				{
					tmp_1 = tmp_1 - 1;
				}
			}

			else if(tmp_10 == 0)
			{
				if(tmp_1 == 0)
				{
					tmp_1 = 3;
					tmp_10 = 2;
				}
				else
				{
					tmp_1 = tmp_1 - 1;
				}
			}
			hour = (tmp_10 << 4) | tmp_1;
			break;

		case 2:
			tmp_10 = ((minute>>4)&0xF);
			tmp_1 = minute & 0xF;
			if(tmp_1 == 0)
			{
				tmp_1 = 9;
				if(tmp_10 == 0)
				{
					tmp_10 = 5;
				}
				else	tmp_10 = tmp_10 - 1;
			}
			else
			{
				tmp_1 = tmp_1 - 1;
			}
			minute = (tmp_10 << 4) | tmp_1;
			break;

		case 3:
			tmp_10 = ((second>>4)&0xF);
			tmp_1 = second & 0xF;
			if(tmp_1 == 0)
			{
				tmp_1 = 9;
				if(tmp_10 == 0)
				{
					tmp_10 = 5;
				}
				else	tmp_10 = tmp_10 - 0x1;
			}
			else
			{
				tmp_1 = tmp_1 - 0x1;
			}
			second = (tmp_10 << 4) | tmp_1;
			break;
	}
}

void bit_up_timer(){

	char tmp_10 = 0;
	char tmp_1 = 0;

	switch(set_where_timer)
	{
		case 1:

			tmp_10 = ((timer_hour>>4) & 0xF);
			tmp_1 = timer_hour & 0xF;
			if(tmp_10 == 0 || tmp_10 == 1)
			{
				tmp_1 = tmp_1 + 1;
				if(tmp_1 >9)
				{
					tmp_1 = 0;
					tmp_10 = tmp_10 + 1;
				}
			}

			else if(tmp_10 == 2)
			{
				tmp_1 = tmp_1 + 1;
				if(tmp_1 > 3)
				{
					tmp_1 = 0;
					tmp_10 = 0;
				}
			}

			timer_hour = (tmp_10 << 4) | tmp_1;
			break;

		case 2:

			tmp_10 = ((timer_minute>>4)&0xF);
			tmp_1 = timer_minute & 0xF;
			tmp_1 = tmp_1 + 1;
			if(tmp_1>9)
			{
				tmp_1 = 0;
				tmp_10 = tmp_10 + 1;
				if(tmp_10>5) tmp_10 = 0;
			}

			timer_minute = (tmp_10 << 4) | tmp_1;
			break;

		case 3:

			tmp_10 = ((timer_second>>4)&0xF);
			tmp_1 = timer_second & 0xF;
			tmp_1 = tmp_1 + 1;
			if(tmp_1>9)
			{
				tmp_1 = 0;
				tmp_10 = tmp_10 + 1;
				if(tmp_10>5)	tmp_10 = 0;
			}

			timer_second = (tmp_10 << 4) | tmp_1;
			break;

	}
}

void bit_down_timer(){

	char tmp_10 = 0;
	char tmp_1 = 0;

	switch(set_where_timer)
	{
		case 1:
			tmp_10 = ((timer_hour>>4)&0xF);
			tmp_1 = timer_hour & 0xF;
			if(tmp_10 == 2 || tmp_10 == 1)
			{
				if(tmp_1 == 0)
				{
					tmp_1 = 9;
					tmp_10 = tmp_10 - 1;
				}
				else
				{
					tmp_1 = tmp_1 - 1;
				}
			}

			else if(tmp_10 == 0)
			{
				if(tmp_1 == 0)
				{
					tmp_1 = 3;
					tmp_10 = 2;
				}
				else
				{
					tmp_1 = tmp_1 - 1;
				}
			}
			timer_hour = (tmp_10 << 4) | tmp_1;
			break;

		case 2:
			tmp_10 = ((timer_minute>>4)&0xF);
			tmp_1 = timer_minute & 0xF;
			if(tmp_1 == 0)
			{
				tmp_1 = 9;
				if(tmp_10 == 0)
				{
					tmp_10 = 5;
				}
				else	tmp_10 = tmp_10 - 1;
			}
			else
			{
				tmp_1 = tmp_1 - 1;
			}
			timer_minute = (tmp_10 << 4) | tmp_1;
			break;

		case 3:
			tmp_10 = ((timer_second>>4)&0xF);
			tmp_1 = timer_second & 0xF;
			if(tmp_1 == 0)
			{
				tmp_1 = 9;
				if(tmp_10 == 0)
				{
					tmp_10 = 5;
				}
				else	tmp_10 = tmp_10 - 0x1;
			}
			else
			{
				tmp_1 = tmp_1 - 0x1;
			}
			timer_second = (tmp_10 << 4) | tmp_1;
			break;
	}
}

void bit_up_alarm(){

	char tmp_10 = 0;
	char tmp_1 = 0;

	switch(set_where_alarm)
	{
		case 1:

			tmp_10 = ((alarm_hour>>4) & 0xF);
			tmp_1 = alarm_hour & 0xF;
			if(tmp_10 == 0 || tmp_10 == 1)
			{
				tmp_1 = tmp_1 + 1;
				if(tmp_1 >9)
				{
					tmp_1 = 0;
					tmp_10 = tmp_10 + 1;
				}
			}

			else if(tmp_10 == 2)
			{
				tmp_1 = tmp_1 + 1;
				if(tmp_1 > 3)
				{
					tmp_1 = 0;
					tmp_10 = 0;
				}
			}

			alarm_hour = (tmp_10 << 4) | tmp_1;
			break;

		case 2:

			tmp_10 = ((alarm_minute>>4)&0xF);
			tmp_1 = alarm_minute & 0xF;
			tmp_1 = tmp_1 + 1;
			if(tmp_1>9)
			{
				tmp_1 = 0;
				tmp_10 = tmp_10 + 1;
				if(tmp_10>5) tmp_10 = 0;
			}

			alarm_minute = (tmp_10 << 4) | tmp_1;
			break;

		case 3:

			tmp_10 = ((alarm_second>>4)&0xF);
			tmp_1 = alarm_second & 0xF;
			tmp_1 = tmp_1 + 1;
			if(tmp_1>9)
			{
				tmp_1 = 0;
				tmp_10 = tmp_10 + 1;
				if(tmp_10>5)	tmp_10 = 0;
			}

			alarm_second = (tmp_10 << 4) | tmp_1;
			break;

	}
}

void bit_down_alarm(){

	char tmp_10 = 0;
	char tmp_1 = 0;

	switch(set_where_alarm)
	{
		case 1:
			tmp_10 = ((alarm_hour>>4)&0xF);
			tmp_1 = alarm_hour & 0xF;
			if(tmp_10 == 2 || tmp_10 == 1)
			{
				if(tmp_1 == 0)
				{
					tmp_1 = 9;
					tmp_10 = tmp_10 - 1;
				}
				else
				{
					tmp_1 = tmp_1 - 1;
				}
			}

			else if(tmp_10 == 0)
			{
				if(tmp_1 == 0)
				{
					tmp_1 = 3;
					tmp_10 = 2;
				}
				else
				{
					tmp_1 = tmp_1 - 1;
				}
			}
			alarm_hour = (tmp_10 << 4) | tmp_1;
			break;

		case 2:
			tmp_10 = ((alarm_minute>>4)&0xF);
			tmp_1 = alarm_minute & 0xF;
			if(tmp_1 == 0)
			{
				tmp_1 = 9;
				if(tmp_10 == 0)
				{
					tmp_10 = 5;
				}
				else	tmp_10 = tmp_10 - 1;
			}
			else
			{
				tmp_1 = tmp_1 - 1;
			}
			alarm_minute = (tmp_10 << 4) | tmp_1;
			break;

		case 3:
			tmp_10 = ((alarm_second>>4)&0xF);
			tmp_1 = alarm_second & 0xF;
			if(tmp_1 == 0)
			{
				tmp_1 = 9;
				if(tmp_10 == 0)
				{
					tmp_10 = 5;
				}
				else	tmp_10 = tmp_10 - 0x1;
			}
			else
			{
				tmp_1 = tmp_1 - 0x1;
			}
			alarm_second = (tmp_10 << 4) | tmp_1;
			break;
	}
}
