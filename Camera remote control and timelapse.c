/*
 Timelapse Remote control
 
 For attiny 85 / 45 / 25
 
 Pin-Connections:
 1 Reset, not connected, maybe pullup high.
 2 PB 3 , pushbutton to GND, internal pullup active (0 = pressed, 1 = open)
 3 PB 2 , pushbutton to GND, internal pullup active
 4 VCC
 5 PB0 , statusled, via 300 Ohm to GND, (active high)
 6 PB1 , 1-Wire bootloader.
 7 PB2 , IR-LED, via 1k to BC547, VCC-IR-CE-30_Ohm-GND
 8 GND
 
 20120710 andyk75 for instructables
 
 */


#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/sleep.h>

/* Uncomment the following statements for your Camera Type
 
 You could also use multiple defines to execute more than one code.
 if you have two cameras, say Fuji and Nikon, define both in the beginning
 and both cameras will be triggered. It takes some milliseconds more time and uses a bit
 more energy from the battery, but hey, what the heck? It works for both!
 
 IMPORTANT:
 The code for Canon, Minolta and Sony has to be checked!
 Because in the control sequence there are statements to control more than the trigger
 of these cameras. But the other button is in my design used for something different and I don't
 have one of these cameras to test it. So please check it for yourself!
 
 */
//#define CANON  // please check code !
//#define PENTAX
#define NIKON
//#define OLYMPUS
//#define SONY	// please check code !
//#define MINOLTA	// please check code !
//#define FUJI

void triggerrelease( void );
void flash( uint8_t counter );
void fastflash( uint8_t counter );
ISR(PCINT0_vect);
ISR(TIMER0_OVF_vect);


#define programmingport 	PB1
#define startbutton			PB4
#define adjusttimelapse		PB3
#define statusled			PB0
#define irleds				PB2


// defines for RC-1 IR Protocoll
#define NR_PULSES 		16
#define DELAY_SHOT		7.33
#define DELAY_DSHOT 	5.36

// Macros for IR LED
#define LED_ON()		PORTB |= (1<<irleds)
#define LED_OFF()		PORTB &= ~(1<<irleds)
#define	PULSE_32k()		LED_ON(); _delay_us(15.24);	LED_OFF(); _delay_us(15.24)
#define PULSE_38k()		LED_ON(); _delay_us(13.16);	LED_OFF(); _delay_us(13.16)
#define PULSE_38_4k()	LED_ON(); _delay_us(13.02);	LED_OFF(); _delay_us(13.02)
#define PULSE_40k()		LED_ON(); _delay_us(12.5);	LED_OFF(); _delay_us(12.5)


uint8_t lapsetime = 0, timeset = 0, i = 0;
volatile uint8_t secondcounter = 0;
uint16_t lapsecounter = 0;

int main(){
    
	DDRB   = (1<<statusled) | (1<<irleds); 					/* PB0/2 as Output for IR LED */
	PORTB |= (1<<startbutton) | (1<<adjusttimelapse) ;		/* Pullup for PB3 and PB4 */
    
	PCMSK  = (1<<PCINT3) | (1<<PCINT4);	// Pin Change Interrupt for Pin3+4
	GIMSK |= (1<<PCIE);					// Enable Pin Change Interrupt
    
	TCCR0B = (1<<CS02) |(1<<CS00);	// Set prescaler for timer 0 to clk/1024
	// TIMSK = (1<<TOIE0); 	// enable Timer Counter0 Overflow will be enabled in interrupt!
    
	flash(2);	// just a welcome to the user. can also be omitted
    
	sei();	// Enable Interrupts
    
    
	/* Power-down Mode until Key pressed */
	while(1){
        
        
		set_sleep_mode(SLEEP_MODE_IDLE);
		sleep_mode();
		if(secondcounter == 10) fastflash(1);	// if in timelapse mode, fast flash every second.
        
	}
    
	return 0;
}


