#define F_CPU 8000000UL

#include <avr/io.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>
#include <math.h>
#include "lcd.h"
#include "fft.h"

/* constants */
#define N 32																								// Number of samples
#define PI_2 2*3.141592654																	// 2*pi
#define DC_BIAS 516																					// level at which signal is biased, we subtract this from each sample
#define FRAME_RATE	30																			// the number of times LCD is updated per second

/* function prototypes */
void adc_init();
uint16_t adc_read();
void timer1_init();
void create_custom_lcd_characters();
void draw_spectrum(cmplx * data);
void fft(unsigned int logN, double *real, double *im);

/* variables */
volatile int count = 0;																			// used for counting the number of samples collected
volatile double fx_real[N];																	// the real part of the samples
volatile double fx_imag[N];																	// the imaginary part of the samples, we don't receive complex samples so this is set to zero

/* custom character definition in flash memory */
static const PROGMEM unsigned char bargraph[] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 					// bargraph 1
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 					// bargraph 2
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 					// bargraph 3
	0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 					// bargraph 4
	0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 					// bargraph 5
	0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 					// bargraph 6
	0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 					// bargraph 7
	0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F  					// bargraph 8
};

int main()
{
	cmplx *input = (cmplx *)calloc(sizeof(cmplx), N);
	cmplx *output = (cmplx *)calloc(sizeof(cmplx), N);
    adc_init();																						// initialize the ADC circuitry

    lcd_init(LCD_DISP_ON);																// initialize the LCD module
    create_custom_lcd_characters();												// load the custom characters in program memory to the LCD

    /* print a welcome message on LCD and hold for 5 seconds */
    lcd_gotoxy(0,0);
    lcd_puts("  Spectrometer  ");
    lcd_gotoxy(0,1);
    lcd_puts("  (0Hz - 4KHz)");
    _delay_ms(5000);

    timer1_init();																				// initialize timer 1

    while(1)
    {
    	/* Number of samples collected has reached N
    	   perform the Fast Fourier Transform
    	   Draw the spectrum on LCD screen and reset sample
    	   count */
    	if(count >= N)
    	{
			int i;
			for(i=0; i<N; i++)
			{
				input[i].real = (float)fx_real[i];
				input[i].imag = (float)fx_imag[i];
			}

			fft_Stockham(input, output, N, -1);
    		draw_spectrum(output);
    		count = 0;
    	}

    	_delay_ms(1000/FRAME_RATE);											// delay for 1000/FRAME_RATE to fulfill the frame rate requirement
    }

    return 0;
}


/* ISR fired whenever a match occurs */
ISR(TIMER1_COMPA_vect)
{
	// collect N number of samples
	if(count < N)
	{
		fx_real[count] = (uint16_t)adc_read() - DC_BIAS;	// remove the DC bias from samples
		fx_imag[count] = 0;																// imaginary part is not received, set it to zero
		count++;																					// incremented sample count
	}
}


/* initialize timer, interrupt and variable */
void timer1_init()
{
	/* timer 1 is set up to achieve a sample rate of 8KHz
	   highest frequency component in received samples
	   will therefore be 4KHz according to Nyquist sampling
	   theorem. Analog filters can be used to filter frequencies above 4KHz*/
	TCCR1B |= (1 << WGM12) | (1 << CS11); 								// set up timer in CTC mode and prescaler = 8
	TCNT1 = 0; 																						// initialize counter
	OCR1A = 124; 																					// initialize compare value
	TIMSK |= (1 << OCIE1A); 															// enable compare interrupt
	sei(); 																								// enable global interrupts
}

/* initialize ADC */
void adc_init()
{
    ADMUX = (1<<REFS0);																	// AREF = AVcc
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS0);				// Enable ADC, prescaler = 32
}

/* read from ADC */
uint16_t adc_read()
{
    ADCSRA |= 1<<ADSC;																	// Start single conversion
    while(ADCSRA & (1<<ADSC));													// wait for conversion to complete
    return ADC;
}

/* load custom characters to the LCD */
void create_custom_lcd_characters()
{
	int i;

	lcd_command(64);																			// set CG RAM start address to 0

	/* write all 64 bytes in the program memory
	   to the LCD */
	for(i=0; i<64; i++)
	{
		lcd_data(pgm_read_byte_near(&bargraph[i]));
	}
}

/* draw the spectrum on the LCD */
void draw_spectrum(cmplx * data)
{
	uint8_t lcd_line1[16];
	uint8_t lcd_line2[16];
	double mag;
	int i;

	for(i=1; i<N/2; i++)
	{
		float real = data[i].real*data[i].real;
		float imag = data[i].imag*data[i].imag;
		float spectra = real + imag;
		mag = sqrt(spectra) / N;												// calculate the magnitude of the spectrum
		mag = (mag / 45.0) * 16;												// scale the magnitude so that if fits nicely of LCD screen
																										// 16 is the number of vertical pixels on LCD
																										// The number 45.0 was determined experimentally, it can vary depending on circuit construction


		/* load spectrum magnitudes into lcd buffers
		   magnitudes fit between numbers 0 and 16, which is 0 - 7
		   per lcd line. The numbers 0 - 7 also represent the
		   custom characters in lcd, with 0 being the smallest bar
		   and 7 being the largest bar */
		if(mag>7)
		{
			lcd_line1[i] = (uint8_t)mag - 7 - 1;					// if the magnitude is bigger than 7, the bar will occupy both line on the lcd
																										// so we subract 7 and 1 to remain with value for the top lcd line

			if(lcd_line1[i] > 7) lcd_line1[i] = 7;				// if the value for the top part of lcd is greater than 7, we just make it 7 so
																										// that it corresponds to character 7 in the CG RAM
			lcd_line2[i] = 7;															// since the magnitude was greater than 7, the bottom part (line2 on lcd) will be 7
		}
		else
		{
			lcd_line1[i] = ' ';														// if magnitude was less than 7, we'll leave line1 on lcd empty
			lcd_line2[i] = (uint8_t)mag;									// line2 on lcd will be equal to mag
		}
	}

	/* plot the spectrum on screen */
	lcd_clrscr();																			// clear the lcd screen
	for(i=1; i<16; i++)
	{
		lcd_gotoxy(i-1, 0);
		lcd_data(lcd_line1[i]);

		lcd_gotoxy(i-1, 1);
		lcd_data(lcd_line2[i]);
	}

}
