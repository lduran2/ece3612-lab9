/*
 * ECE 3613 Lab 9 Activity 1.c
 *
 * Created: 4/1/2020 3:01:33 PM
 * Author : Leomar Duran <https://github.com/lduran2>
 * Board  : ATmega324PB Xplained Pro - 2505
 * For    : ECE 3612, Spring 2020
 * Self   : <https://github.com/lduran2/ece3612-lab9/blob/master/activity1.c>
 *
 * Performs a duty cycle of 10 ms that toggles LED PA5 using Normal
 * Mode of the 8-bit counter.
 */
#include <avr/io.h>
/* CPU at 16 MHz */
#define	F_CPU	((unsigned long)(16e+6))

/* Below is header stuff.  #main program starts line 97. */


/* *******************************************************************/
/* timer0-related constants:                                         */
/* *******************************************************************/

/* bits on timer 0 */
#define	N_TIMER_BITS	(8)

/* clock select modes for timer 0 */
typedef enum {
	CS_STOP,
	CS_NO_PRESCALING,
	CS_PRESCALING_8,
	CS_PRESCALING_64,
	CS_PRESCALING_256,
	CS_PRESCALING_1024,
	CS_EXTERNAL_SOURCE_FALLING,
	CS_EXTERNAL_SOURCE_RISING
} CLOCK_SELECT_T;
/* list of scalings, log base 2 */
char cs_prescale[] = { 0, 0, 3, 6, 8, 10, 0, 0 };
/* bit mask for clearing CS0 */
#define	CLEAR_CS0	(~(CS_EXTERNAL_SOURCE_RISING << CS00))
/* the maximum value scalable + 1 */
#define	SCALEABLE_OVERFLOW_0	((1uL << N_TIMER_BITS) << cs_prescale[CS_EXTERNAL_SOURCE_FALLING - 1])

/* wave generation modes for timer 0 */
typedef enum {
	WGM0_NORMAL,
	WGM0_CTC,
	WGM0_PWM_PHASE_CORRECT = 0b1000, /* skip COM0[1:0] */
	WGM0_FAST_PWM
} WAVE_GENERATION_MODE_0_t;
/* bit mask for clearing WGM0 */
#define	CLEAR_WGM0	(~(WGM0_FAST_PWM << WGM02))


/* *******************************************************************/
/* macros to timer0 accessors:                                       */
/* *******************************************************************/

/**
 * Delays using the timer0 timer in Normal mode for a given scaled
 * count and with a given clock select.
 * @params
 *    scaled_count : char             = scaled count whereby to delay
 *    clock_select : CLOCK_SELECT_T = the type of source clock to use
 */
#define	DELAY_NORMAL_TIMER0(scaled_count, clock_select)	{\
	/* initialize the counter [(ms * MHz)/<1>] */\
	TCNT0 = -(scaled_count);\
	/* continue the delay from the start */\
	continue_delay_timer0(TOV0, (clock_select));\
} /* end DELAY_NORMAL_TIMER0(scaled_count, clock_select) */


/* *******************************************************************/
/* prototypes to functions:                                          */
/* *******************************************************************/

/*
 * Finds the lowest clock select needed to scale a count, and scales
 * the count accordingly.
 */
char scale_count_0(
	char *p_scaled_count, CLOCK_SELECT_T *p_clock_select,
	long unsigned prescale_count
);
/* Initializes the timer0 timer. */
void init_normal_timer0();
/*
 * Continues the delay using the timer0 timer from where the timer left
 * off, with a given end flag and clock select.
 */
void continue_delay_timer0(char end_flag, CLOCK_SELECT_T clock_select);


/* *******************************************************************/
/* #main program:                                                    */
/* *******************************************************************/

/* the LED to toggle */
#define	LED	(5)
/* rate of activity */
#define	DUTY_RATE	(0.5)
/* the cycle period [s] */
#define	PERIOD	(10e-3)

