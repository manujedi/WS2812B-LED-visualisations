//#define AVR
//#define PC


#ifdef PC

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#include <signal.h>
#include <fcntl.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif

#ifdef AVR
#define F_CPU 16000000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#endif

#define LEDS 256
#define LEDS_PER_STRIP 16
#define LEDS_PER_ROW (LEDS/LEDS_PER_STRIP)
#define BYTES_PER_LED 3

#define GOL_SIZE LEDS
#define GOL_COLMS (LEDS/LEDS_PER_STRIP)
#define GOL_ROWS LEDS_PER_STRIP

#ifdef AVR
#define n_D62 asm("nop");
#define n_D125 n_D62 n_D62
#define n_D250 n_D125 n_D125
#define n_D500 n_D250 n_D250
#define u_D1 n_D500 n_D500
#endif

#ifdef PC
SDL_Window* win;
SDL_Renderer* renderer;

//some things to let it compile
#define n_D62  SDL_Delay(1);
#define n_D125 SDL_Delay(1);
#define n_D250 SDL_Delay(1);
#define n_D500 SDL_Delay(1);
#define u_D1   SDL_Delay(1);

uint8_t PORTB;
uint8_t PORTF;
uint8_t PORTK;
uint8_t DDRB;
uint8_t DDRF;
uint8_t DDRK;
uint8_t UBRR0H;
uint8_t UBRR0L;
uint8_t UCSR0A;
uint8_t UCSR0B;
uint8_t UCSR0C;
uint8_t UDR0;

#define _delay_ms(X) SDL_Delay(X)
#define sleep_mode() while(1){_delay_ms(1000);}
#define sei() ;

#endif


/* 0 = Grün
 * 1 = Rot
 * 2 = Blau
 */
uint8_t leds[BYTES_PER_LED * LEDS];

uint8_t gol_field[GOL_SIZE / 8];
uint8_t gol_h[(GOL_ROWS / 8) * 3];

uint32_t seed = 2345;
uint32_t a = 48271u;
uint32_t m = 0x7fffffff;

#define BUFFER_SIZE 100

struct Buffer
{
	uint8_t data[BUFFER_SIZE];
	uint8_t read; // zeigt auf das Feld mit dem ältesten Inhalt
	uint8_t write; // zeigt immer auf leeres Feld
};

struct Buffer bufferIn = {{}, 0, 0};
struct Buffer bufferOut = {{}, 0, 0};

uint8_t uart_in_save(uint8_t byte)
{
	//if (buffer.write >= BUFFER_SIZE)
	//  buffer.write = 0; // erhöht sicherheit

	if ((bufferIn.write + 1 == bufferIn.read) ||
			(bufferIn.read == 0 && bufferIn.write + 1 == BUFFER_SIZE))
		return 1; // voll

	bufferIn.data[bufferIn.write] = byte;

	bufferIn.write++;
	if (bufferIn.write >= BUFFER_SIZE)
		bufferIn.write = 0;

	return 0;
}

uint8_t uart_in_read(uint8_t* pByte)
{
	if (bufferIn.read == bufferIn.write)
		return 1;

	*pByte = bufferIn.data[bufferIn.read];

	bufferIn.read++;
	if (bufferIn.read >= BUFFER_SIZE)
		bufferIn.read = 0;

	return 0;
}

uint8_t uart_out_save(uint8_t byte)
{
	//if (buffer.write >= BUFFER_SIZE)
	//  buffer.write = 0; // erhöht sicherheit

	if ((bufferOut.write + 1 == bufferOut.read) ||
			(bufferOut.read == 0 && bufferOut.write + 1 == BUFFER_SIZE))
		return 1; // voll

	bufferOut.data[bufferOut.write] = byte;

	bufferOut.write++;
	if (bufferOut.write >= BUFFER_SIZE)
		bufferOut.write = 0;

	return 0;
}

uint8_t uart_out_read(uint8_t* pByte)
{
	if (bufferOut.read == bufferOut.write)
		return 1;

	*pByte = bufferOut.data[bufferOut.read];

	bufferOut.read++;
	if (bufferOut.read >= BUFFER_SIZE)
		bufferOut.read = 0;

	return 0;
}

