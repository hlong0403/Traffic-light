#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <system_stm32f10x.h>
#include <stm32f10x.h>
#include "systick_time.h"
#include "lcd_1602_drive.h"

// define states
#define Pgo 0
#define Wgo 1
#define Sgo 2
#define Wwait 3
#define Swait 4
#define Warn1 5
#define Off1 6
#define Warn2 7
#define Off2 8
#define Warn3 9
#define AllStop 10

//define colors
#define GREEN 1
#define YELLOW 2
#define WARN 3
#define ALLRED 4

const uint32_t greenDelay = 10000;
const uint32_t yellowDelay = 5000;
const uint32_t warnDelay = 1000;
const uint32_t redDelay = 180403;

static void TimerDelayMs(uint32_t time);
static void sendRemaningTime(uint8_t color, uint32_t time);
static void TIM_Init(void);
static void GPIO_Init(void);
static void PLLInit(void);
static void Interupt_Config(void);


struct State {
	uint32_t out;
	unsigned long wait;
	uint32_t next[8]; //Index of the state
};
typedef const struct State Stype;

Stype fsm[11] = {
		// Pgo
		{0x127, greenDelay, {Warn1,	Warn1,	Warn1,	Warn1,	Pgo,	Warn1,	Warn1,	Warn1}},
		// Wgo
		{0x61, greenDelay, {Wwait,	Wgo,	Wwait,	Wwait,	Wwait,	Wwait,	Wwait,	Wwait}},
		// Sgo
		{0x109, greenDelay, {Swait,	Swait,	Sgo,	Swait,	Swait,	Swait,	Swait,	Swait}},
		// Wwait
		{0xA1, yellowDelay, {AllStop,	Wgo,	Sgo,	Sgo,	Pgo,	Pgo,	Pgo,	Pgo}},
		// Swait
		{0x111, yellowDelay, {AllStop,	Wgo,	Sgo,	Wgo,	Pgo,	Wgo,	Pgo,	Wgo}},
		// Warn1
		{0x127, warnDelay, {Off1,	Off1,	Off1,	Off1,	Off1,	Off1,	Off1,	Off1}},
		// Off1
		{0x121, warnDelay, {Warn2,	Warn2,	Warn2,	Warn2,	Warn2,	Warn2,	Warn2,	Warn2}},
		// Warn2
		{0x120, warnDelay, {Off2,	Off2,	Off2,	Off2,	Off2,	Off2,	Off2,	Off2}},
		// Off2
		{0x121, warnDelay, {Warn3,	Warn3,	Warn3,	Warn3,	Warn3,	Warn3,	Warn3,	Warn3}},
		// Warn3
		{0x120, warnDelay, {AllStop,	Wgo,	Sgo,	Sgo,	Pgo,	Wgo,	Sgo,	Sgo}},
		// AllStop
		{0x121, redDelay, {AllStop,	Wgo,	Sgo,	Sgo,	Pgo,	Wgo,	Sgo,	Sgo}}
};

uint16_t S;
uint32_t Input = 0xFF;
bool checkWalk = 0;
bool checkSouth = 0;
bool checkWest = 0;
bool checkGPIO = 0;
uint8_t inputValue = 0;
uint32_t greenEnd = 0;
uint32_t yellowEnd = 0;
uint32_t warnEnd = 0;
uint32_t greenEnds = 0;
uint32_t yellowEnds = 0;
uint32_t warnEnds = 0;
uint32_t count1 = 0;
uint32_t count2 = 0;
uint16_t greenCNT = 0;
uint16_t yellowCNT = 0;
uint16_t warnCNT = 0;
uint32_t warnTime = 0;
char lcdCNT[50];


int main(void) {
	PLLInit();
	GPIO_Init();
	TIM_Init();
	Interupt_Config();
	systick_init(); //necessarry function for I2C LCD
	lcd_i2c_init(1);
	S = AllStop;
  while(1) {
	  // Clear counter of timers
	  TIM2->CNT = 0;
	  TIM3->CNT = 0;
	  TIM4->CNT = 0;
	  // set output
	  GPIOA->ODR = (fsm[S].out)|((fsm[S].out & 0x100)<<1);
	  // delay
	  TimerDelayMs(fsm[S].wait);
	  //read input
	  Input = inputValue;
	  //S = next state
	  S = fsm[S].next[Input];
  }
}