int main(void)
{
	/* bit mask for the LED to toggle */
	const char LED_MASK = (1 << LED);

	/* count before scaling */
	long unsigned prescale_count;
	/* count after scaling */
	char scaled_count;
	/* the prescaling clock to use */
	CLOCK_SELECT_T clock_select;

	/* set DDRA to output for the desired LED */
	DDRA |= LED_MASK;
	/* set the desired LED */
	PORTA |= LED_MASK;

	/* calculate the count before scaling */
	prescale_count = ((long unsigned)((DUTY_RATE * PERIOD) * F_CPU));
	/* find the scale and scale the count, */
	if (scale_count_0(&scaled_count, &clock_select, prescale_count))
	{
		/* stopping if there was no appropriate scale */
		return 1;
	} /* end if (scale_count_0(, , prescale_count)) */

	/* initialize the clock in normal mode */
	init_normal_timer0();

	/* loop indefinitely */
	while (1)
	{
		/* hold the LED using timer0 normal mode*/
		DELAY_NORMAL_TIMER0(scaled_count, clock_select);
		/* toggle the LED */
		PORTA ^= LED_MASK;
	} /* end while (1) */

	return 0;	/* exit with success */
} /* end int main(void) */


/* ****************************************************************//**
 * Finds the lowest clock select needed to scale a count, and scales
 * the count accordingly.
 * @params
 *   *p_scaled_count : char           = points to count after scaling
 *   *p_clock_select : CLOCK_SELECT_T = points to clock select needed
 *   prescale_count  : long unsigned  = the count before scaling
 * @returns 0 on success, 1 on failure.
 */
char scale_count_0(
	char *p_scaled_count, CLOCK_SELECT_T *p_clock_select,
	long unsigned prescale_count
)
{
	/* integer used to hold scaled_count with overflow */
	int i_scaled_count;

	/* if the prescale count exceeds the scalable max */
	if (prescale_count >= SCALEABLE_OVERFLOW_0)
	{
		return 1; /* exit with failure */
	} /* if (prescale_count >= SCALEABLE_OVERFLOW_0) */

	/* search for the best scale, */
	/* smallest where the scaled count fits in the clock */
	for (
		/* initial clock select */
		*p_clock_select = CS_NO_PRESCALING;
		/* scale and compare */
		((i_scaled_count
			= ((int)(prescale_count
				>> cs_prescale[*p_clock_select]
			))
		)
			> (1 << N_TIMER_BITS));
		/* next clock select */
		++(*p_clock_select)
	)
	{
		/* NOP */
	} /* end for (;
		((int)(prescale_count
			>> cs_prescale[*p_clock_select]
		))
			< (1 << N_TIMER_BITS);
	)  */

	/* cast and store the scaled count */
	*p_scaled_count = (char)i_scaled_count;

	return 0;	/* exit with success */
} /* end char scale_count_0(
	char *p_scaled_count, CLOCK_SELECT_T *p_clock_select,
	long unsigned prescale_count
)  */


/* ****************************************************************//**
 * Initializes the timer0 timer.
 * @see #INIT_NORMAL_TIMER0
 */
void init_normal_timer0(void)
{
	/* set wave generation mode to normal */
	TCCR0B &= CLEAR_WGM0;	/* clear */
	TCCR0B |= (WGM0_NORMAL << WGM02); /* set */
} /* end init_normal_timer0(void) */

/* ****************************************************************//**
 * Continues the delay using the timer0 timer from where the timer left
 * off, with a given end flag and clock select.
 * @params
 *    end_flag     : char           = flags the ending of timer0
 *    clock_select : CLOCK_SELECT_T = the type of source clock to use
 */
void continue_delay_timer0(char end_flag, CLOCK_SELECT_T clock_select)
{
	/* select the clock */
	TCCR0B &= CLEAR_CS0; /* clear */
	TCCR0B |= (clock_select << CS00); /* set */

	/* wait while no overflow */
	while (!((TIFR0 >> end_flag) & 1))
	{
		/* NOP */
	} /* end while (!((TIFR0 >> *end_flag) & 1)) */

	/* stop the clock */
	TCCR0B &= CLEAR_CS0; /* clear */
	TCCR0B |= (CS_STOP << CS00); /* set */
	/* clear the end flag */
	TIFR0 |= (1 << end_flag);
} /* end continue_delay_timer0(
	char end_flag, CLOCK_SELECT_T clock_select
)  */
