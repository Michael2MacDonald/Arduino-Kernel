@ .cpu cortex-m0plus
.syntax unified
.thumb

.section .text
.align 2

.global SVC_Handler
.type	SVC_Handler,%function
.thumb_func
SVC_Handler:
	mrs r4, msp  // Get the Main Stack Pointer (MSP)
	msr psp, r4  // Set PSP to the MSP
	msr msp, r0  // Set MSP to the allocated stack space for interrupt handlers
	movs r0, #2  // Use Program Stack Pointer (PSP) (Set bit 1 of the CONTROL register [CONTROL.SPSEL])
	msr control, r0 // Write to the control register
	bx lr // Return
