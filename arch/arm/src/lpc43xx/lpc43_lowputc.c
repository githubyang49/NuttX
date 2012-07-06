/**************************************************************************
 * arch/arm/src/lpc43xx/lpc43_lowputc.c
 *
 *   Copyright (C) 2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************/

/**************************************************************************
 * Included Files
 **************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#include <arch/irq.h>
#include <arch/board/board.h>

#include "up_internal.h"
#include "up_arch.h"

#include "chip.h"
#include "lpc43_config.h"
#include "lpc43_pinconfig.h"
#include "chip/lpc43_uart.h"

#include "lpc43_lowputc.h"

/**************************************************************************
 * Private Definitions
 **************************************************************************/
     
/* Select UART parameters for the selected console */

#if defined(CONFIG_USART0_SERIAL_CONSOLE)
#  define CONSOLE_BASE     LPC43_USART0_BASE
#  define CONSOLE_BAUD     CONFIG_USART0_BAUD
#  define CONSOLE_BITS     CONFIG_USART0_BITS
#  define CONSOLE_PARITY   CONFIG_USART0_PARITY
#  define CONSOLE_2STOP    CONFIG_USART0_2STOP
#elif defined(CONFIG_UART1_SERIAL_CONSOLE)
#  define CONSOLE_BASE     LPC43_UART1_BASE
#  define CONSOLE_BAUD     CONFIG_UART1_BAUD
#  define CONSOLE_BITS     CONFIG_UART1_BITS
#  define CONSOLE_PARITY   CONFIG_UART1_PARITY
#  define CONSOLE_2STOP    CONFIG_UART1_2STOP
#elif defined(CONFIG_USART2_SERIAL_CONSOLE)
#  define CONSOLE_BASE     LPC43_USART2_BASE
#  define CONSOLE_BAUD     CONFIG_USART2_BAUD
#  define CONSOLE_BITS     CONFIG_USART2_BITS
#  define CONSOLE_PARITY   CONFIG_USART2_PARITY
#  define CONSOLE_2STOP    CONFIG_USART2_2STOP
#elif defined(CONFIG_USART3_SERIAL_CONSOLE)
#  define CONSOLE_BASE     LPC43_USART3_BASE
#  define CONSOLE_BAUD     CONFIG_USART3_BAUD
#  define CONSOLE_BITS     CONFIG_USART3_BITS
#  define CONSOLE_PARITY   CONFIG_USART3_PARITY
#  define CONSOLE_2STOP    CONFIG_USART3_2STOP
#elif defined(HAVE_CONSOLE)
#  error "No CONFIG_UARTn_SERIAL_CONSOLE Setting"
#endif

/* Get word length setting for the console */

#if CONSOLE_BITS == 5
#  define CONSOLE_LCR_WLS UART_LCR_WLS_5BIT
#elif CONSOLE_BITS == 6
#  define CONSOLE_LCR_WLS UART_LCR_WLS_6BIT
#elif CONSOLE_BITS == 7
#  define CONSOLE_LCR_WLS UART_LCR_WLS_7BIT
#elif CONSOLE_BITS == 8
#  define CONSOLE_LCR_WLS UART_LCR_WLS_8BIT
#elif defined(HAVE_CONSOLE)
#  error "Invalid CONFIG_UARTn_BITS setting for console "
#endif

/* Get parity setting for the console */

#if CONSOLE_PARITY == 0
#  define CONSOLE_LCR_PAR 0
#elif CONSOLE_PARITY == 1
#  define CONSOLE_LCR_PAR (UART_LCR_PE|UART_LCR_PS_ODD)
#elif CONSOLE_PARITY == 2
#  define CONSOLE_LCR_PAR (UART_LCR_PE|UART_LCR_PS_EVEN)
#elif CONSOLE_PARITY == 3
#  define CONSOLE_LCR_PAR (UART_LCR_PE|UART_LCR_PS_STICK1)
#elif CONSOLE_PARITY == 4
#  define CONSOLE_LCR_PAR (UART_LCR_PE|UART_LCR_PS_STICK0)
#elif defined(HAVE_CONSOLE)
#    error "Invalid CONFIG_UARTn_PARITY setting for CONSOLE"
#endif

/* Get stop-bit setting for the console and USART0/2/3, UART1 */

#if CONSOLE_2STOP != 0
#  define CONSOLE_LCR_STOP UART_LCR_STOP
#else
#  define CONSOLE_LCR_STOP 0
#endif