void init_uart()
{
	UBRR0H = 0x00;
	UBRR0L = 103;    //16MHz
	UCSR0A = 0x00;    //U2X auf LO
	UCSR0B = 0x00;    //clear
	UCSR0B |= 0x80;    //Rx Interrupt enable
	UCSR0B |= 0x40;    //Tx Interrupt enable
	UCSR0B |= 0x10;    //Rx enable
	UCSR0B |= 0x08;    //Tx enable
	UCSR0C = 0x00;    //clear
	UCSR0C |= 0x04;    //8datenbits
	UCSR0C |= 0x02;    //8datenbits

	sei();
}

#ifdef AVR
void uart_send()
{
	if (bufferOut.read != bufferOut.write)
	{
		uint8_t c;
		uart_out_read(&c);
		UDR0 = c;
	}
}
#endif

#ifdef PC

void uart_pc_helper(int param)
{
	char c = 0;
	while (c != '\n')
	{
		c = getchar();
		uart_in_save(c);
	}
	fflush(stdout);
}

void uart_send()
{
	while (bufferOut.read != bufferOut.write)
	{
		uint8_t c;
		uart_out_read(&c);
		putchar(c);
		fflush(stdout);
	}
}

#endif

#ifdef AVR
ISR(USART0_RX_vect){
		uart_in_save(UDR0);
}

ISR(USART0_TX_vect){
		uart_send();
}
#endif


uint32_t rand_seed()
{
	seed = (a * seed) % m;
	return seed;
}


void printF(uint8_t pin, uint8_t* data)
{

	uint8_t cur = 0;
	uint16_t itt = 0;
	uint8_t i = 0;

	uint8_t pin_ = (1 << pin);
	uint8_t notpin_ = ~(1 << pin);

	cur = *data;

	while (itt++ < (LEDS_PER_STRIP * BYTES_PER_LED))
	{

		while (i < 7)
		{
			if (cur & 0x80)
			{
				PORTF |= pin_;
				i++;
				cur = cur << 1;
				n_D500
				PORTF &= notpin_;
			}
			else
			{
				PORTF |= pin_;
				n_D125
				PORTF &= notpin_;
				i++;
				cur = cur << 1;
				n_D250    //Lower end, try adding 125ns
			}
		}

		//Letztes Bit extra, da nächste daten berechnen
		if (cur & 0x80)
		{
			PORTF |= pin_;
			data++;
			cur = *data;
			i = 0;
			n_D500
			PORTF &= notpin_;
		}
		else
		{
			PORTF |= pin_;
			n_D125
			PORTF &= notpin_;
			data++;
			cur = *data;
			i = 0;
			n_D250    //Lower end, try adding 125ns
		}
	}
	//reset
	PORTF &= notpin_;
}

void printK(uint8_t pin, uint8_t* data)
{

	uint8_t cur = 0;
	uint16_t itt = 0;
	uint8_t i = 0;
	uint8_t pin_ = (1 << pin);
	uint8_t notpin_ = ~(1 << pin);

	cur = *data;

	while (itt++ < (LEDS_PER_STRIP * BYTES_PER_LED))
	{
		while (i < 7)
		{
			if (cur & 0x80)
			{
				PORTK |= pin_;
				i++;
				cur = cur << 1;
				n_D500
				PORTK &= notpin_;
			}
			else
			{
				PORTK |= pin_;
				n_D125
				PORTK &= notpin_;
				i++;
				cur = cur << 1;
				n_D250    //Lower end, try adding 125ns
			}
		}

		//Letztes Bit extra, da nächste daten berechnen
		if (cur & 0x80)
		{
			PORTK |= pin_;
			data++;
			cur = *data;
			i = 0;
			n_D500
			PORTK &= notpin_;
		}
		else
		{
			PORTK |= pin_;
			n_D125
			PORTK &= notpin_;
			data++;
			cur = *data;
			i = 0;
			n_D250    //Lower end, try adding 125ns
		}
	}
	//reset
	PORTK &= notpin_;
}


uint8_t getGol(uint32_t index)
{
	if (gol_field[(index % GOL_SIZE) / 8] & (1 << (index % 8)))
		return 1;
	else
		return 0;
}

