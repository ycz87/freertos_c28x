;-------------------------------------------------------------------------------------------------
; Author: Ivan Zaitsev, ivan.zaitsev@gmail.com
;
; This file follows the FreeRTOS distribution license.
;
; FreeRTOS is free software; you can redistribute it and/or modify it under
; the terms of the GNU General Public License (version 2) as published by the
; Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.
;
; ***************************************************************************
; >>!   NOTE: The modification to the GPL is included to allow you to     !<<
; >>!   distribute a combined work that includes FreeRTOS without being   !<<
; >>!   obliged to provide the source code for proprietary components     !<<
; >>!   outside of the FreeRTOS kernel.                                   !<<
; ***************************************************************************
;
; FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
; WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
; FOR A PARTICULAR PURPOSE.  Full license text is available on the following
; link: http://www.freertos.org/a00114.html
;-------------------------------------------------------------------------------------------------

  .ref _pxCurrentTCB
  .ref _bYield
  .ref _bPreemptive
  .ref _xTaskIncrementTick
  .ref _vTaskSwitchContext

  .def _portTICK_ISR
  .def _portRESTORE_FIRST_CONTEXT

  .if .TMS320C2800_FPU32 = 1

  .def _getSTF

_getSTF
  MOV32   *SP++, STF
  POP     ACC
  LRETR

_portRESTORE_FIRST_CONTEXT
; Restore stack pointer from new task control block.
  MOVL    XAR0, #_pxCurrentTCB
  MOVL    XAR0, *XAR0
  MOVL    XAR0, *XAR0
  MOV     @SP, AR0

; Restore XAR4 and RPC from saved task stack.
; and return to main task function.
  SUBB   SP, #23
  POP    XAR4
  SUBB   SP, #14
  POP    RPC
  SUBB   SP, #11
  LRETR

_portTICK_ISR:
; Save context
  ASP          
  PUSH    RB
  PUSH    AR1H:AR0H
  PUSH    RPC
  MOVL    *SP++, XT
  MOVL    *SP++, XAR2
  MOVL    *SP++, XAR3
  MOVL    *SP++, XAR4
  MOVL    *SP++, XAR5
  MOVL    *SP++, XAR6
  MOVL    *SP++, XAR7
  MOV32   *SP++, STF
  MOV32   *SP++, R0H
  MOV32   *SP++, R1H
  MOV32   *SP++, R2H
  MOV32   *SP++, R3H
  MOV32   *SP++, R4H
  MOV32   *SP++, R5H
  MOV32   *SP++, R6H
  MOV32   *SP++, R7H

; Save stack pointer in the task control block.
  MOVL    XAR0, #_pxCurrentTCB
  MOVL    XAR0, *XAR0
  MOV     AR6, @SP
  MOVL    *XAR0, XAR6

; Save IER on stack to avoid corruption.
  MOV     AR7, *-SP[44]
  MOVL    *SP++, XAR7

; Increment tick counter if timer tick is executed.
; Don't increment if explicitly yielded.
  MOVL    XAR0, #_bYield
  MOV     ACC, *XAR0
  SB      RESET_YIELD_FLAG, NEQ
  LCR     _xTaskIncrementTick

RESET_YIELD_FLAG:
; Save bYield in AR1 and clear it in memory.
  MOV     AR1, ACC
  MOV     ACC, #0
  MOV     *XAR0, ACC

; Do context switch if bYield=1 or bPreemptive=1
  MOVL    XAR0, #_bPreemptive
  MOV     ACC, *XAR0
  CMPB    AL, #0x1
  SB      CONTEXT_SWITCH, EQ
  MOV     ACC, AR1
  CMPB    AL, #0x1
  SB      SKIP_CONTEXT_SWITCH, NEQ

CONTEXT_SWITCH:
  LCR     _vTaskSwitchContext

SKIP_CONTEXT_SWITCH:
; Restore IER value from stack.
  MOVL    XAR7, *--SP

