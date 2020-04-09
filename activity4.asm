;
; ECE 3613 Lab 9 Activity 4.asm
;
; Created: 4/6/2020 2:21:21 PM
; Author : Leomar Duran <https://github.com/lduran2>
; Board  : ATmega324PB Xplained Pro - 2505
; For    : ECE 3612, Spring 2020
; Self   : <https://github.com/lduran2/ece3612-lab9/blob/master/activity4.asm>
;
; Performs a 75% duty cycle of 10 ms that toggles LED PA5 using Compare
; Match Mode of the 8-bit counter.
;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; CONSTANTS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.equ	F_CPU=16*1000*1000	;CPU at 16 MHz

.equ	N_TIMER_BITS=8	;number of bits on timer 0
.equ	CS0_STOP=0	;timer0 clock select to stop
.equ	CS0_NO_PRESCALING=1	;timer0 clock select with no prescaling
.equ	WGM0_CTC=0b0001	;timer0 compare match mode
.equ	CLEAR_CS0 =0b11111000	;clears the clock select mode for timer0
.equ	CLEAR_WGM0=0b10110111	;clears the wave generation mode for timer0

.equ	LED=5	;the LED to toggle
.equ	PERIOD=10	;period of wave [ms]
.equ	DUTY_N=75	;numerator of duty
.equ	DUTY_D=100	;denominator of duty

.equ	FULL_CYCLE=((PERIOD*F_CPU)/1000)	;full cycle time
.equ	ON_TIME=((DUTY_N*FULL_CYCLE)/DUTY_D)	;time LED will be on
.equ	OFF_TIME=(FULL_CYCLE - ON_TIME)	;time LED will be off


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; MACROS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;LoaD Immediate to Word: loads a word value into a word register
;@params
;  @0:@1 -- the word register (little endian)
;  @2    -- the word value to load
.macro	ldiw
	ldi	@0,high(@2)	;the high byte
	ldi	@1, low(@2)	;the  low byte
.endmacro	;ldiw

;LoaD Immediate to Double Word: loads a 4-byte value into a 4-byte register
;@params
;  @0:@3 -- the 4-byte register (little endian)
;  @4    -- the 4-byte value to load
.macro	ldidw
	ldi	@0,(@4 >> (3 << 3)) & 0xFF	; most significant byte
	ldi	@1,(@4 >> (2 << 3)) & 0xFF
	ldi	@2,(@4 >> (1 << 3)) & 0xFF
	ldi	@3,(@4 >> (0 << 3)) & 0xFF	;least significant byte
.endmacro

;Logical Right Shift Double Word: shifts all bits in a 4-byte register
;once to the right.  Bit 0 of @0:@3 is shifted into the C flag of the
;status register.
;@params
;  @0:@3 -- the 4-byte register (little endian)
.macro	lrsdw
	lsr	@0	;right shift the most significant byte
	ror	@1	;rotate in the carry and left shift
	ror	@2	;rotate in the carry and left shift
	ror	@3	;rotate in the carry and left shift the lowest byte
.endmacro	;lrsw

;BRanch if Not Byte Double Word:
;@params
;  @0:@2 -- the 3 highest bytes of the 4-byte register (R16:R31)
;  @3    -- label whereto, to branch if not a byte
.macro	brnbdw
	cpi	@2,0	;if byte 1 is nonzero
	brne	@3	;branch
	cpi	@1,0	;if byte 2 is nonzero
	brne	@3	;branch
	cpi	@0,0	;if MSB is nonzero
	brne	@3	;branch
.endmacro

;STore Output Compare: scales the time given by @2, then stores the minimum
;prescaled clock select and scaled time
;@params
;  @0 -- output compare register wherein to store the scaled time
;  @1 -- the clock select mode
;  @2 -- time to scale
.macro	stoc
	ldidw	r19,r18,r17,r16,@2	;load the time
	rcall	SCALE_TIME	;scale this time
	out	@0,r16	;store the scaled time
	mov	@1,r20	;store the clock select mode
.endmacro


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; MAIN PROGRAM
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	;set up the stack pointer
	ldiw	r17,r16,RAMEND	;load to registers
	out	SPH,r17	;store the stack pointer high byte
	out	SPL,r16	;store the stack pointer low byte

	;set the LED for output
	in	r16,DDRA	;load in DDRA
	ori	r16, 1 << LED	;or in the LED
	out	DDRA,r16	;update the DDRA

	;set the LED
	in	r16,PORTA	;load in PORTA
	ori	r16, 1 << LED	;or in the LED
	out	PORTA,r16	;update the PORTA

	stoc	ocr0b,r2,ON_TIME	;scale the  on time in 0b
	stoc	ocr0a,r3,OFF_TIME	;scale the off time in 0a

	in	r16,TCCR0B	;load in timer0 clock control B
	andi	r16,CLEAR_WGM0	;clear the wave generation mode
	ori	r16,WGM0_CTC << WGM02	;set to compare match mode
	out	TCCR0B,r16	;update the clock control
	out	TCCR0A,r16	;copy to clock control A

	ldi	r17, 1 << LED	;the bitmask for the LED
	ldi	r19, 1 << OCF0B	;end flag, start with output control B
	ldi	r20, (1 << OCF0B) | (1 << OCF0A)	;end flag toggler