/* LCR and FCR values for the console */

#define CONSOLE_LCR_VALUE (CONSOLE_LCR_WLS | CONSOLE_LCR_PAR | CONSOLE_LCR_STOP)
#define CONSOLE_FCR_VALUE (UART_FCR_RXTRIGGER_8 | UART_FCR_TXRST |\
                           UART_FCR_RXRST | UART_FCR_FIFOEN)

/* We cannot allow the DLM/DLL divisor to become to small or will will lose too
 * much accuracy.  This following is a "fudge factor" that represents the minimum
 * value of the divisor that we will permit.
 */

#define UART_MINDL 32

/* Select a CCLK divider to produce the UART PCLK.  The strategy is to select the
 * smallest divisor that results in an solution within range of the 16-bit
 * DLM and DLL divisor:
 *
 *   BAUD = PCLK / (16 * DL), or
 *   DL   = PCLK / BAUD / 16
 *
 * Where:
 *
 *   PCLK = CCLK / divisor
 *
 * Ignoring the fractional divider for now.
 *
 * Check divisor == 1.  This works if the upper limit is met	
 *
 *   DL < 0xffff, or
 *   PCLK / BAUD / 16 < 0xffff, or
 *   CCLK / BAUD / 16 < 0xffff, or
 *   CCLK < BAUD * 0xffff * 16
 *   BAUD > CCLK / 0xffff / 16
 *
 * And the lower limit is met (we can't allow DL to get very close to one).
 *
 *   DL >= MinDL
 *   CCLK / BAUD / 16 >= MinDL, or
 *   BAUD <= CCLK / 16 / MinDL
 */

#if CONSOLE_BAUD < (LPC43_CCLK / 16 / UART_MINDL)
#  define CONSOLE_CCLKDIV  SYSCON_PCLKSEL_CCLK
#  define CONSOLE_NUMERATOR (LPC43_CCLK)

/* Check divisor == 2.  This works if:
 *
 *   2 * CCLK / BAUD / 16 < 0xffff, or
 *   BAUD > CCLK / 0xffff / 8
 *
 * And
 *
 *   2 * CCLK / BAUD / 16 >= MinDL, or
 *   BAUD <= CCLK / 8 / MinDL
 */

#elif CONSOLE_BAUD < (LPC43_CCLK / 8 / UART_MINDL)
#  define CONSOLE_CCLKDIV SYSCON_PCLKSEL_CCLK2
#  define CONSOLE_NUMERATOR (LPC43_CCLK / 2)

/* Check divisor == 4.  This works if:
 *
 *   4 * CCLK / BAUD / 16 < 0xffff, or
 *   BAUD > CCLK / 0xffff / 4
 *
 * And
 *
 *   4 * CCLK / BAUD / 16 >= MinDL, or
 *   BAUD <= CCLK / 4 / MinDL 
 */

#elif CONSOLE_BAUD < (LPC43_CCLK / 4 / UART_MINDL)
#  define CONSOLE_CCLKDIV SYSCON_PCLKSEL_CCLK4
#  define CONSOLE_NUMERATOR (LPC43_CCLK / 4)

/* Check divisor == 8.  This works if:
 *
 *   8 * CCLK / BAUD / 16 < 0xffff, or
 *   BAUD > CCLK / 0xffff / 2
 *
 * And
 *
 *   8 * CCLK / BAUD / 16 >= MinDL, or
 *   BAUD <= CCLK / 2 / MinDL 
 */

#else /* if CONSOLE_BAUD < (LPC43_CCLK / 2 / UART_MINDL) */
#  define CONSOLE_CCLKDIV   SYSCON_PCLKSEL_CCLK8
#  define CONSOLE_NUMERATOR (LPC43_CCLK /  8)
#endif

/* Then this is the value to use for the DLM and DLL registers */

#define CONSOLE_DL          (CONSOLE_NUMERATOR / (CONSOLE_BAUD << 4))

/**************************************************************************
 * Private Types
 **************************************************************************/

/**************************************************************************
 * Private Function Prototypes
 **************************************************************************/

/**************************************************************************
 * Global Variables
 **************************************************************************/

/**************************************************************************
 * Private Variables
 **************************************************************************/

/**************************************************************************
 * Private Functions
 **************************************************************************/

/**************************************************************************
 * Public Functions
 **************************************************************************/