static void sendRemaningTime(uint8_t color, uint32_t time) {
	count1 = time;
	greenCNT = TIM2->CNT;
	yellowCNT = TIM3->CNT;
	warnCNT = TIM4->CNT;
	time /= 1000;
	count2 = time;

	switch(color)
	{
	case 1: // green
		lcd_blank_12(1);
		sprintf(lcdCNT,"%02u", time+greenEnd*10);
	  lcd_i2c_msg(1, 1, 14, lcdCNT);
		break;
	case 2: // yellow
		lcd_blank_12(1);
		sprintf(lcdCNT,"%02u", time);
		lcd_i2c_msg(1, 1, 7, lcdCNT);
		break;
	case 3: // warn
		lcd_blank_12(1);
		if(warnEnds == 1) {
			sprintf(lcdCNT,"%02u", warnTime);
			lcd_i2c_msg(1, 1, 7, lcdCNT);
			warnTime = 1;
			break;
		} else if (warnEnds == 2) {
			sprintf(lcdCNT,"%02u", warnTime);
			lcd_i2c_msg(1, 1, 7, lcdCNT);
			warnTime = 2;
			break;
		} else if (warnEnds == 3) {
			sprintf(lcdCNT,"%02u", warnTime);
			lcd_i2c_msg(1, 1, 7, lcdCNT);
			warnTime = 3;
			break;
		} else if (warnEnds == 4) {
			sprintf(lcdCNT,"%02u", warnTime);
			lcd_i2c_msg(1, 1, 7, lcdCNT);
			warnTime = 4;
			break;
		} else if (warnEnds == 5) {
			sprintf(lcdCNT,"%02u", warnTime);
			 lcd_i2c_msg(1, 1, 7, lcdCNT);
			warnTime = 0;
			break;
		} else {
			sprintf(lcdCNT,"%02u", warnTime);
			 lcd_i2c_msg(1, 1, 7, "00");
			warnTime = 0;
			break;
		}
		break;
	case 4: // allRed
		 lcd_blank_12(1);
	   lcd_i2c_msg(1, 1, 0, "00");
			break;
	}
}


static void TimerDelayMs(uint32_t time) {
	bool jumpToMain = 0;
	switch (time)
	{
	case 10000: // green
		warnEnds = 0;
		if(greenEnd == 0) {
			TIM2->CR1 |= TIM_CR1_CEN;
			if(greenEnds == 0) {
				greenEnds ++;
				greenEnd = 0;
			}
			while(1) {
				sendRemaningTime(GREEN, TIM2->CNT);
				if(greenEnd == 1) {
					greenCNT = TIM2->CNT;
					jumpToMain = 1;
					TIM2->CR1 &= ~TIM_CR1_CEN;
					break;
				}
			}
		}
		if((greenEnd >= 1 ) && (jumpToMain == 0)) {
//			count2++;
			TIM2->CR1 |= TIM_CR1_CEN;;
			uint32_t nextGreenEnd = greenEnd + 1;
			uint8_t inputCompare = inputValue;
			while(1) {
				sendRemaningTime(GREEN, TIM2->CNT);
				if((greenEnd == nextGreenEnd) || (inputCompare != inputValue)) {
					TIM2->CR1 &= ~TIM_CR1_CEN;;
					break;
				}
			}
		}
		break;
	case 180403: // all red
		greenEnds = 0;
		yellowEnds = 0;
		warnEnds = 0;
		greenEnd = 0;
		yellowEnd = 0;
		warnEnd = 0;
		while(1) {
			sendRemaningTime(ALLRED, 180403);
			if(checkGPIO == 1) {
				break;
			}
		}
	break;
	case 5000: // yellow
		yellowEnd = 0;
		greenEnd = 0;
		greenEnds = 0;
		warnEnd = 0;
		warnEnds = 0;
		TIM3->CR1 |= TIM_CR1_CEN;;
		yellowEnd = 0;
		while(1) {
			sendRemaningTime(YELLOW, TIM3->CNT);
			if(yellowEnd == 1) {
				TIM3->CR1 &= TIM_CR1_CEN;
				break;
			}
		}
	case 1000: // warn
		warnEnd = 0;
		greenEnd = 0;
		greenEnds = 0;
		TIM4->CR1 |= TIM_CR1_CEN;
		while(1) {
			sendRemaningTime(WARN, TIM4->CNT);
			warnCNT = TIM4->CNT;
			if(warnEnd == 1) {
				TIM4->CR1 &= TIM_CR1_CEN;;
				break;
			}
		}
		break;
	default:
		break;
	}
}

