#define F_CPU 16000000UL

// Define states
#define IDLE 0
#define SETTING 1
#define RUNNING 2
#define PAUSE 3
#define ALARM 4

// notes (Alarm, button)
#define C 262
#define E 330
#define G 392

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// 7segment digits 0~9
const unsigned char Segment_Data[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66,
	0x6D, 0x7D, 0x27, 0x7F, 0x6F};

// Global variants
// timer 7-segment
char COLUMN[4] = {0, 0, 0, 0};
int Seg_number = 0;
int Seg_number12 = 0;
int Seg_number34 = 0;

// initial states
volatile int state = IDLE;
volatile unsigned char flag_button = 0;
volatile int total_seconds = 0;
volatile int set_seconds = 0;
volatile unsigned char timer_tick = 0;
volatile unsigned int mute = 0;

// Function declaration
void Initialize_IO(void);
void Initialize_Timer(void);
void Initialize_PWM(void);
void initialize_adc(void);
void startConversion(void);
unsigned int readConvertData(void);
int adc_to_seconds(unsigned int adc_value);
void Show4Digit(int number);
void ShowDigit(int digit_value, int position);
void Run(void);
void setting_mode_feedback(void);
void play_tone(unsigned int freq, unsigned int duration_ms);
void play_button_sound(void);
void play_alarm(void);
void sound_off(void);
void display_text_SET(void);
void display_text_DONE(void);
void display_text_MUTE(void);
void display_text_BEEP(void);
void select_sound_mode(void);

int main(void)
{
	Initialize_IO();
	Initialize_Timer();
	Initialize_PWM();
	initialize_adc();

	SREG |= 0x80;

	state = IDLE;

	while (1)
	{
		// State transition
		if (flag_button != 0)
		{
			_delay_ms(50); // Debouncing

			// ISR0 : Start/Pause
			if (flag_button & (1 << 0))
			{
				play_button_sound();

				if (state == IDLE)
				{
					state = RUNNING;
				}
				else if (state == RUNNING)
				{
					state = PAUSE;
				}
				else if (state == PAUSE)
				{
					state = RUNNING;
				}
				flag_button = 0;
			}
			// ISR1 : Reset
			else if (flag_button & (1 << 1))
			{
				play_button_sound();

				state = IDLE;
				total_seconds = 0;
				flag_button = 0;
			}
			// ISR2 : Time setting
			else if (flag_button & (1 << 2))
			{
				play_button_sound();

				if (state == IDLE)
				{
					display_text_SET();
					state = SETTING;
				}
				else if (state == SETTING)
				{
					total_seconds = set_seconds;
					display_text_DONE();
					state = IDLE;
				}
				flag_button = 0;
			}
			// ISR3 : Sound / mute
			else if (flag_button & (1 << 3))
			{
				play_button_sound();

				mute = ~mute;

				if (mute != 0)
				{
					display_text_MUTE();
				}
				else
				{
					display_text_BEEP();
				}
				flag_button = 0;
			}

			flag_button = 0;
		}

		// timer - 1 second
		if (timer_tick)
		{
			timer_tick = 0;

			if (state == RUNNING)
			{
				if (total_seconds > 0)
				{
					total_seconds--;
				}
				else
				{ // time over
					state = ALARM;
					play_alarm();
					state = IDLE;
					total_seconds = 0; // protect errors
				}
			}
		}

		Run();

		// 7-Segment display
		Seg_number = Seg_number12 * 100 + Seg_number34;
		Show4Digit(Seg_number);
	}
}

// Initialize setting
void Initialize_IO(void)
{
	DDRC = 0xFF;					 // 7-Segment Digit
	DDRA = 0xFF;					 // 7-Segment Data
	DDRG |= (1 << PG0) | (1 << PG1); // LED

	// External interrupt (INT0, INT1, INT2,INT3)
	EICRA = 0xFF; // rising edge
	EIMSK = 0x0F; // INT0, INT1, INT2, INT3 activate

	PORTC = 0x00;
	PORTA = 0x00;
	PORTG = 0x00;
}

void Initialize_Timer(void)
{
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); // CTC, prescaler 1024
	OCR1A = 15624;									   // 16MHz / 1024 / 1Hz - 1
	TIMSK = (1 << OCIE1A);
}

void Initialize_PWM(void)
{
	DDRB |= (1 << PB4); // OC0
}

void initialize_adc(void)
{
	ADCSRA = 0x00;
	ADMUX = 0x00;  // ADC0
	ADCSRA = 0x87; // ADC activate, prescaler 128
}

// ADC conversion (Voltage - seconds)
void startConversion(void)
{
	ADCSRA |= (1 << ADSC);
}

unsigned int readConvertData(void)
{
	while ((ADCSRA & (1 << ADIF)) == 0)
		;				   // escape when ADC interrupt flag rise
	ADCSRA |= (1 << ADIF); // interrupt flag reset
	return ADC;
}

int adc_to_seconds(unsigned int adc_value)
{
	// 0~1023 → 0~3600s (0~60m)
	unsigned long sec = ((unsigned long)adc_value * 3600UL) / 1023UL;
	if (sec > 3600)
		sec = 3600;
	return (int)sec;
}