/**************************************************************************
 * Name: up_lowputc
 *
 * Description:
 *   Output one byte on the serial console
 *
 **************************************************************************/

void up_lowputc(char ch)
{
#if defined HAVE_UART && defined HAVE_CONSOLE
  /* Wait for the transmitter to be available */

  while ((getreg32(CONSOLE_BASE+LPC43_UART_LSR_OFFSET) & UART_LSR_THRE) == 0);

  /* Send the character */

  putreg32((uint32_t)ch, CONSOLE_BASE+LPC43_UART_THR_OFFSET);
#endif
}

/**************************************************************************
 * Name: lpc43_lowsetup
 *
 * Description:
 *   This performs basic initialization of the UART used for the serial
 *   console.  Its purpose is to get the console output availabe as soon
 *   as possible.
 *
 *   The USART0/2/3 and UART1 peripherals are configured using the following registers:
 *   1. Power: In the PCONP register, set bits PCUSART0/1/2/3.
 *      On reset, USART0 and UART 1 are enabled (PCUSART0 = 1 and PCUART1 = 1)
 *      and USART2/3 are disabled (PCUART1 = 0 and PCUSART3 = 0).
 *   2. Peripheral clock: In the PCLKSEL0 register, select PCLK_USART0 and
 *      PCLK_UART1; in the PCLKSEL1 register, select PCLK_USART2 and PCLK_USART3.
 *   3. Baud rate: In the LCR register, set bit DLAB = 1. This enables access
 *      to registers DLL and DLM for setting the baud rate. Also, if needed,
 *      set the fractional baud rate in the fractional divider 
 *   4. UART FIFO: Use bit FIFO enable (bit 0) in FCR register to
 *      enable FIFO.
 *   5. Pins: Select UART pins through the PINSEL registers and pin modes
 *      through the PINMODE registers. UART receive pins should not have
 *      pull-down resistors enabled.
 *   6. Interrupts: To enable UART interrupts set bit DLAB = 0 in the LCRF
 *      register. This enables access to IER. Interrupts are enabled
 *      in the NVIC using the appropriate Interrupt Set Enable register.
 *   7. DMA: UART transmit and receive functions can operate with the
 *      GPDMA controller.
 *
 **************************************************************************/