void setGol(uint32_t index, uint8_t value)
{
	if (value)
		gol_field[(index % GOL_SIZE) / 8] |= (1 << (index % 8));
	else
		gol_field[(index % GOL_SIZE) / 8] &= ~(1 << (index % 8));
}

uint8_t getGolHelper(uint32_t index)
{
	if (gol_h[(index % (3 * GOL_ROWS)) / 8] & (1 << (index % 8)))
		return 1;
	else
		return 0;
}

void setGolHelper(uint32_t index, uint8_t value)
{
	if (value)
		gol_h[(index % (3 * GOL_ROWS)) / 8] |= (1 << (index % 8));
	else
		gol_h[(index % (3 * GOL_ROWS)) / 8] &= ~(1 << (index % 8));
}

void calcGOL()
{

	uint16_t offset = 0;
	uint32_t i = 0;

	//Spalte n,1 in Helper 2,3
	while (i < GOL_ROWS * 2)
	{
		setGolHelper(i + GOL_ROWS, getGol(GOL_SIZE - GOL_ROWS + i));
		i++;
	}

	while (offset != GOL_COLMS)
	{
		//Helper: Spalte 2,3 -> 1,2
		i = 0;
		while (i < 2 * GOL_ROWS)
		{
			setGolHelper(i, getGolHelper(i + GOL_ROWS));
			i++;
		}

		//Neue Spalte in Helper 3
		//Bsp. erste spalte 0*16 + 1*16 = 32
		i = offset * GOL_ROWS + GOL_ROWS;
		while (i < offset * GOL_ROWS + 2 * GOL_ROWS)
		{
			setGolHelper((i % GOL_ROWS) + (2 * GOL_ROWS), getGol(i));
			i++;
		}

		uint16_t j = GOL_ROWS;
		i = GOL_ROWS;
		while (i)
		{
			uint8_t neighbours;
			//unterste Reihe
			if (j == GOL_ROWS)
			{
				neighbours = getGolHelper(j - 1) +
						getGolHelper(j + GOL_ROWS) +
						getGolHelper(j - GOL_ROWS + 1) +
						getGolHelper(j + GOL_ROWS - 1) +
						getGolHelper(j + 1) +
						getGolHelper(j + 2 * GOL_ROWS - 1) +
						getGolHelper(j + GOL_ROWS) +
						getGolHelper(j + GOL_ROWS + 1);
			}
				//oberste Reihe
			else if (j == 2 * GOL_ROWS - 1)
			{
				neighbours = getGolHelper(j - 2 * GOL_ROWS + 1) +
						getGolHelper(j - GOL_ROWS) +
						getGolHelper(j - GOL_ROWS - 1) +
						getGolHelper(j - GOL_ROWS + 1) +
						getGolHelper(j - 1) +
						getGolHelper(j + 1) +
						getGolHelper(j + GOL_ROWS) +
						getGolHelper(j + GOL_ROWS - 1);
			}
			else
			{
				neighbours = getGolHelper(j - GOL_ROWS + 1) +
						getGolHelper(j - GOL_ROWS) +
						getGolHelper(j - GOL_ROWS - 1) +
						getGolHelper(j + 1) +
						getGolHelper(j - 1) +
						getGolHelper(j + GOL_ROWS - 1) +
						getGolHelper(j + GOL_ROWS) +
						getGolHelper(j + GOL_ROWS + 1);
			}

			//living
			if (getGolHelper(j))
			{
				if (neighbours < 2 || neighbours > 3)
					setGol(j - GOL_ROWS + offset * GOL_ROWS, 0);
			}
				//dead
			else
			{
				if (neighbours == 3)
					setGol(j - GOL_ROWS + offset * GOL_ROWS, 1);
			}
			j++;
			i--;
		}
		offset++;
	}
}

void setLed(uint16_t led, uint8_t red, uint8_t green, uint8_t blue)
{
	leds[led * BYTES_PER_LED + 0] = green;
	leds[led * BYTES_PER_LED + 1] = red;
	leds[led * BYTES_PER_LED + 2] = blue;
}

void setLed_xy(uint16_t x, uint16_t y, uint8_t red, uint8_t green, uint8_t blue)
{
	leds[(x * LEDS_PER_STRIP + y) * BYTES_PER_LED + 0] = green;
	leds[(x * LEDS_PER_STRIP + y) * BYTES_PER_LED + 1] = red;
	leds[(x * LEDS_PER_STRIP + y) * BYTES_PER_LED + 2] = blue;
}