// 7-Segment 4 digits display
void Show4Digit(int number)
{
	COLUMN[0] = number / 1000;
	COLUMN[1] = (number % 1000) / 100;
	COLUMN[2] = (number % 100) / 10;
	COLUMN[3] = number % 10;

	for (int i = 0; i < 4; i++)
	{
		ShowDigit(COLUMN[i], i);
		_delay_ms(2);
	}
}

void ShowDigit(int digit_value, int position)
{
	PORTC = ~(0x01 << position);

	// MM.SS format
	if (position == 1)
	{
		PORTA = Segment_Data[digit_value] | 0x80;
	}
	else
	{
		PORTA = Segment_Data[digit_value];
	}
}

// Run by state
void Run(void)
{

	switch (state)
	{
	case IDLE:
	case RUNNING:
	case PAUSE:
		// total_seconds -> MM.SS
		Seg_number12 = total_seconds / 60;
		Seg_number34 = total_seconds % 60;
		break;

	case SETTING:
		setting_mode_feedback();
		break;

	case ALARM:
		// Seg_number12 = 0;
		// Seg_number34 = 0;

		// LED & Alarm sound
		break;
	}
}

void setting_mode_feedback(void)
{
	startConversion();
	unsigned int adc_value = readConvertData();
	set_seconds = adc_to_seconds(adc_value);

	// MM.SS
	Seg_number12 = set_seconds / 60;
	Seg_number34 = set_seconds % 60;
}

// PWM play sound
void play_tone(unsigned int freq, unsigned int duration_ms)
{
	if (freq == 0)
		return;

	TCCR0 = (1 << WGM01) | (1 << COM00) | (1 << CS02); // CTC, Toggle OC0, prescaler 64

	// calculate frequency
	unsigned long ocr_value = F_CPU / (2UL * 64UL * (unsigned long)freq) - 1;
	if (ocr_value > 255)
		ocr_value = 255;
	if (ocr_value < 1)
		ocr_value = 1;
	OCR0 = (unsigned char)ocr_value;

	// wait for duration
	for (unsigned int i = 0; i < duration_ms / 10; i++)
	{
		_delay_ms(10);
	}

	sound_off();
}

void sound_off(void)
{
	TCCR0 = 0;
	PORTB &= ~(1 << PB4);
}

void play_button_sound(void)
{
	play_tone(1000, 100); // 1kHz, 100ms
}

void play_alarm(void)
{
	if (mute != 0)
	{
		// Mute mode
		for (int i = 0; i < 2; i++)
		{
			PORTG |= (1 << PG0) | (1 << PG1); // LED ON
			_delay_ms(200);
			PORTG &= ~((1 << PG0) | (1 << PG1)); // LED OFF
			_delay_ms(200);
		}
	}
	else
	{
		// Beef mode
		PORTG &= ~((1 << PG0) | (1 << PG1)); // LED off
		for (int i = 0; i < 2; i++)
		{
			play_tone(C * 2, 200);
			play_tone(E * 2, 200);
			play_tone(G * 2, 300);
		}
	}
}

// mode select display
void display_text_SET(void)
{
	for (int k = 0; k < 100; k++)
	{
		PORTC = ~(1 << 0);
		PORTA = 0x6D; // S
		_delay_ms(5);
		PORTC = ~(1 << 1);
		PORTA = 0x79; // E
		_delay_ms(5);
		PORTC = ~(1 << 2);
		PORTA = 0x78; // t
		_delay_ms(5);
		PORTC = ~(1 << 3);
		PORTA = 0x80; // .
		_delay_ms(5);
	}
}

void display_text_DONE(void)
{
	for (int k = 0; k < 100; k++)
	{
		PORTC = ~(1 << 0);
		PORTA = 0x5E; // d
		_delay_ms(5);
		PORTC = ~(1 << 1);
		PORTA = 0x3F; // O
		_delay_ms(5);
		PORTC = ~(1 << 2);
		PORTA = 0x37; // n
		_delay_ms(5);
		PORTC = ~(1 << 3);
		PORTA = 0x79; // E
		_delay_ms(5);
	}
}

void display_text_MUTE(void)
{
	for (int k = 0; k < 100; k++)
	{
		PORTC = ~(1 << 0);
		PORTA = 0x37; // n (replace m -> n)
		_delay_ms(5);
		PORTC = ~(1 << 1);
		PORTA = 0x3E; // U
		_delay_ms(5);
		PORTC = ~(1 << 2);
		PORTA = 0x78; // t
		_delay_ms(5);
		PORTC = ~(1 << 3);
		PORTA = 0x79; // E
		_delay_ms(5);
	}
}

void display_text_BEEP(void)
{
	for (int k = 0; k < 100; k++)
	{
		PORTC = ~(1 << 0);
		PORTA = 0x7C; // b
		_delay_ms(5);
		PORTC = ~(1 << 1);
		PORTA = 0x79; // E
		_delay_ms(5);
		PORTC = ~(1 << 2);
		PORTA = 0x79; // E
		_delay_ms(5);
		PORTC = ~(1 << 3);
		PORTA = 0x73; // P
		_delay_ms(5);
	}
}

// Interrupt handler
ISR(INT0_vect)
{
	flag_button |= (1 << 0);
}

ISR(INT1_vect)
{
	flag_button |= (1 << 1);
}

ISR(INT2_vect)
{
	flag_button |= (1 << 2);
}

ISR(INT3_vect)
{
	flag_button |= (1 << 3);
}

ISR(TIMER1_COMPA_vect)
{
	timer_tick = 1;
}