; Restore stack pointer from new task control block.
  MOVL    XAR0, #_pxCurrentTCB
  MOVL    XAR0, *XAR0
  MOVL    XAR0, *XAR0
  MOV     @SP, AR0

; Update IER value in target context.
  MOV     *-SP[44], AR7

; Restore context.
  MOV32   R7H, *--SP
  MOV32   R6H, *--SP
  MOV32   R5H, *--SP
  MOV32   R4H, *--SP
  MOV32   R3H, *--SP
  MOV32   R2H, *--SP
  MOV32   R1H, *--SP
  MOV32   R0H, *--SP
  MOV32   STF, *--SP
  MOVL    XAR7, *--SP
  MOVL    XAR6, *--SP
  MOVL    XAR5, *--SP
  MOVL    XAR4, *--SP
  MOVL    XAR3, *--SP
  MOVL    XAR2, *--SP
  MOVL    XT, *--SP
  POP     RPC
  POP     AR1H:AR0H
  POP     RB
  NASP 
  IRET

  .else

_portRESTORE_FIRST_CONTEXT
; Restore stack pointer from new task control block.
  MOVL    XAR0, #_pxCurrentTCB
  MOVL    XAR0, *XAR0
  MOVL    XAR0, *XAR0
  MOV     @SP, AR0

; Restore XAR4 and RPC from saved task stack.
; and return to main task function.
  SUBB   SP, #5
  POP    XAR4
  SUBB   SP, #12
  POP    RPC
  SUBB   SP, #11
  LRETR

_portTICK_ISR:
; Save context
  ASP          
  PUSH    AR1H:AR0H
  PUSH    RPC
  MOVL    *SP++, XT
  MOVL    *SP++, XAR2
  MOVL    *SP++, XAR3
  MOVL    *SP++, XAR4
  MOVL    *SP++, XAR5
  MOVL    *SP++, XAR6
  MOVL    *SP++, XAR7

; Save stack pointer in the task control block.
  MOVL    XAR0, #_pxCurrentTCB
  MOVL    XAR0, *XAR0
  MOV     AR6, @SP
  MOVL    *XAR0, XAR6

; Save IER on stack to avoid corruption.
  MOV     AR7, *-SP[24]
  MOVL    *SP++, XAR7

; Increment tick counter if timer tick is executed.
; Don't increment if explicitly yielded.
  MOVL    XAR0, #_bYield
  MOV     ACC, *XAR0
  SB      RESET_YIELD_FLAG, NEQ
  LCR     _xTaskIncrementTick

RESET_YIELD_FLAG:
; Save bYield in AR1 and clear it in memory.
  MOV     AR1, ACC
  MOV     ACC, #0
  MOV     *XAR0, ACC

; Do context switch if bYield=1 or bPreemptive=1
  MOVL    XAR0, #_bPreemptive
  MOV     ACC, *XAR0
  CMPB    AL, #0x1
  SB      CONTEXT_SWITCH, EQ
  MOV     ACC, AR1
  CMPB    AL, #0x1
  SB      SKIP_CONTEXT_SWITCH, NEQ

CONTEXT_SWITCH:
  LCR     _vTaskSwitchContext

SKIP_CONTEXT_SWITCH:
; Restore IER value from stack.
  MOVL    XAR7, *--SP

; Restore stack pointer from new task control block.
  MOVL    XAR0, #_pxCurrentTCB
  MOVL    XAR0, *XAR0
  MOVL    XAR0, *XAR0
  MOV     @SP, AR0

; Update IER value in target context.
  MOV     *-SP[24], AR7

; Restore context.
  MOVL    XAR7, *--SP
  MOVL    XAR6, *--SP
  MOVL    XAR5, *--SP
  MOVL    XAR4, *--SP
  MOVL    XAR3, *--SP
  MOVL    XAR2, *--SP
  MOVL    XT, *--SP
  POP     RPC
  POP     AR1H:AR0H
  NASP 
  IRET

  .endif
