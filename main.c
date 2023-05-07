/* File:	main.c
 * Author:	Integrated Bioengineering, LLC.
 * Processor: SAMD21G18A @ 48MHz, 3.3v
 * Program: Main Application File
 * Compiler: ARM-GCC Microchip Studio
 * Program Version 1.0
 * Program Description: Pump Control Application
 * Hardware Description: Pressure transmitter for pressure monitoring, Pump Relay for pressure control, Low Level Switch for run-dry protection
 * 
 * Change History:
 * Author                Rev     Date          Description
 * Brad				     1.0     02/12/2022    ADC Setup
 * Brad				     1.1     01/25/2023    Low Level Alarm
 
Copyright (c) 2023 Integrated Bioengineering, LLC. and its subsidiaries.
 */

//////////////////////////////////////////////////////////////////////////
// INCLUDES AND DEFINITIONS
//////////////////////////////////////////////////////////////////////////

#include <atmel_start.h>

#define ADC_11_channel					11				// A7 Labeled on Board
#define ADC_11_raw_buffer_size			2				// Bytes
#define ADC_11_ave_buffer_size			5				// ADC values to average. Note this is an average of an average( ie. 16 averaged samples from the ADC x 5 = 80 total sample size)
#define ADC_11_voltage_reference		2.229729729730	// VDC INTERNAL
#define ADC_11_fsr		65520 							// FSR, HIGHEST VALUE THE ADC WILL READOUT. 4 BITS ARE DISCARDED AND USED INSTEAD AS MOST SIGNIFICANT BITS (MSB)
#define ADC_11_max		64638							// CALCULATED 2.20/2.23 * FSR VDC
#define ADC_11_min		12927	// CALCULATED 0.44/2.23 * VDC

/* TRAFAG INDUSTRIAL PRESSURE TRANSMITTER
TYPE:			8287.G8.2530
S/N:			832054-003 | 08/21
RANGE:			0....150 psi-G | max. 450 psi
POWER:			+9 - 32 VDC
OUTPUT:			4-20 ma converted to voltage with a shunt 110 Ohm 1/4 Watt resistor [YR1B110RCC] to ground
ADC: Values at 0 = 15,500
ADC: Values at 150 = 65000.

PRESSURE MAPPING DEFINITIONS	*/
float map(float x, float in_min, float in_max, float out_min, float out_max) { return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; }
#define voltage_at_min				0.54				// V@ 0PSI | 0.44 VDC at  4ma Calculated
#define voltage_at_max				ADC_11_voltage_reference				// 2.20 VDC at 20ma Calculated
#define min_pressure				0					// PSI at  4 ma
#define max_pressure				150					// PSI at 20 ma

//TIMER DEFINITIONS
#define	min_on_time_interval		2					// RUNTIME PARAMETER (Seconds)
#define	min_off_time_interval		2					// RUNTIME PARAMETER (Seconds)

/* PENTAIR SHURFLOW
TYPE:			SHURFLO
MODEL #:		8030-863-239
MAX FLOW:		1.5 GPM
MAX PRESSURE:	150 PSI
POWER:			120VAC, 60HZ, 1.2A, 138W
*/

//////////////////////////////////////////////////////////////////////////
//GLOBAL VARIABLES
//////////////////////////////////////////////////////////////////////////
//ADC 11
uint16_t	ADC_11_value		=	0;					// 16 BIT UNSIGNED INTEGER FOR SINGLE ENDED (ALWAYS POSITVE)
int values;												// INT ADC VALUES
int size;												// AVERAGE ADC SAMPLE SIZE
float		ADC_11_volts		=	0;					// ADC READOUT IN VOLTS
float		ADC_11_pressure		=	0;					// ADC PRESSURE IN PRESSURE WITH FLOAT PRECISION
int			ADC_11_pressure_int =	0;					// ADC PRESSURE READOUT AS INT

//PUMP
uint8_t		pump_set_pressure	=	100;				// INITIAL PRESSURE SETPOINT
uint8_t		deadband			=	5;					// PRESSURE CONTROL DREADBAND (Seconds)

//EEPROM MEMORY
uint16_t	pump_total_runtime	=	0;					// ACCUMULATED PUMP RUNTIME (Hours)
uint32_t	pump_starts_count	=	0;					// COUNTER VARIABLE FOR NUM OF PUMP STARTS(#)