;duty cycle on the LEDs specified by `r17`.
;@params
;  r2  --  on time clock select
;  r3  -- off time clock select
;  r16 -- contents of the clock control register
;  r17 -- the LEDs to cycle
;@returns
;  r4    -- the current clock select
;  r18   -- temporary variable
;  YH:YL -- pointer in r2:r5
DUTY_CYCLE_POINTER:
		; ldiw	XH,XL, 0x20 + TCCR0B	;initialize clock control register
	ldiw	YH,YL,2	;the clock select pointer
DUTY_CYCLE:
	ld	r4,Y+	;load in the clock select
	rcall	DELAY	;run the delay
		;ready for next part of cycle
	; ld	r18,-X	;decrement the clock register
	eor	r19,r20	;toggle the end flag
	;toggle LED
	in	r18,PORTA	;load in PORTA
	eor	r18,r17	;toggle the LED
	out	PORTA,r18	;update PORTA
	;check if the off time is done
		;NOTE:  No longer need the times
		;because they are in output control register.
	cpi	YL,2 + (1*2)	;(r2 + (clock select)*2 toggles)
	breq	DUTY_CYCLE_POINTER	;if so, reinitialize the pointer
	;otherwise
	rjmp	DUTY_CYCLE	;repeat the duty cycle
END:	rjmp END

;scales the time to fit within one byte
;@params
;  r19:r16 -- time to scale to one byte
;@returns
;  r16     -- the time as a byte
;  r20     -- stores the clock select used to prescale
;  r21     -- 0
;  ZH:ZL   -- end of CS0_PRESCALE
SCALE_TIME:
	ldiw	ZH,ZL,	CS0_PRESCALE << 1	;set up Z pointer
	ldi	r20,CS0_NO_PRESCALING	;corresponding clock select mode
SCALE_TIME_FIND:
	;next shift count
	lpm	r21,Z+	;load shift count from program memory
	cpi	r21,-1	;check if the shift counts are terminated
	breq	END	;if so, end the program
SCALE_TIME_SHIFT:
	cpi	r21,0	 ;until zero
	breq	SCALE_TIME_SHIFT_DONE	;continue shifting
	lrsdw	r19,r18,r17,r16	;shift the time
	dec	r21	;decrease the shift count
	rjmp SCALE_TIME_SHIFT	;repeat
SCALE_TIME_SHIFT_DONE:
	;finished shifting
	brnbdw	r19,r18,r17,SCALE_TIME_NEXT	;next if the double word is not a byte
	ret
SCALE_TIME_NEXT:
	;otherwise
	inc	r20	;increase the clock select
	rjmp	SCALE_TIME_FIND	;try again

;delays timer0 in compare match mode, counting from 0 until output
;match
;@params
;  r4  -- the clock select
;  r16 -- contents of the clock control registers
;@returns
;  r18 -- used as a temporary variable by TCNT0 and TIFR0
DELAY:
	ldi	r18,0	;to timer0 count
	out	TCNT0,r18	;initialize to zero
	;initialize the timer0 clock control register
	andi	r16,CLEAR_CS0	;clear the clock select
	or	r16,r4	;set to the appropriate clock select
		; st	X,r16	;update the clock control
	out	TCCR0B,r16	;update the clock control
DELAY_LOOP:
	in	r18,TIFR0	;load in timer0 interrupt flag register
	and	r18,r19	;until overflow
	breq	DELAY_LOOP	;continue delaying
	;otherwise, stop the clock
	andi	r16,CLEAR_CS0	;clear the clock select
	ori	r16,CS0_STOP	;set the clock select to stop
		; st	X,r16	;update the clock control
	out	TCCR0B,r16	;update the clock control
	;clear the end flag
	in	r18,TIFR0	;load in timer0 interrupt flag register
	or	r18,r19	;set the end flag
	out	TIFR0,r18	;update the timer0 interrupt flag register
	ret	;done delaying


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; PROGRAM MEMORY
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;list of scalings, differences of log base 2
;[CS0_NO_PRESCALING .. CS0_PRESCALING_1024]
CS0_PRESCALE:
	.db	0, 3, 3, 2, 2, -1	; -1 terminated
