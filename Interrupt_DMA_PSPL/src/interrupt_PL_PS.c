/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */
#if 1
#include <stdio.h>
#include <stdlib.h>
#include "xil_printf.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "xaxidma.h"
#include "xscugic.h"
#include "xil_exception.h"
#include <time.h>



#define DMA_DEVICE_ID  XPAR_AXIDMA_1_DEVICE_ID

#define DDR_BASE_ADDRESS    XPAR_PSU_DDR_0_S_AXI_BASEADDR
#define MEM_BASE_ADDR		(DDR_BASE_ADDRESS + 0x1000000)

#define RX_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
#define TX_INTR_ID		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID

#define TX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00100000)
#define RX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00300000)
#define RX_BUFFER_HIGH		(MEM_BASE_ADDR + 0x004FFFFF)

#ifndef SDT
#ifdef XPAR_INTC_1_DEVICE_ID
#define INTC_DEVICE_ID          XPAR_INTC_0_DEVICE_ID
#else
#define INTC_DEVICE_ID          XPAR_SCUGIC_SINGLE_DEVICE_ID
#endif

#ifdef XPAR_INTC_0_DEVICE_ID
#define INTC		XIntc
#define INTC_HANDLER	XIntc_InterruptHandler
#else
#define INTC		XScuGic
#define INTC_HANDLER	XScuGic_InterruptHandler
#endif

/* Timeout loop counter for reset
 */
#define RESET_TIMEOUT_COUNTER	10000

/*
 * Buffer and Buffer Descriptor related constant definition
 */
#define MAX_PKT_LEN		0x32

#define NUMBER_OF_TRANSFERS	256
#define POLL_TIMEOUT_COUNTER    1000000U
#define NUMBER_OF_EVENTS	1

#ifndef DEBUG
extern void xil_printf(const char *format, ...);
#endif

#ifdef XPAR_UARTNS550_0_BASEADDR
static void Uart550_Setup(void);
#endif

static int CheckData(int Length, u8 StartValue);
static void TxIntrHandler(void *Callback);
static void RxIntrHandler(void *Callback);



#ifndef SDT
static int SetupIntrSystem(INTC *IntcInstancePtr,
			   XAxiDma *AxiDmaPtr, u16 TxIntrId, u16 RxIntrId);
static void DisableIntrSystem(INTC *IntcInstancePtr,
			      u16 TxIntrId, u16 RxIntrId);

#endif


static XAxiDma axidma;

#ifndef SDT
static INTC Intc;	/* Instance of the Interrupt Controller */

#endif

/*
 * Flags interrupt handlers use to notify the application context the events.
 */
volatile u32 TxDone;
volatile u32 RxDone;

volatile u32 Error;


int main(void)
{
	int Status;
	XAxiDma_Config *Config;
	u8 *TxBufferptr;
	u8 *RxBufferptr;
	u32 value;

	TxBufferptr = (u8*)TX_BUFFER_BASE;
	RxBufferptr = (u8*)RX_BUFFER_BASE;

	xil_printf("\r\n---- Entering main() ---\r\n");
#ifndef SDT
	Config = XAxiDma_LookupConfig(DMA_DEVICE_ID);
	if(!Config)
	{
		xil_printf("No config found for %d \r\n",DMA_DEVICE_ID);

		return XST_FAILURE;
	}
#else
	Config = XAxiDma_LookupConfig(XPAR_XAXIDMA_0_BASEADDR);
	if (!Config) {
		xil_printf("No config found for %d\r\n", XPAR_XAXIDMA_0_BASEADDR);

		return XST_FAILURE;
	}
#endif

	/* Initialize DAMA engine */
	Status = XAxiDma_CfgInitialize(&axidma,Config);

	if(Status != XST_SUCCESS)
	{
		xil_printf("Initialization failed %d\r\n",Status);
		return XST_FAILURE;
	}
	if(XAxiDma_HasSg(&axidma))
	{
		xil_printf("Device configured as SG mode");
		return XST_FAILURE;
	}
	/*Set up Interrupt  system*/
#ifndef SDT
	Status = SetupIntrSystem(&Intc, &axidma, TX_INTR_ID, RX_INTR_ID);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed intr setup\r\n");
		return XST_FAILURE;
	}