//STATE VARIABLES
volatile bool pump_state		=	0;					// PUMP STATE INITIALIZED AS OFF WHEN STARTING UP
volatile bool low_level_flag		=	0;					// DEFAULT IS FAlSE (0),TRUE WHEN ACTIVE
volatile bool min_on_time_flag =	0;					// MIN ON TIMER IN SECONDS. DEFAULT IS FAlSE (0),TRUE WHEN ACTIVE
volatile bool min_off_time_flag =	0;					// MIN OFF TIMER IN SECONDS. DEFAULT IS FAlSE (0),TRUE WHEN ACTIVE
volatile bool max_on_time_flag = 	0;					// MIN RUNTIME TIMER IN SECONDS. DEFAULT IS FAlSE (0),TRUE WHEN ACTIVE

//TIMER VARIABLES
static struct timer_task min_on_time, min_off_time;

//ENABLE TC3 CALLBACK FUNCTION 1
static void min_on_time_expire(const struct timer_task *const timer_task)
{
	min_on_time_flag = false;							// FLAG IS OFF OR FALSE
}

//ENABLE TC3 CALLBACK FUNCTION 2
static void min_off_time_expire(const struct timer_task *const timer_task)
{
	min_off_time_flag = false;							// FLAG IS OFF OR FALSE
}
	///////////////////////////			SETUP		////////////////////////////////
int main(void){
	atmel_start_init();
	adc_sync_enable_channel(&ADC_0, ADC_11_channel);


	//Configure Minimum Runtime Timer
	min_on_time.interval = min_on_time_interval;
	min_on_time.cb = min_on_time_expire;
	min_on_time.mode = TIMER_TASK_ONE_SHOT;
	
	min_off_time.interval = min_off_time_interval;
	min_off_time.cb = min_off_time_expire;
	min_off_time.mode = TIMER_TASK_ONE_SHOT;
	
	char *setup_greeting = "\nSETUP COMPLETE!";
	printf("\n%s", setup_greeting); //GREETING
	//LED_0_off();
	
	///////////////////////////		END OF SETUP	////////////////////////////////

	while(true) {	//MAIN APPLICATION LOOP		
		// MONITOR SYSTEM PARAMETERS
		printf("\n");
		//SYSTEMS CHECK
		check_low_level_protection();									//Ensure tank is not at a low level
		check_min_on_time_protection();
		check_min_off_time_protection();								
		check_pressure_reading();										//Read Pressure
		check_pump_status();
		
		// ***************      PRESSURE CONTROL        ********************
		
		// PRESSURE REQUIRED 
		if(ADC_11_pressure < (pump_set_pressure - deadband)){				//BELOW SET PRESSURE
			if (!pump_state && !min_off_time_flag && !low_level_flag)	//IF PUMP IS NOT ON AND THERE IS NO MINIMUM OFF FLAG
			{
				//IF MIN OFFTIME HAS NOT EXPIRED
				PUMP_on();												//Pump is cycled ON
				pump_state = true;										//Pump ON state
				min_on_time_flag = true;								// START MINIMUM RUNTIME FLAG
				timer_add_task(&TIMER_0, &min_on_time);					// ADDING A TASK TO TC3
				timer_start(&TIMER_0);									// STARTING MINIMUM RUN TIMER			
			}
		}

		// PRESSURE NOT REQUIRED - TURN OFF
		if(ADC_11_pressure >= (pump_set_pressure+deadband) ){			// If above set pressure
			if (pump_state && !min_on_time_flag){						// IF MINIMUM ON TIME HASE NOT EXPIRED, PUMP STOP IS LIMITED.
				PUMP_off();												// Pump is cycled OFF
				pump_state = false;										// Pump OFF state
				min_off_time_flag = true;								// START MINIMUM RUNTIME FLAG
				timer_add_task(&TIMER_0, &min_off_time);				// ADDING A TASK TO TC3
				timer_start(&TIMER_0);									// STARTING MINIMUM RUN TIMER
			}	
		}

	} //end of while for application
}

