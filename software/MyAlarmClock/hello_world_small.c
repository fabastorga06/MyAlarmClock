#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "sys/alt_stdio.h"
#include "sys/alt_irq.h"
#include "system.h"
#include "alt_types.h"
#include "altera_avalon_timer_regs.h"
#include "altera_avalon_pio_regs.h"
#include "altera_up_avalon_rs232.h"

#define MIN_UNIT 60
#define MIN_DEC 600
#define HOUR_UNIT 3600
#define HOUR_DEC 36000
#define BLINK_7SEG 0xFF
#define READ_FIFO_EMPTY 0x0
#define MSG_DATA 5

volatile int edge_capture_button;
volatile int edge_capture_time;
volatile int edge_capture_uart;

volatile char *timer_status_ptr = (char *)( TIMER_BASE);
volatile char *timer_control_ptr = (char *)(TIMER_BASE + 0x4);
volatile char *timer_mask_ptr = (char *)(TIMER_BASE + 0x8);
volatile char *timer_edge_cap_ptr = (char *)(TIMER_BASE + 0xC);

volatile char *btn_direction_ptr = (volatile char *)(BUTTONS_BASE + 0x4);
volatile char *btn_mask_ptr = (volatile char *)(BUTTONS_BASE + 0x8);
volatile char *btn_edge_ptr = (volatile char *)(BUTTONS_BASE + 0xC);

volatile char *uart_rxdata_ptr = (volatile char *)(UART_BASE);
volatile char *uart_status_ptr = (volatile char *)(UART_BASE + 0x8);
volatile char *uart_control_ptr = (volatile char *)(UART_BASE + 0xC);

volatile char *buttons = (char *) BUTTONS_BASE;
volatile char *leds = (char *) LEDS_BASE;

volatile char *hex0 = (char *) SEG4_BASE;
volatile char *hex1 = (char *) SEG3_BASE;
volatile char *hex2 = (char *) SEG2_BASE;
volatile char *hex3 = (char *) SEG1_BASE;
volatile char *hex4 = (char *) SEG5_BASE;
volatile char *hex5 = (char *) SEG6_BASE;

time_t actual_time = 0;
time_t new_time;
time_t alarm = 0;

struct tm timestamp;
struct tm alarm_timestamp;

int decs, units;
char button_data = 15;
short status = 0;
short pos_ptr = 0;

char rcv_data[MSG_DATA];
unsigned char data_pos = 0;

char convert_to_7seg(int num)
{
	unsigned int result = 0;
	if (num == 0) { result = 0x7F - 0x40; }
	else if (num == 1) { result = 0x7F - 0x79; }
	else if (num == 2) { result = 0x7F - 0x24; }
	else if (num == 3) { result = 0x7F - 0x30; }
	else if (num == 4) { result = 0x7F - 0x19; }
	else if (num == 5) { result = 0x7F - 0x12; }
	else if (num == 6) { result = 0x7F - 0x02; }
	else if (num == 7) { result = 0x7F - 0x78; }
	else if (num == 8) { result = 0x7F - 0x00; }
	else if (num == 9) { result = 0x7F - 0x10; }

	return ~result;
}

int get_time_diff()
{
	switch (pos_ptr){
		case 0:
			return MIN_UNIT;
			break;
		case 1:
			return MIN_DEC;
			break;
		case 2:
			return HOUR_UNIT;
			break;
		case 3:
			return HOUR_DEC;
			break;
		default:
			return 0;
			break;
		}
}

void split_time(const int digit, int* dec, int* unit)
{
	*dec = convert_to_7seg(digit / 10);
	*unit = convert_to_7seg(digit % 10);
}

void show_time(int hours, int minutes, int seconds)
{
	split_time(hours, &decs, &units);
	*hex5 = decs;
	*hex4 = units;
	split_time(minutes, &decs, &units);
	*hex3 = decs;
	*hex2 = units;
	split_time(seconds, &decs, &units);
	*hex1 = decs;
	*hex0 = units;
}

int concatenate_nums(int a, int b)
{
	return (a * 10) + b;
}

/********************************************************************/
static void timer_handler(void * context)
{
	*timer_status_ptr = 0;
	actual_time++;

	/* Check actual time and check alarm */
	if (status == 0){
		timestamp = *localtime(&actual_time);
		alarm_timestamp = *localtime(&alarm);
		show_time(timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);
		if (timestamp.tm_sec % 2 == 0)
		{
			if (timestamp.tm_hour == alarm_timestamp.tm_hour && timestamp.tm_min == alarm_timestamp.tm_min){
				*leds = 0xAA;
			}
			else{
				*leds = 0;
			}
		}
		else { *leds = 0; }
	}

	/* New time */
	else if (status == 1)
	{
		timestamp = *localtime(&new_time);
		new_time++;
	}

	/* Set alarm in 7seg */
	else if (status == 2)
	{
		timestamp = *localtime(&alarm);
	}

	alarm_timestamp = *localtime(&actual_time);
	if(status == 0 || (status ==1  && timestamp.tm_sec % 2 == 0) ||
		(status == 2 && alarm_timestamp.tm_sec % 2 == 0))
	{
		show_time(timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);
	}

	else {
		split_time(timestamp.tm_sec, &decs, &units);
		*hex1 = decs;
		*hex0 = units;

		switch (pos_ptr){
		case 0:
			*hex2 = BLINK_7SEG;
			break;
		case 1:
			*hex3 = BLINK_7SEG;
			break;
		case 2:
			*hex4 = BLINK_7SEG;
			break;
		case 3:
			*hex5 = BLINK_7SEG;
			break;
		default:
			break;
		}
	}
}

