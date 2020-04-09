/*
 * ECE 3613 Lab 9 Activity 6.c
 *
 * Created: 4/8/2020 12:01:45 AM
 * Author : Leomar Duran <https://github.com/lduran2>
 * Board  : ATmega324PB Xplained Pro - 2505
 * For    : ECE 3612, Spring 2020
 * Self   : <https://github.com/lduran2/ece3612-lab9/blob/master/activity6.c>
 *
 * Performs a cycle that rotates a servo motor counterclockwise for 3
 * seconds, stops for 2 seconds, and rotates clockwise for 5 seconds
 * using timer1 in Normal Mode.
 *
 * @mode_constant E3613L9A6_MODE_RUN signals for the program to run
 * counterclockwise 3 seconds, stop 2 seconds, and clockwise 5 seconds.
 * Otherwise, this program runs stop 1 second continuously for
 * calibration.
 */
#define E3613L9A6_MODE_RUN

#include <avr/io.h>
/* CPU at 16 MHz */
#define	F_CPU	((unsigned long)(16e+6))

/* Below is header stuff.  #main program starts line 114. */


/* *******************************************************************/
/* timer1-related constants:                                         */
/* *******************************************************************/

/* bits on timer 0 */
#define	N_TIMER_BITS	(16)

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
/* bit mask for clearing CS1 */
#define	CLEAR_CS1	(~(CS_EXTERNAL_SOURCE_RISING << CS10))
/* the maximum value scalable + 1 */
#define	SCALEABLE_OVERFLOW_1	((1uL << N_TIMER_BITS) << cs_prescale[CS_EXTERNAL_SOURCE_FALLING - 1])

/* wave generation modes for timer 1 */
typedef enum {
	/* WGM1[3:2] == 0b00 */
	WGM1_NORMAL,
	WGM1_PWM_PHASE_CORRECT_08BIT,
	WGM1_PWM_PHASE_CORRECT_09BIT,
	WGM1_PWM_PHASE_CORRECT_10BIT,
	/* WGM1[3:2] == 0b01 */
	WGM1_CTC_OCR1A = 0b0000100000000000,
	WGM1_PWM_FAST_08BIT,
	WGM1_PWM_FAST_09BIT,
	WGM1_PWM_FAST_10BIT,
	/* WGM1[3:2] == 0b10 */
	WGM1_PWM_PHASE_AND_FREQ_CORRECT_ICR1 = 0b0001000000000000,
	WGM1_PWM_PHASE_AND_FREQ_CORRECT_OCR1A,
	WGM1_PWM_PHASE_CORRECT_ICR1,
	WGM1_PWM_PHASE_CORRECT_OCR1A,
	/* WGM1[3:2] == 0b11 */
	WGM1_CTC_ICR1 = 0b0001100000000000,
	WGM1_REVERSED,
	WGM1_PWM_FAST_ICR1,
	WGM1_PWM_FAST_OCR1A
} WAVE_GENERATION_MODE_1_t;
/* bit mask for clearing WGM1 */
#define	CLEAR_WGM1	(~(WGM1_PWM_FAST_OCR1A << WGM10))


/* *******************************************************************/
/* macros to timer1 accessors:                                       */
/* *******************************************************************/

/**
 * Delays using the timer1 timer represented by `timer` in Normal mode
 * for a given scaled count and with a given clock select.
 * @params
 *    scaled_count : char             = scaled count whereby to delay
 *    clock_select : CLOCK_SELECT_T = the type of source clock to use
 */
#define	DELAY_NORMAL_TIMER1(scaled_count, clock_select)	{\
	/* initialize the counter [(ms * MHz)/<1>] */\
	TCNT1 = -(scaled_count);\
	/* continue the delay from the start */\
	continue_delay_timer1(TOV1, (clock_select));\
} /* end DELAY_NORMAL_TIMER1(scaled_count, clock_select) */


/* *******************************************************************/
/* prototypes to functions:                                          */
/* *******************************************************************/

/*
 * Finds the lowest clock select needed to scale a count for timer1,
 * and scales the count accordingly.
 */
char scale_count_1(
	int *p_scaled_count, CLOCK_SELECT_T *p_clock_select,
	long unsigned prescale_count
);
/* Initializes the timer1 timer. */
void init_normal_timer1(void);
/*
 * Continues the delay using the timer1 timer from where the timer left
 * off, with a given end flag and clock select.
 */
void continue_delay_timer1(char end_flag, CLOCK_SELECT_T clock_select);


/* *******************************************************************/
/* #main program:                                                    */
/* *******************************************************************/

/* the Servo to toggle */
#define	SERVO	(2)

/* the delay for the high signal needed for each state [s] */
#define SERVO_HIGH_CCW	(1.7e-3)
#define SERVO_HIGH_STOP	(1.5e-3)
#define SERVO_HIGH_CW	(1.3e-3)
#define SERVO_LOW	(20e-3)

/* state data depends on run mode or calibration mode */
#ifdef	E3613L9A6_MODE_RUN
	/* count of states: counterclockwise, stop, clockwise */
	#define	N_STATES	(3)

	/* Note: these arrays are in reversed order ! */
	/* state_delays[N_STATES] = the delay for the low signal */
	double state_delays[N_STATES + 1] = {
		SERVO_HIGH_CW, SERVO_HIGH_STOP, SERVO_HIGH_CCW, SERVO_LOW
	};
	/* the periods of each state [s] */
	char state_periods[N_STATES] = { 5, 2, 3 };

#else	/* calibration mode */
	/* count of states: stop */
	#define	N_STATES	(1)

	/* Note: these arrays are in reversed order ! */
	/* state_delays[N_STATES] = the delay for the low signal */
	double state_delays[N_STATES + 1] = {
		SERVO_HIGH_STOP, SERVO_LOW
	};
	/* the periods of each state [s] */
	char state_periods[N_STATES] = { 1 };