void ledcopy(uint16_t src, uint16_t dest)
{
	leds[dest * 3] = leds[src * 3];
	leds[dest * 3 + 1] = leds[src * 3 + 1];
	leds[dest * 3 + 2] = leds[src * 3 + 2];
}

void ledcopy_xy(uint16_t x_s, uint16_t y_s, uint16_t x_d, uint16_t y_d)
{
	uint16_t led_source = (x_s * LEDS_PER_STRIP + y_s);
	uint16_t led_dest = (x_d * LEDS_PER_STRIP + y_d);
	leds[led_dest * BYTES_PER_LED + 0] = leds[led_source * BYTES_PER_LED + 0];
	leds[led_dest * BYTES_PER_LED + 1] = leds[led_source * BYTES_PER_LED + 1];
	leds[led_dest * BYTES_PER_LED + 2] = leds[led_source * BYTES_PER_LED + 2];
}

void print()
{
#ifdef AVR
	uint8_t* start = leds;

	//First 128 Leds on PortF
	for (uint8_t j = 0; j < 8; ++j)
	{
		printF(j, start);
		start += LEDS_PER_STRIP * BYTES_PER_LED;
	}

	//Second 128 Leds on PortK
	for (uint8_t j = 0; j < 8; ++j)
	{
		printK(j, start);
		start += LEDS_PER_STRIP * BYTES_PER_LED;
	}



#endif

#ifdef PC
/*
	//Cleanup
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
*/
	int h;
	int w;
	SDL_GetRendererOutputSize(renderer, &w, &h);

	int rect_w = w / (LEDS / LEDS_PER_STRIP);
	int rect_h = h / LEDS_PER_STRIP;

	for (int i = 0; i < LEDS; ++i)
	{
		int x = rect_w * (i / LEDS_PER_STRIP);
		int y = (h - rect_h) - (i % LEDS_PER_STRIP) * rect_h;

		SDL_Rect rect;
		rect.h = rect_h;
		rect.w = rect_w;
		rect.x = x;
		rect.y = y;

		SDL_SetRenderDrawColor(renderer, leds[i * 3 + 1], leds[i * 3 + 0], leds[i * 3 + 2], 255);
		SDL_RenderFillRect(renderer, &rect);

	}
	SDL_RenderPresent(renderer);
#endif

}


//Farbverlauf

int loop_farben(void)
{
	uint8_t state = 0;
	uint8_t red = 0;
	uint8_t green = 255;
	uint8_t blue = 0;

	for (int j = LEDS - 1; j >= 0; --j)
	{
		setLed(j, red, green, 0);
		green--;
		red++;
	}
	green = 0;

	int i = 0;
	while (1)
	{
		print();

		//Farbkreis durchgehen
		if (state == 0)
		{
			red--;
			blue++;
			if (blue == 255)
			{
				red = 0;
				state = 1;
			}
		}
		else if (state == 1)
		{
			blue--;
			green++;
			if (green == 255)
			{
				blue = 0;
				state = 2;
			}
		}
		else if (state == 2)
		{
			green--;
			red++;
			if (red == 255)
			{
				green = 0;
				state = 0;
			}
		}

		//Erste led setzten
		setLed(0, red, green, blue);

		//Alle leds eins weiter
		i = LEDS;
		while (i > 1)
		{
			i--;
			ledcopy(i - 1, i);
		}
		//xmit('a');
		_delay_ms(100);
	}

	return 0;
}


//Game of Life
int loop_gol(void)
{

	//define preset here

	while (1)
	{
		_delay_ms(200);

		for (int j = 0; j < LEDS; ++j)
		{
			if (getGol(j))
				setLed(j, 0, 0, 0xFF);
			else
				setLed(j, 0, 0, 0);
		}

		print();
		calcGOL();
	}
	return 0;
}