static void buttons_handler(void * context)
{
	volatile int * edge_capture_ptr = (volatile int*) context;
	*edge_capture_ptr = *(volatile int *)(BUTTONS_BASE + 0xC);
	*btn_mask_ptr = 0xF;
	*btn_edge_ptr = *edge_capture_ptr;

	switch (*edge_capture_ptr) {

		/* Change mode */
		case 8:
			if(button_data != *edge_capture_ptr)
			{
				if (status == 0)  				 	/* Show time */
				{
					status = 1;
					new_time = actual_time;
					*leds = 2;
				}
				else if (status == 1)  				/* Set time */
				{
					status = 2;
					actual_time = new_time;
					*leds = 4;
				}
				else if (status == 2)  				/* Set alarm */
				{
					status = 0;
					*leds = 1;
				}
			}
			break;

		/* Change pointer position in 7segs */
		case 4:

			if(button_data != *edge_capture_ptr)
			{
				pos_ptr++;
				if (pos_ptr > 3) { pos_ptr = 0; }
			}
			break;

		/* Increase digit in 7seg  */
		case 2:
			if(button_data != *edge_capture_ptr){
				if (status == 1)
				{
					new_time += get_time_diff();
				}
				else if (status == 2)
				{
					alarm += get_time_diff();
				}
			}
			break;

		/* Decrease digit in 7seg  */
		case 1:
			alt_putstr("DOWN");
			if(button_data != *edge_capture_ptr) {
				if (status == 1)
				{
					if(new_time != 0) (new_time -= get_time_diff());
				}
				else if (status == 2)
				{
					if(alarm != 0) (alarm -= get_time_diff());
				}
			}
			break;

		default:
			break;
	}
}

void print_data() {
	for (int i = 0; i < MSG_DATA; ++i) {
		printf("%c", rcv_data[i]);
	}
	printf("\n");
}

void uart_handler(void* context)
{
	unsigned int flag = 0;

	if (*uart_rxdata_ptr == 's') {
		flag = 1;
	}
	else if (flag) {
		if (*uart_rxdata_ptr == 'f') {
			flag = 0;
		}
		else {
			rcv_data[data_pos] = *uart_rxdata_ptr;
			data_pos++;
		}
	}
	else {
		data_pos = 0;
		int res_h = concatenate_nums(rcv_data[1] - '0', rcv_data[2] - '0');
		int res_m = concatenate_nums(rcv_data[3] - '0', rcv_data[4] - '0');

		if (rcv_data[0] == 't')
		{
			actual_time = res_h * 3600 + res_m * 60;
			new_time = actual_time;
		}
		else if (rcv_data[0] == 'a')
		{
			alarm = res_h * 3600 + res_m * 60;
		}
	}
}

/********************************************************************/

static void init_buttons(void)
{
	void * edge_capture_ptr = (void*) &edge_capture_button;
	*btn_mask_ptr = 0xF;
	*btn_edge_ptr = 0xF;
	*btn_direction_ptr = 0;

	alt_ic_isr_register( BUTTONS_IRQ_INTERRUPT_CONTROLLER_ID,
						BUTTONS_IRQ,
						buttons_handler,
						edge_capture_ptr, 0);
}

static void init_timer()
{
	void * edge_capture_ptr = (void*) &edge_capture_time;
	*timer_mask_ptr = 1;
	*timer_edge_cap_ptr = 0xF;
	*timer_control_ptr = 7;
	*timer_status_ptr = 0;

	alt_ic_isr_register (TIMER_IRQ_INTERRUPT_CONTROLLER_ID,
						TIMER_IRQ,
						timer_handler,
						edge_capture_ptr,
						0);
}

static void init_uart_port()
{
	void * edge_capture_ptr = (void*) &edge_capture_uart;
	*uart_control_ptr = 0x80;

	alt_ic_isr_register( UART_IRQ_INTERRUPT_CONTROLLER_ID,
						 UART_IRQ,
						 uart_handler,
						 edge_capture_ptr,
						 0);
}

/********************************************************************/

int main()
{
	init_timer();
	init_buttons();
	init_uart_port();

	/* Event loop never exits. */
	while (1);

	return 0;
}