#else
	Status = XSetupInterruptSystem(&AxiDma, &TxIntrHandler,
				       Config->IntrId[0], Config->IntrParent,
				       XINTERRUPT_DEFAULT_PRIORITY);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XSetupInterruptSystem(&AxiDma, &RxIntrHandler,
				       Config->IntrId[1], Config->IntrParent,
				       XINTERRUPT_DEFAULT_PRIORITY);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
#endif

	/*Disable all interrupts before setup*/
	XAxiDma_IntrDisable(&axidma,XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DMA_TO_DEVICE);

	XAxiDma_IntrDisable(&axidma, XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DEVICE_TO_DMA);

	/*initialize flags before start transfer*/
	TxDone =0;
	RxDone = 0;
	Error = 0;

	TxBufferptr[0] = 0xc;
	/* Flush the buffers before the DMA transfer, in case the Data Cache
	 * is enabled
	 */
	Xil_DCacheFlushRange((UINTPTR)TxBufferptr, MAX_PKT_LEN);
	Xil_DCacheFlushRange((UINTPTR)RxBufferptr, MAX_PKT_LEN);

	for (int Index = 0; Index < 2; Index ++)
	{

	Status = XAxiDma_SimpleTransfer(&axidma, (UINTPTR) RxBufferptr,MAX_PKT_LEN, XAXIDMA_DEVICE_TO_DMA);
	if (Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	Status = XAxiDma_SimpleTransfer(&axidma, (UINTPTR) TxBufferptr,MAX_PKT_LEN, XAXIDMA_DMA_TO_DEVICE);
	if (Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	Status = Xil_WaitForEventSet(POLL_TIMEOUT_COUNTER, NUMBER_OF_EVENTS, &Error);
	if (Status == XST_SUCCESS) {
		if (!TxDone) {
			xil_printf("Transmit error %d\r\n", Status);
			goto Done;
		} else if (Status == XST_SUCCESS && !RxDone) {
			xil_printf("Receive error %d\r\n", Status);
			goto Done;
		}
	}

	/*
	 * Wait for TX done or timeout
	 */
	Status = Xil_WaitForEventSet(POLL_TIMEOUT_COUNTER, NUMBER_OF_EVENTS, &TxDone);
	if (Status != XST_SUCCESS) {
		xil_printf("Transmit failed %d\r\n", Status);
		goto Done;

	}

	/*
	 * Wait for RX done or timeout
	 */
	Status = Xil_WaitForEventSet(POLL_TIMEOUT_COUNTER, NUMBER_OF_EVENTS, &RxDone);
	if (Status != XST_SUCCESS) {
		xil_printf("Receive failed %d\r\n", Status);
		goto Done;
	}

	}

	xil_printf("Successfully ran AXI DMA interrupt \r\n");


	/* Disable TX and RX Ring interrupts and return success */
#ifndef SDT
	DisableIntrSystem(&Intc, TX_INTR_ID, RX_INTR_ID);
#else
	XDisconnectInterruptCntrl(Config->IntrId[0], Config->IntrParent);
	XDisconnectInterruptCntrl(Config->IntrId[1], Config->IntrParent);
#endif
	Done:
	xil_printf("___Exitining main()____");

	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}
/*****************************************************************************/
/*
*
* This is the DMA TX Interrupt handler function.
*
* It gets the interrupt status from the hardware, acknowledges it, and if any
* error happens, it resets the hardware. Otherwise, if a completion interrupt
* is present, then sets the TxDone.flag
*
* @param	Callback is a pointer to TX channel of the DMA engine.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/

static void TxIntrHandler(void * callback)
{
	xil_printf("Tx interrupt handler is called\r\n");
	u32 IrqStatus;
	u32 Timeout;
	XAxiDma * AxiDmaInst = (XAxiDma*)callback;

	/* Read pending Interrupts*/
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst,XAXIDMA_DEVICE_TO_DMA);

	/* Acknowledge asserted interrupts*/
	XAxiDma_IntrAckIrq(AxiDmaInst,IrqStatus,XAXIDMA_DEVICE_TO_DMA);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {

		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		Error = 1;

		/*
		 * Reset should never fail for transmit channel
		 */
		XAxiDma_Reset(AxiDmaInst);

		Timeout = RESET_TIMEOUT_COUNTER;

		while (Timeout) {
			if (XAxiDma_ResetIsDone(AxiDmaInst)) {
				break;
			}

			Timeout -= 1;
		}

		return;
	}

	/*
	 * If Completion interrupt is asserted, then set the TxDone flag
	 */
	if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)) {

		TxDone = 1;
	}

}