//Game of Life
int loop_gol_big(void)
{
	while (1)
	{

		//Fill with random data
		for (int j = 0; j < GOL_SIZE / 2; ++j)
		{
			int randField = rand_seed() % GOL_SIZE;
			//gol_field[randField] = 1;
			setGol(randField, 1);
		}

		//Play
		for (int k = 0; k < 10; ++k)
		{
			for (int i = 0; i < LEDS; ++i)
			{
				if (getGol(i))
				{
					leds[i * 3 + 0] = 0xFF;
					leds[i * 3 + 1] = 0xFF;
					leds[i * 3 + 2] = 0xFF;
				}
				else
				{
					leds[i * 3 + 0] = 0;
					leds[i * 3 + 1] = 0;
					leds[i * 3 + 2] = 0;
				}
			}
			print();
			calcGOL();
		}

	}
	return 0;
}


//Random mit GOL
int loop_rand_gol(void)
{
	uint8_t state = 0;
	uint8_t red = 0;
	uint8_t blue = 0;
	uint8_t green = 0;

	int i = 0;
	while (i < LEDS)
	{
		i++;
	}

	while (1)
	{

		print();
		if (rand_seed() % 0x100 > 180)
		{
			int randLed = rand_seed() % LEDS;

			leds[randLed * 3 + 0] = rand_seed();
			leds[randLed * 3 + 1] = rand_seed();
			leds[randLed * 3 + 2] = rand_seed();
		}


		for (int j = 0; j < 3 * LEDS; ++j)
		{
			if (leds[j])
				leds[j] -= 1;
		}

		i = 100;
		if (rand_seed() % 0x100 == 0)
		{
			for (int j = 0; j < LEDS; ++j)
			{
				//gol_field[j] = (leds[j * 3] + leds[j * 3 + 1] + leds[j * 3 + 2]) ? 1 : 0;
				setGol(j, (leds[j * 3] + leds[j * 3 + 1] + leds[j * 3 + 2]) ? 1 : 0);
			}

			while (i)
			{
				//Farbkreis durchgehen
				if (state == 0)
				{
					red--;
					blue++;
					if (blue == 255)
					{
						red = 0;
						state = 1;
					}
				}
				else if (state == 1)
				{
					blue--;
					green++;
					if (green == 255)
					{
						blue = 0;
						state = 2;
					}
				}
				else if (state == 2)
				{
					green--;
					red++;
					if (red == 255)
					{
						green = 0;
						state = 0;
					}
				}

				for (int j = 0; j < LEDS; ++j)
				{
					//if (gol_field[j])
					if (getGol(j))
						setLed(j, red, green, blue);
					else
						setLed(j, 0, 0, 0);
				}
				_delay_ms(100);
				print();
				calcGOL();
				i--;
			}
		}
	}
	return 0;
}

void loop_sun()
{
	//Has to be a square field

	uint8_t red = 0;
	uint8_t blue = 0;
	uint8_t green = 0;

	print();

	while (1)
	{
		setLed_xy(0, 0, red, green, blue);
		if (red < 249)
			red += 5;
		if (green < 200)
			green += 4;
		if (blue < 42)
			blue += 1;

		for (int j = LEDS_PER_STRIP - 1; j > 0; --j)
		{
			//printf("---------ROW: %i----------\n", j);
			//printf("x: %i y: %i to x: %i y: %i\n", j-1, j - 1, j, j);
			ledcopy_xy(j - 1, j - 1, j, j);
			for (int inner = j - 1; inner >= 0; --inner)
			{
				//printf("x: %i y: %i to x: %i y: %i\n", inner, j - 1, inner, j);
				ledcopy_xy(inner, j - 1, inner, j);
			}
			for (int inner = j - 1; inner >= 0; --inner)
			{
				//printf("x: %i y: %i to x: %i y: %i\n", j - 1, inner, j, inner);
				ledcopy_xy(j - 1, inner, j, inner);
			}
			_delay_ms(10);
		}

		print();
		_delay_ms(10);


	}
}

//source
//http://www.codecodex.com/wiki/Calculate_an_integer_square_root

unsigned int sqrt32(unsigned long n)
{
	uint32_t c = 0x8000;
	uint32_t g = 0x8000;

	for (;;)
	{
		if (g * g > n)
			g ^= c;
		c >>= 1;
		if (c == 0)
			return g;
		g |= c;
	}
}