static void TIM_Init(void) {
	//Enable timer 2,3,4 clock
	RCC->APB1ENR |= 0x07; 
	TIM2->PSC = 16000-1;
	TIM2->ARR = 10000-100;
	TIM3->PSC = 16000-1;
	TIM3->ARR = 5000-100;
	TIM4->PSC = 16000-1;
	TIM4->ARR = 1000-100;
	
	//Send an update event to reset the timer and apply settings.
	TIM2->EGR |= TIM_EGR_UG;
	TIM3->EGR |= TIM_EGR_UG;
	TIM4->EGR |= TIM_EGR_UG;
}

static void PLLInit(void) {
	
	//Flash Latency setup
	FLASH->ACR	|= FLASH_ACR_LATENCY_0; // because we are running with 16MHz
	RCC->CFGR	  |= RCC_CFGR_PPRE1_DIV1; // APB1 division is 1
	RCC->CFGR	  |= RCC_CFGR_PPRE2_DIV1; // APB2 division is 1
	RCC->CFGR 	|= RCC_CFGR_PLLXTPRE_HSE; // setting the division factor of the HSE clock to 0
	
	//Turn on HSE
	RCC->CR 	|= RCC_CR_HSEON;
	while (!(RCC->CR & RCC_CR_HSERDY)); // wait for hse ready
	RCC->CFGR 	|= RCC_CFGR_PLLSRC_HSE; //Setting this bit configures HSE as the source of the PLL.
	RCC->CFGR   |= RCC_CFGR_PLLMULL2; //Setting these bits to PLLMULL2 multiplies the HSE clock by a factor of 2
	
	//Turn on PLL
	RCC->CR     |= RCC_CR_PLLON;
	while (!(RCC->CR & RCC_CR_PLLRDY)); // wait for PLL ready
	
	//(System clock switch) set clock source to pll
	RCC->CFGR 	|= RCC_CFGR_SW_PLL; 	
	
	while (!(RCC->CFGR & RCC_CFGR_SWS_PLL))
		; // wait for PLL to be CLK
	SystemCoreClockUpdate();
}


static void GPIO_Init(void) {
	RCC->APB2ENR = 0x0D; //Enable port A + port B 's clock and AFIO function
	GPIOA->CRL = 0x33333333;
	GPIOA->CRH = 0x33;
	GPIOB->CRL = 0x888000;
	GPIOB->ODR &= ~0x38;
}