void lpc43_lowsetup(void)
{
#ifdef HAVE_UART
  uint32_t regval;

  /* Step 1: Enable power for all console UART and disable power for
   * other UARTs
   *
   * USART0/2/3 clocking and power control:
   *
   *    ----------------------------------- -------------- --------------
   *                                        BASE CLOCK     BRANCH CLOCK
   *    ----------------------------------- -------------- --------------
   *    USART0 clock to register interface  BASE_M4_CLK    CLK_M4_USART0
   *    USART0 peripheral clock (PCLK)      BASE_UART0_CLK CLK_APB0_UART0
   *    UART1 clock to register interface   BASE_M4_CLK    CLK_M4_UART1
   *    UART1 peripheral clock (PCLK)       BASE_UART1_CLK CLK_APB0_UART1
   *    USART2 clock to register interface  BASE_M4_CLK    CLK_M4_USART2
   *    USART2 peripheral clock (PCLK)      BASE_UART2_CLK CLK_APB2_UART2
   *    USART3 clock to register interface  BASE_M4_CLK    CLK_M4_USART3
   *    USART3 peripheral clock (PCLK)      BASE_UART3_CLK CLK_APB2_UART3
   *    ----------------------------------- -------------- --------------
   */

#warning "Missing logic"
#if 0
  regval  = getreg32(LPC43_SYSCON_PCONP);
  regval &= ~(SYSCON_PCONP_PCUSART0|SYSCON_PCONP_PCUART1|
              SYSCON_PCONP_PCUSART2|SYSCON_PCONP_PCUSART3);
#if defined(CONFIG_USART0_SERIAL_CONSOLE)
  regval |= SYSCON_PCONP_PCUSART0;
#elif defined(CONFIG_UART1_SERIAL_CONSOLE)
  regval |= SYSCON_PCONP_PCUART1;
#elif defined(CONFIG_USART2_SERIAL_CONSOLE)
  regval |= SYSCON_PCONP_PCUSART2;
#elif defined(CONFIG_USART3_SERIAL_CONSOLE)
  regval |= SYSCON_PCONP_PCUSART3;
#endif
  putreg32(regval, LPC43_SYSCON_PCONP);

/* Step 2: Enable peripheral clocking for the console UART and disable
 * clocking for all other UARTs
 */

  regval = getreg32(LPC43_SYSCON_PCLKSEL0);
  regval &= ~(SYSCON_PCLKSEL0_USART0_MASK|SYSCON_PCLKSEL0_UART1_MASK);
#if defined(CONFIG_USART0_SERIAL_CONSOLE)
  regval |= (CONSOLE_CCLKDIV << SYSCON_PCLKSEL0_USART0_SHIFT);
#elif defined(CONFIG_UART1_SERIAL_CONSOLE)
  regval |= (CONSOLE_CCLKDIV << SYSCON_PCLKSEL0_UART1_SHIFT);
#endif
  putreg32(regval, LPC43_SYSCON_PCLKSEL0);

  regval = getreg32(LPC43_SYSCON_PCLKSEL1);
  regval &= ~(SYSCON_PCLKSEL1_USART2_MASK|SYSCON_PCLKSEL1_USART3_MASK);
#if defined(CONFIG_USART2_SERIAL_CONSOLE)
  regval |= (CONSOLE_CCLKDIV << SYSCON_PCLKSEL1_USART2_SHIFT);
#elif defined(CONFIG_USART3_SERIAL_CONSOLE)
  regval |= (CONSOLE_CCLKDIV << SYSCON_PCLKSEL1_USART3_SHIFT);
#endif
  putreg32(regval, LPC43_SYSCON_PCLKSEL1);
#endif

  /* Configure UART pins for the selected CONSOLE.  Definitions are
   * required in the board.h header file in order to map pins to the
   * correct pin and GPIO configuration.
   */

#if defined(CONFIG_USART0_SERIAL_CONSOLE)
  lpc43_pin_config(PINCONF_U0_TXD);
  lpc43_pin_config(PINCONF_U0_RXD);
#elif defined(CONFIG_UART1_SERIAL_CONSOLE)
  lpc43_pin_config(PINCONF_U1_TXD);
  lpc43_pin_config(PINCONF_U1_RXD);
#ifdef CONFIG_UART1_FLOWCONTROL
  lpc43_pin_config(PINCONF_U1_CTS);
  lpc43_pin_config(PINCONF_U1_DCD);
  lpc43_pin_config(PINCONF_U1_DSR);
  lpc43_pin_config(PINCONF_U1_DTR);
  lpc43_pin_config(PINCONF_U1_RI);
  lpc43_pin_config(PINCONF_U1_RTS);
#endif
#elif defined(CONFIG_USART2_SERIAL_CONSOLE)
  lpc43_pin_config(PINCONF_U2_TXD);
  lpc43_pin_config(PINCONF_U2_RXD);
#elif defined(CONFIG_USART3_SERIAL_CONSOLE)
  lpc43_pin_config(PINCONF_U3_TXD);
  lpc43_pin_config(PINCONF_U3_RXD);
#endif

  /* Configure the console (only) */

#if defined(HAVE_CONSOLE) && !defined(CONFIG_SUPPRESS_UART_CONFIG)

  /* Clear fifos */

  putreg32(UART_FCR_RXRST|UART_FCR_TXRST, CONSOLE_BASE+LPC43_UART_FCR_OFFSET);

  /* Set trigger */

  putreg32(UART_FCR_FIFOEN|UART_FCR_RXTRIGGER_8, CONSOLE_BASE+LPC43_UART_FCR_OFFSET);

  /* Set up the LCR and set DLAB=1 */

  putreg32(CONSOLE_LCR_VALUE|UART_LCR_DLAB, CONSOLE_BASE+LPC43_UART_LCR_OFFSET);

  /* Set the BAUD divisor */

  putreg32(CONSOLE_DL >> 8, CONSOLE_BASE+LPC43_UART_DLM_OFFSET);
  putreg32(CONSOLE_DL & 0xff, CONSOLE_BASE+LPC43_UART_DLL_OFFSET);

  /* Clear DLAB */

  putreg32(CONSOLE_LCR_VALUE, CONSOLE_BASE+LPC43_UART_LCR_OFFSET);

  /* Configure the FIFOs */

  putreg32(UART_FCR_RXTRIGGER_8|UART_FCR_TXRST|UART_FCR_RXRST|UART_FCR_FIFOEN,
           CONSOLE_BASE+LPC43_UART_FCR_OFFSET);
#endif
#endif /* HAVE_UART */
}