void loop_drop()
{
	uint16_t normal = 255 / sqrt32(2 * ((LEDS_PER_STRIP / 2) * 10) * ((LEDS_PER_ROW / 2) * 10));
	int16_t d_x;
	int16_t d_y;
	uint8_t x;
	uint8_t y;
	uint16_t diff;

	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;
	uint8_t state = 0;

	while (1)
	{
		//Farbkreis durchgehen
		if (state == 0)
		{
			red--;
			blue++;
			if (blue == 255)
			{
				red = 0;
				state = 1;
			}
		}
		else if (state == 1)
		{
			blue--;
			green++;
			if (green == 255)
			{
				blue = 0;
				state = 2;
			}
		}
		else if (state == 2)
		{
			green--;
			red++;
			if (red == 255)
			{
				green = 0;
				state = 0;
			}
		}

		for (int i = 0; i < LEDS; ++i)
		{

			x = i / LEDS_PER_ROW;
			y = i % LEDS_PER_STRIP;

			d_x = LEDS_PER_ROW / 2 - x;
			//mult*10 for accuracy
			d_x *= 10;
			d_x = (d_x < 0) ? -d_x : d_x;

			d_y = LEDS_PER_STRIP / 2 - y;
			d_y *= 10;
			d_y = (d_y < 0) ? -d_y : d_y;

			diff = sqrt32(d_x * d_x + d_y * d_y);

			setLed_xy(x, y, ((normal * diff) * red) / 255, ((normal * diff) * green) / 255,
					  ((normal * diff) * blue) / 255);
		}
		_delay_ms(10);
		print();
	}

}

void loop_blue()
{
	for (int i = 0; i < LEDS; ++i)
	{
		setLed(i, 0, 0, 0xFF);
	}
	print();
	sleep_mode();
}

void loop_dick()
{
	uint8_t pen_red = 255;
	uint8_t pen_green = 239;
	uint8_t pen_blue = 213;

	uint8_t ejac_red = 0xFF;
	uint8_t ejac_green = 0xFF;
	uint8_t ejac_blue = 0xFF;

	uint16_t delay = 200;

	//ball one
	setLed_xy(0, 0, pen_red, pen_green, pen_blue);
	setLed_xy(0, 1, pen_red, pen_green, pen_blue);
	setLed_xy(1, 2, pen_red, pen_green, pen_blue);
	setLed_xy(1, 3, pen_red, pen_green, pen_blue);
	setLed_xy(2, 4, pen_red, pen_green, pen_blue);
	setLed_xy(3, 3, pen_red, pen_green, pen_blue);
	setLed_xy(3, 2, pen_red, pen_green, pen_blue);
	setLed_xy(4, 1, pen_red, pen_green, pen_blue);
	setLed_xy(4, 0, pen_red, pen_green, pen_blue);

	setLed_xy(5, 0, pen_red, pen_green, pen_blue);
	setLed_xy(5, 1, pen_red, pen_green, pen_blue);
	setLed_xy(5, 2, pen_red, pen_green, pen_blue);
	setLed_xy(5, 3, pen_red, pen_green, pen_blue);
	setLed_xy(5, 4, pen_red, pen_green, pen_blue);
	setLed_xy(5, 5, pen_red, pen_green, pen_blue);
	setLed_xy(5, 6, pen_red, pen_green, pen_blue);
	setLed_xy(5, 7, pen_red, pen_green, pen_blue);
	setLed_xy(5, 8, pen_red, pen_green, pen_blue);
	setLed_xy(5, 9, pen_red, pen_green, pen_blue);

	setLed_xy(6, 10, pen_red, pen_green, pen_blue);
	setLed_xy(6, 11, pen_red, pen_green, pen_blue);


	setLed_xy(7, 9, pen_red, pen_green, pen_blue);
	setLed_xy(8, 9, pen_red, pen_green, pen_blue);

	setLed_xy(7, 12, pen_red, pen_green, pen_blue);
	setLed_xy(8, 12, pen_red, pen_green, pen_blue);

	setLed_xy(7, 12, pen_red, pen_green, pen_blue);
	setLed_xy(8, 12, pen_red, pen_green, pen_blue);

	setLed_xy(9, 10, pen_red, pen_green, pen_blue);
	setLed_xy(9, 11, pen_red, pen_green, pen_blue);

	setLed_xy(10, 0, pen_red, pen_green, pen_blue);
	setLed_xy(10, 1, pen_red, pen_green, pen_blue);
	setLed_xy(10, 2, pen_red, pen_green, pen_blue);
	setLed_xy(10, 3, pen_red, pen_green, pen_blue);
	setLed_xy(10, 4, pen_red, pen_green, pen_blue);
	setLed_xy(10, 5, pen_red, pen_green, pen_blue);
	setLed_xy(10, 6, pen_red, pen_green, pen_blue);
	setLed_xy(10, 7, pen_red, pen_green, pen_blue);
	setLed_xy(10, 8, pen_red, pen_green, pen_blue);
	setLed_xy(10, 9, pen_red, pen_green, pen_blue);

	//ball two
	setLed_xy(11, 0, pen_red, pen_green, pen_blue);
	setLed_xy(11, 1, pen_red, pen_green, pen_blue);
	setLed_xy(12, 2, pen_red, pen_green, pen_blue);
	setLed_xy(12, 3, pen_red, pen_green, pen_blue);
	setLed_xy(13, 4, pen_red, pen_green, pen_blue);
	setLed_xy(14, 3, pen_red, pen_green, pen_blue);
	setLed_xy(14, 2, pen_red, pen_green, pen_blue);
	setLed_xy(15, 1, pen_red, pen_green, pen_blue);
	setLed_xy(15, 0, pen_red, pen_green, pen_blue);

	while (1)
	{
		setLed_xy(9, 13, ejac_red, ejac_green, ejac_blue);
		print();
		setLed_xy(9, 13, 0, 0, 0);
		_delay_ms(delay);

		setLed_xy(9, 14, ejac_red, ejac_green, ejac_blue);
		print();
		setLed_xy(9, 14, 0, 0, 0);
		_delay_ms(delay);

		setLed_xy(10, 15, ejac_red, ejac_green, ejac_blue);
		print();
		setLed_xy(10, 15, 0, 0, 0);
		_delay_ms(delay);

		setLed_xy(11, 15, ejac_red, ejac_green, ejac_blue);
		print();
		setLed_xy(11, 15, 0, 0, 0);
		_delay_ms(delay);

		setLed_xy(12, 15, ejac_red, ejac_green, ejac_blue);
		print();
		setLed_xy(12, 15, 0, 0, 0);
		_delay_ms(delay);

		print();
		_delay_ms(3 * delay);
	}
}