/*****************************************************************************/
/*
*
* This is the DMA RX interrupt handler function
*
*
* It gets the interrupt status from the hardware, acknowledges it, and if any
* error happens, it resets the hardware. Otherwise, if a completion interrupt
* is present, then it sets the RxDone flag.
*
* @param	Callback is a pointer to RX channel of the DMA engine.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void RxIntrHandler(void * Callback)
{
	xil_printf("Rx interrupt raised");
	u32 IrqStatus;
	int Timeout;
	XAxiDma *AxiDmaInst = (XAxiDma*)Callback;

	/*Read pending Interrupts*/
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst,XAXIDMA_DEVICE_TO_DMA);

	/*Acknowledge pending Interrupts*/
	XAxiDma_IntrAckIrq(AxiDmaInst,IrqStatus,XAXIDMA_DEVICE_TO_DMA);

	/* If no interrupt is asserted, we do nothing just like you*/
	if(!(IrqStatus & XAXIDMA_IRQ_ALL_MASK))
	{
		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the hardware to recover from error,
	 * and return with no further proicessing
	 */
	if((IrqStatus & XAXIDMA_IRQ_ERROR_MASK))
	{
		Error = 1;
		/* reset DMA*/

		XAxiDma_Reset(AxiDmaInst);

		Timeout = RESET_TIMEOUT_COUNTER;

		while(Timeout)
		{
			if(XAxiDma_ResetIsDone(AxiDmaInst))
			{
				break;
			}
			Timeout -= 1;
		}

		return;
	}
	/* If completion interrupt asserted, then RxDone flag*/
	if((IrqStatus & XAXIDMA_IRQ_IOC_MASK))
	{
		RxDone = 1;
	}
}
static int SetupIntrSystem(INTC* IntcInstancePtr, XAxiDma *AxiDmaPtr,u16 TxIntrId,u16 RxIntrId)
{
	int Status;
	XScuGic_Config *IntcConfig;

	/* Initialize interrupt controller driver*/
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if(NULL == IntcConfig)
	{
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr,IntcConfig,IntcConfig->CpuBaseAddress);
	if(Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	XScuGic_SetPriorityTriggerType(IntcInstancePtr,TxIntrId,0xA0, 0x3);
	XScuGic_SetPriorityTriggerType(IntcInstancePtr,RxIntrId,0xA0, 0x3);

	/*Connect the device driver handler that will be called when an interrupt occurs*/
	Status = XScuGic_Connect(IntcInstancePtr,TxIntrId, (Xil_InterruptHandler)TxIntrHandler,AxiDmaPtr);
	if(Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	/*Connect the device driver handler that will be called when an interrupt occurs*/
	Status = XScuGic_Connect(IntcInstancePtr,RxIntrId, (Xil_InterruptHandler)TxIntrHandler,AxiDmaPtr);
	if(Status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	XScuGic_Enable(IntcInstancePtr,TxIntrId);
	XScuGic_Enable(IntcInstancePtr,RxIntrId);

	/* Enable exception interrupts*/
	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler)INTC_HANDLER,(void *)IntcInstancePtr);
	Xil_ExceptionEnable();

	return XST_SUCCESS;

}
/*****************************************************************************/
/**
*
* This function disables the interrupts for DMA engine.
*
* @param	IntcInstancePtr is the pointer to the INTC component instance
* @param	TxIntrId is interrupt ID associated w/ DMA TX channel
* @param	RxIntrId is interrupt ID associated w/ DMA RX channel
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void DisableIntrSystem(INTC *IntcInstancePtr,
			      u16 TxIntrId, u16 RxIntrId)
{
#ifdef XPAR_INTC_0_DEVICE_ID
	/* Disconnect the interrupts for the DMA TX and RX channels */
	XIntc_Disconnect(IntcInstancePtr, TxIntrId);
	XIntc_Disconnect(IntcInstancePtr, RxIntrId);
#else
	XScuGic_Disconnect(IntcInstancePtr, TxIntrId);
	XScuGic_Disconnect(IntcInstancePtr, RxIntrId);
#endif
}

#endif
#endif