/* Pin Change Interrupt ISR */
ISR(PCINT0_vect)
{
	cli();		// disable interrupts
	_delay_ms(50);		// wait a bit while the button might bounce
	uint8_t ppp = PINB;	// read the current state of the PORTB
    
	if( 0 == (ppp & (1<<startbutton) ) )
	{
		// start/triggerrelease button is pressed, (1 = button is open, internal pullup active! )
		_delay_ms(250);
        
		if(timeset == 0 )
		{
			// if this button is pressed, it means that the timelapse is adjusted and shall not be changed any more
			timeset = 1;	// prevents further changes of the timelapse
			flash(lapsetime);	// give a feedback how many seconds we will wait, 1 flash means 10s and so on...
			lapsetime = lapsetime * 10;	// calculate the real time.
			if(lapsetime > 0 )	TIMSK = (1<<TOIE0); 	// enable Timer Counter0 Overflow only if there is a time lapse set, otherwise it stays disabled.
		}
        
		triggerrelease();		// Trigger the camera release
        
	}
	else if (0 == (ppp & (1<<adjusttimelapse)))
	{
		// the timelapsebutton was pressed
		if(timeset == 0)
		{
			// if the timelapse didn't start yet, increment lapsetime, if it is bigger than 6, set to zero again.
			if(++lapsetime>6)lapsetime=0; 	// this prevents long timelapses of several minutes, as the camera will shut itself down before the next picture would be taken
			flash(1);	// give a feedback that the press was counted.
		}
        
	}
    
	sei();	// reenable interrupts
}





/* Timer Overflow, wake up and counter for timelapse
 * - count the wakeups
 * - trigger the release if the time is right
 * */
ISR(TIMER0_OVF_vect)
{
	secondcounter++;
	// a second means 30 overflows and 132.5 ticks.
	if(secondcounter > 30)
	{
		// now a second has passed
		secondcounter = 0;	// Reset the second-counter
		lapsecounter++;	// count the second in lapse-counter
		if(lapsecounter == lapsetime)		// if the time is right, trigger the camera to take a picture
		{
			triggerrelease();
			lapsecounter = 0;		// reset lapse-counter
		}
	}
}

/*!
 *	\brief function to let the statusled flash as many times as counter indicates
 */
void flash( uint8_t counter )
{
	uint8_t i;
    
	for( i= 0; i< counter; i++ )
	{
		PORTB |= (1<<statusled);
		_delay_ms(250);
		PORTB &= ~(1<<statusled);
		_delay_ms(250);
	}
    
}
/*!
 *	\brief function to let the statusled flash fast as many times as counter indicates
 */
void fastflash( uint8_t counter )
{
	uint8_t i;
    
	for( i= 0; i< counter; i++ )
	{
		PORTB |= (1<<statusled);
		_delay_ms(20);
		PORTB &= ~(1<<statusled);
		//_delay_ms(50);
	}
    
}



/*!
 *	\brief function to trigger the shutter of the camera
 */