void loop_uart(){

	//loop input and print it as hex (for easier comparison later)
	while (0)
	{
		char copy;
		uint8_t helper;
		while (!uart_in_read(&copy))
		{
			uart_out_save('0');
			uart_out_save('x');

			helper = copy / 16;
			if (helper > 9)
				helper += 0x40 - 9;
			else
				helper += 0x30;
			uart_out_save(helper);

			helper = copy % 16;
			if (helper > 9)
				helper += 0x40 - 9;
			else
				helper += 0x30;
			uart_out_save(helper);

			uart_out_save(' ');
		}
		uart_send();
		_delay_ms(1000);
	}
	//echo the input
	while(1){
		char copy;
		while (!uart_in_read(&copy))
		{
			//why do we not get newline? only CR????
			if(copy == 0xD)
			{
				uart_out_save(0x0D);
				uart_out_save('\n');
			}else
				uart_out_save(copy);
		}
		uart_send();
		_delay_ms(1000);
	}

}

int main(void)
{
	DDRF |= 0xFF;
	DDRK |= 0xFF;
	DDRB |= (1 << 7);

	init_uart();

#ifdef PC
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("error initializing SDL: %s\n", SDL_GetError());
	}
	win = SDL_CreateWindow("LEDS", 1000, 100, 800, 800,
						   SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

	//keypress interrupt, sadly only on enter :(
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = uart_pc_helper;
	sigaction(SIGIO, &sa, NULL);
	fcntl(0, F_SETFL, O_ASYNC);

#endif


/*	Onboard led
	while(1){
		PORTB |= (1<<PB7);
		_delay_ms(100);
		PORTB &= ~(1<<PB7);
		_delay_ms(100);
	}
*/


	//loop_farben();
	//loop_gol();
	//loop_rand_gol();
	//loop_gol_big();
	//loop_sun();
	//loop_drop();
	//loop_blue();
	//loop_dick();
	loop_uart();
}