#endif	/* E3613L9A6_MODE == E3613L9A6_MODE_CALIBRATE */

int main(void)
{
	/* bit mask for the LED to toggle */
	const char SERVO_MASK = (1 << SERVO);

	/* counter for states */
	unsigned char i_state;
	/* counter for repetitions */
	unsigned char i_rep;

	/* current count before scaling */
	long unsigned prescale_count;

	/* the counts after scaling */
	int scaled_counts[N_STATES + 1];
	/* the prescaling clocks to use */
	CLOCK_SELECT_T clock_selects[N_STATES + 1];
	/* number of times to repeat for each period */
	char PERIOD_REPS[N_STATES];

	/* set DDRA to output for the desired LED */
	DDRD |= SERVO_MASK;

	/* loop through the states, including low state */
	for (i_state = (N_STATES + 1); i_state--; )
	{
		/* calculate the count before scaling */
		prescale_count = ((long unsigned)(state_delays[i_state] * F_CPU));
		/* find the scale and scale the count, */
		if (scale_count_1(
			&scaled_counts[i_state], &clock_selects[i_state], prescale_count
		))
		{
			/* stopping if there was no appropriate scale */
			return 1;
		} /* end if (scale_count_1(, , prescale_count)) */
	} /* end for (i_state = (N_STATES + 1); i_state--; ) */

	/* loop through the states, excluding low state */
	for (i_state = N_STATES; i_state--; )
	{
		/* calculate the repetitions per state */
		PERIOD_REPS[i_state] = ((char)(
			state_periods[i_state]/(state_delays[i_state] + state_delays[N_STATES])
		));
	} /* end for (i_state = N_STATES; i_state--; ) */

	/* initialize the clock in normal mode */
	init_normal_timer1();

	/* loop indefinitely */
	while (1)
	{
		for (int i_state = N_STATES; i_state--; )
		{
			/* loop through the cycles */
			for (i_rep = PERIOD_REPS[i_state]; i_rep--; )
			{
				/* set the servo */
				PORTD |= SERVO_MASK;
				/* hold the servo set using timer1 normal mode*/
				DELAY_NORMAL_TIMER1(scaled_counts[i_state], clock_selects[i_state]);
				/* clear the servo */
				PORTD &= (~SERVO_MASK);
				/* hold the servo clear using timer1 normal mode*/
				DELAY_NORMAL_TIMER1(scaled_counts[N_STATES], clock_selects[N_STATES]);
			} /* end for (i_rep = PERIOD_REPS[i_state]; --i_rep; ) */
		} /* end for (int i_state = N_STATES; i_state--; ) */
	} /* end while (1) */

	return 0;	/* exit with success */
} /* end int main(void) */


/* ****************************************************************//**
 * Finds the lowest clock select needed to scale a count for timer1,
 * and scales the count accordingly.
 * @params
 *   *p_scaled_count : int            = points to count after scaling
 *   *p_clock_select : CLOCK_SELECT_T = points to clock select needed
 *   prescale_count  : long unsigned  = the count before scaling
 * @returns 0 on success, 1 on failure.
 */
char scale_count_1(
	int *p_scaled_count, CLOCK_SELECT_T *p_clock_select,
	long unsigned prescale_count
)
{
	/* integer used to hold scaled_count with overflow */
	long l_scaled_count;

	/* if the prescale count exceeds the scalable max */
	if (prescale_count >= SCALEABLE_OVERFLOW_1)
	{
		return 1; /* exit with failure */
	} /* if (prescale_count >= SCALEABLE_OVERFLOW_1) */

	/* search for the best scale, */
	/* smallest where the scaled count fits in the clock */
	for (
		/* initial clock select */
		*p_clock_select = CS_NO_PRESCALING;
		/* scale and compare */
		((l_scaled_count
			= ((long)(prescale_count
				>> cs_prescale[*p_clock_select]
			))
		)
			> (1L << N_TIMER_BITS)); /* this is a long integer! */
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
	*p_scaled_count = (int)l_scaled_count;

	return 0;	/* exit with success */
} /* end char scale_count_1(
	char *p_scaled_count, CLOCK_SELECT_T *p_clock_select,
	long unsigned prescale_count
)  */


/* ****************************************************************//**
 * Initializes the timer1 timer.
 */
void init_normal_timer1(void)
{
	/* set wave generation mode to normal */
	*((int *)&TCCR1A) &= CLEAR_WGM1;	/* clear */
	*((int *)&TCCR1A) |= (WGM1_NORMAL << WGM10);
} /* end init_normal_timer1(void) */

/* ****************************************************************//**
 * Continues the delay using the timer1 timer from where the timer left
 * off, with a given end flag and clock select.
 * @params
 *    end_flag     : char           = flags the ending of timer1
 *    clock_select : CLOCK_SELECT_T = the type of source clock to use
 */
void continue_delay_timer1(char end_flag, CLOCK_SELECT_T clock_select)
{
	/* select the clock */
	TCCR1B &= CLEAR_CS1; /* clear */
	TCCR1B |= (clock_select << CS10); /* set */

	/* wait while no overflow */
	while (!((TIFR1 >> end_flag) & 1))
	{
		/* NOP */
	} /* end while (!((TIFR1 >> *end_flag) & 1)) */

	/* stop the clock */
	TCCR1B &= CLEAR_CS1; /* clear */
	TCCR1B |= (CS_STOP << CS10); /* set */
	/* clear the end flag */
	TIFR1 |= (1 << end_flag);
} /* end continue_delay_timer1(
	char end_flag, CLOCK_SELECT_T clock_select
)  */