void triggerrelease( void )
{
	cli();	// disable further interrupts, because the codes are time critical.
    
#ifdef CANON
	uint8_t i;
	/* Key pressed for undelayed Shot ? */
	{
		for(i = 0; i < NR_PULSES; i++){
			PULSE_32k();
		}
		_delay_ms(DELAY_SHOT);
		for(i = 0; i < NR_PULSES; i++){
			PULSE_32k();
		}
	}
	/* Key pressed for delayed Shot ? */
	else if((PINB & 1<<PB4) == 0){
		for(i = 0; i < NR_PULSES; i++){
			PULSE_32k();
		}
		_delay_ms(DELAY_DSHOT);
		for(i = 0; i < NR_PULSES; i++){
			PULSE_32k();
		}
	}
    
#elif defined PENTAX
	uint16_t i;
	uint8_t	j;
    
	{
		for(i = 0; i < 494; i++){
			PULSE_38k();
		}
		_delay_ms(3);
		for(j = 0; j < 7; j++){
			for(i = 0; i < 38; i++){
				PULSE_38k();
			}
			_delay_ms(1);
		}
        
	}
    
#elif defined NIKON
	uint8_t i;
	uint8_t	j;
    
	{
        for(j = 0; j < 2; j++){
            for(i = 0; i < 77; i++){
				PULSE_38_4k();
			}
			_delay_ms(27.83);
			for(i = 0; i < 15; i++){
				PULSE_38_4k();
			}
			_delay_ms(1.58);
			for(i = 0; i < 16; i++){
				PULSE_38_4k();
			}
			_delay_ms(3.58);
			for(i = 0; i < 16; i++){
				PULSE_38_4k();
			}
			if(!j)
				_delay_ms(63.2);
		}
	}
    
    
#elif defined OLYMPUS
	uint8_t i,j;
	/* Code for Shot */
	uint32_t code=0x61DC807F;
	uint32_t mask=0x80000000;
    
	{
		for(i = 0; i < 152; i++){
			PULSE_40k();
		}
		for(i = 0; i < 22; i++){
			PULSE_40k();
		}
		_delay_ms(4);
		j = 32;
		while(j--){
			if(code & mask){
				_delay_ms(1.5);
				for(i=0; i < 20; i++){
					PULSE_40k();
				}
			}
			else{
				_delay_us(500);
				for(i=0; i < 20; i++){
					PULSE_40k();
				}
			}
			mask >>= 1;
		}
	}
    
    
#elif defined SONY
	uint8_t i,j=0,k;
	uint8_t cmd = 0;
	uint16_t mask = 1;
	uint16_t address = 0x1E3A;
    
    
	if((PINB & 1<<PB3) == 0)
		cmd = 0x2D;
	else if((PINB & 1<<PB4) == 0)
		cmd = 0x37;
    
	for(k = 0; k<5; k++){
		mask = 1;
		/* Send Start-Burst */
		for(i = 0; i < 96; i++){
			PULSE_40k();
		}
		_delay_us(640);
		j=0;
		/* Send CMD; LSB first */
		while(j++ <7){
			if(cmd & mask){
				for(i = 0; i < 48; i++){
					PULSE_40k();
				}
			}
			else{
				for(i = 0; i < 24; i++){
					PULSE_40k();
				}
				
			}
			_delay_us(640);
			mask <<= 1;
		}
		j=0;
		mask = 1;
		/* Send Adress; LSB first */
		while(j++<13){
			if(address & mask){
				for(i = 0; i < 48; i++){
					PULSE_40k();
				}
			}
			else{
				for(i = 0; i < 24; i++){
					PULSE_40k();
				}
                
			}
			_delay_us(640);
			mask <<= 1;
		}
		_delay_ms(11);
	}
	
#elif defined MINOLTA
	uint8_t i,j,k;
	uint16_t mask;
	uint16_t address = 0xCA34;
	uint16_t cmd = 0;
    
	if((PINB & 1<<PB3) == 0)
		cmd = 0x141;
	else if((PINB & 1<<PB4) == 0)
		cmd = 0x41;
    
	for(k=0;k<10;k++){
		/* Send Start-Burst */
		for(i = 0; i < 144; i++){
			PULSE_38k();
		}
		_delay_ms(1.89);
        
		/* Send address */
		mask = 1;
		j=0;
		while(j < 16){
			for(i = 0; i < 18; i++){
				PULSE_38k();
			}
			if(address & mask)
				_delay_us(1430);
			else
                _delay_us(487);
            
			mask <<= 1;
			j++;
		}
        
		/* Send command */
		mask = 1;
		j=0;
		while(j < 16){
			for(i = 0; i < 18; i++){
				PULSE_38k();
			}
			if(cmd & mask)
				_delay_us(1430);
            
			else
				_delay_us(487);
			
			mask <<= 1;
			j++;
		}
        
		/* Stop-Pulse */
		for(i = 0; i < 18; i++){
			PULSE_38k();
		}
		_delay_ms(9.2);
	}
    
#elif defined FUJI
    /* NEC Protocoll
     Starburst + 16 Bit-Address + 8-Bit-CMD + not( 8-Bit-CMD) + Stopburst
     
     */
	uint8_t i;
	uint8_t j,k;
	uint16_t mask;
	uint16_t address=0x30CB;
	uint16_t cmd = 0;
	
	cmd = 0x7E81;
	
	/* Send Start-Sequence */
	for(i = 0; i < 171; i++){
		PULSE_38k();
	}
	/* again to get 9ms */
	for(i = 0; i < 171; i++){
		PULSE_38k();
	}
	_delay_ms(4.5);
    
	/* Send address */
	mask = 1;
	for(j=0; j < 16; j++){
		for(i = 0; i < 22; i++){
			PULSE_38k();
		}
		if(address & mask)
			_delay_us(1650);
		else
			_delay_us(560);
        
		mask <<= 1;
	}
	/* Send command */
	mask = 1;
	for(j=0; j < 16; j++){
		for(i = 0; i < 22; i++){
			PULSE_38k();
		}
		if(cmd & mask)
			_delay_us(1650);
		else
			_delay_us(560);
        
		mask <<= 1;
	}
	/* Stop-"Bit" */
	for(i = 0; i < 22; i++){
        PULSE_38k();
	}
	_delay_ms(41);
	
	/* Stop-Sequence */
	for(i = 0; i < 171; i++){
		PULSE_38k();
	}
	/* again to get 9ms */
	for(i = 0; i < 171; i++){
		PULSE_38k();
	}
	_delay_ms(2.2);
	/* and a short one */
	/* Stop-"Bit" */
	for(i = 0; i < 22; i++){
        PULSE_38k();
	}
    
#endif
    
	sei();	// reenable interrupts
}