void check_pressure_reading(void){
		/* ///////////////////////////////////////////////////////////////
		 * ADC READ CONFIGURATION SUMMARY
		 * ADC CHANNEL:			11 (+) PB03/A7 
		 * ADC MODE:			SINGLE ENDED VS. INTERNAL GROUND GROUND	
		 * ADC CLOCK SOURCE:	GENERIC CLOCK 2 >> [OSC32K] HIGH-ACCURACY CLOCKED AT >> 32,768 HZ / 4 = [8,192 HZ]
		 * RESOLUTION:			16 BIT
		 * NUMBER OF SAMPLES:	16
		 * REFERENCE:			1 / 1.48 VDDANA reference [2.23 VDC]
		 *////////////////////////////////////////////////////////////////
		LED_0_on();																//LED ON during ADC reading
		uint8_t ADC_11_raw_buffer[ADC_11_raw_buffer_size];						//Initializing an ADC buffer for raw outputs
		int ADC_11_ave_buffer[ADC_11_ave_buffer_size];							//Initializing an averaging buffer for the raw values	

		for (int i = 0; i < ADC_11_ave_buffer_size; i++){						//For Loop to fill up and average values buffer
			adc_sync_read_channel(&ADC_0, ADC_11_channel, ADC_11_raw_buffer, ADC_11_raw_buffer_size);
			ADC_11_value = ADC_11_raw_buffer[1];								//Assigning upper 8 bits of ADC value from the raw buffer
			ADC_11_value = ADC_11_value << 8;									//Bit shifting the 16-bit ADC value to the lower 8 bits
			ADC_11_value = ADC_11_value | ADC_11_raw_buffer[0];					//Assigning lower 8 bits of ADC value from the raw buffer	
			ADC_11_ave_buffer[i] = ADC_11_value;								//Assigning the full 16-bit value to buffer
			delay_ms(1);														//General delay
		}

		//CALCULATING AVERAGE ADC CHANNEL 11 VALUE
		ADC_11_value = average(ADC_11_ave_buffer, ADC_11_ave_buffer_size);
		printf("\tADC 11:\t%i",ADC_11_value);
		
		ADC_11_volts = ((float)ADC_11_value / (float)ADC_11_fsr) * (float)ADC_11_voltage_reference;		//CALCULATING VOLTAGE READING FOR THE ADC 11
		ADC_11_pressure = map(ADC_11_volts,voltage_at_min,voltage_at_max,min_pressure,max_pressure);	//CALCULATING PRESSURE READING FOR THE ADC 11
		ADC_11_pressure_int = ADC_11_pressure;															//Truncating float precision by assigning as int value
		
		if (ADC_11_pressure_int <=   0){ ADC_11_pressure_int =  min_pressure;}	//Min pressure limit
		if (ADC_11_pressure_int >= 150){ ADC_11_pressure_int =  max_pressure;}	//Max pressure limit
				
		char ADC_11_str_buffer[3] = {0};										//String buffer to print floats to 
		gcvt(ADC_11_volts,3,ADC_11_str_buffer);									//Convert float to string that is 3 characters
		printf("\tVolts:\t%.4s", ADC_11_str_buffer);							//Prints 3 characters of sting buffer
		printf("\tPressure:\t%.3i psi", ADC_11_pressure_int);					//Convert float to string that is 3 characters
		
		LED_0_off();															//LED OFF after any ADC reading
	}	//END check_pressure_reading

	//ADC AVERAGING FUNCTION
	float average(int* values, int size){
		int sum = 0;
		for (int i = 0; i < size; i++){
			sum += values[i];
		}
		return (float)sum / size;
	}
	
	//LOW LEVEL PROTECTION
	check_low_level_protection(){
	low_level_flag = gpio_get_pin_level(LOW_LVL_PIN);						//Read the low level pin for True = None Zero / False status
	printf("\tLow:%d", low_level_flag);							//Prints 3 characters of sting buffer
	};
	
	//MIN ON TIME FUNCTION
	check_min_on_time_protection(){
	printf("\tMin ON:%d",min_on_time_flag);							//Prints 3 characters of sting buffer
	};
	
	//MIN OFF TIME FUNCTION
	check_min_off_time_protection(){
	printf("\tMin OFF:%d",min_off_time_flag);							//Prints 3 characters of sting buffer
	};
	
	check_pump_status(){
	if(pump_state)
	printf("\t+");
	};