static void Interupt_Config(void) {
	//GPIO Interrupts Config
	RCC->APB2ENR |= (1<<0);  // Enable AFIO CLOCK
	AFIO->EXTICR[0] &= (0x01<<12);  // Setup GPIO interrupt for PB3,4,5
	AFIO->EXTICR[1] &= (0x11<<0);
	EXTI->IMR |= (0x07<<3);  // Bit[1] = 1  --> Disable the Mask on EXTI 3.4.5
	EXTI->RTSR |= (0x07<<3);  // Bit[1] = 1  --> Enable Rising Edge Trigger for PB 3,4,5
	EXTI->FTSR |= (0x07<<3);  // Bit[1] = 1  --> Enable Falling Edge Trigger for PB 3,4,5
	NVIC_SetPriority(EXTI3_IRQn, 0);
	NVIC_SetPriority(EXTI4_IRQn, 0);
	NVIC_SetPriority(EXTI9_5_IRQn, 0);
	NVIC_EnableIRQ (EXTI3_IRQn);  // Enable Interrupt for PB3
	NVIC_EnableIRQ (EXTI4_IRQn);  // Enable Interrupt for PB4
	NVIC_EnableIRQ (EXTI9_5_IRQn);  // Enable Interrupt for PB5
	
	//Timers Interrupts Config
	TIM2->DIER = (1<<0);
	TIM3->DIER = (1<<0);
	TIM4->DIER = (1<<0);
	NVIC_SetPriority(TIM2_IRQn, 0);
	NVIC_SetPriority(TIM3_IRQn, 0);
	NVIC_SetPriority(TIM4_IRQn, 0);
	NVIC_EnableIRQ (TIM2_IRQn);  // Enable Interrupt for PB3
	NVIC_EnableIRQ (TIM3_IRQn);  // Enable Interrupt for PB4
	NVIC_EnableIRQ (TIM4_IRQn);  // Enable Interrupt for PB5
}

void TIM2_IRQHandler(void) {
	if ((TIM2->SR & TIM_SR_UIF) != 0) // if UIF flag is set
    {
      greenEnd += 1;
			TIM2->SR &= ~TIM_SR_UIF; // clear UIF flag
				
    }
}

void TIM3_IRQHandler(void) {
	if ((TIM3->SR & TIM_SR_UIF) != 0) // if UIF flag is set
    {
      yellowEnd += 1;
			TIM3->SR &= ~TIM_SR_UIF; // clear UIF flag
				
    }
}

void TIM4_IRQHandler(void) {
	if ((TIM4->SR & TIM_SR_UIF) != 0) // if UIF flag is set
    {
      warnEnd += 1;
    	warnEnds += 1;
    	if(warnEnds >= 6) {
    		warnEnds = 0;
    	}
			TIM4->SR &= ~TIM_SR_UIF; // clear UIF flag
				
    }
}

void EXTI3_IRQHandler(void) {
	if (EXTI->PR & (1<<3))    // If the PB3 triggered the interrupt
	{
		if((((GPIOB->IDR)&0x08)>>3)==1) {
			// Rising edge (button released)
			checkWest = 1;
		} else {
		  // Falling edge (button pressed)
			checkWest = 0;
		}
		EXTI->PR |= (1<<3);  // Clear the interrupt flag by writing a 1 
	}
	inputValue = (checkWalk << 2) | (checkSouth << 1) | (checkWest << 0);
	checkGPIO = checkWalk | checkSouth |  checkWest;
}

void EXTI4_IRQHandler(void) {
	if (EXTI->PR & (1<<4))    // If the PB3 triggered the interrupt
	{
		if((((GPIOB->IDR)&0x10)>>4)==1) {
			// Rising edge (button released)
			checkSouth = 1;
		} else {
		  // Falling edge (button pressed)
			checkSouth = 0;
		}
		EXTI->PR |= (1<<4);  // Clear the interrupt flag by writing a 1 
	}
	inputValue = (checkWalk << 2) | (checkSouth << 1) | (checkWest << 0);
	checkGPIO = checkWalk | checkSouth |  checkWest;
}

void EXTI9_5_IRQHandler(void) {
	if (EXTI->PR & (1<<5))    // If the PB5 triggered the interrupt
	{
		if((((GPIOB->IDR)&0x20)>>5)==1) {
			// Rising edge (button released)
			checkWalk = 1;
		} else {
		  // Falling edge (button pressed)
			checkWalk = 0;
		}
		EXTI->PR |= (1<<5);  // Clear the interrupt flag by writing a 1 
	}
	inputValue = (checkWalk << 2) | (checkSouth << 1) | (checkWest << 0);
	checkGPIO = checkWalk | checkSouth |  checkWest;
}