#ifndef __STM32F10X_STUB_H
#define __STM32F10X_STUB_H

/*==============================================================================
 *  PC 上编译验证 OLED 驱动用的假 stm32f10x.h
 *
 *  只提供 OLED.c 用到的那点东西，让 GCC 能在电脑上编译这份驱动，
 *  顺便记录每次 GPIO 写，供 I2C 从机模拟器还原显存。
 *  真机编译时用的是 Start\stm32f10x.h，这个文件不参与 Keil 工程。
 *============================================================================*/

#include <stdint.h>
#include <stddef.h>

typedef enum { RESET = 0, SET = 1 } FlagStatus, ITStatus;
typedef enum { DISABLE = 0, ENABLE = 1 } FunctionalState;
typedef enum { Bit_RESET = 0, Bit_SET = 1 } BitAction;

typedef struct { uint32_t dummy; } GPIO_TypeDef;

extern GPIO_TypeDef *const GPIOA;
extern GPIO_TypeDef *const GPIOB;
extern GPIO_TypeDef *const GPIOC;
extern GPIO_TypeDef *const GPIOD;

typedef enum
{
	GPIO_Mode_AIN = 0x0,
	GPIO_Mode_IN_FLOATING = 0x04,
	GPIO_Mode_IPD = 0x28,
	GPIO_Mode_IPU = 0x48,
	GPIO_Mode_Out_OD = 0x14,
	GPIO_Mode_Out_PP = 0x10,
	GPIO_Mode_AF_OD = 0x1C,
	GPIO_Mode_AF_PP = 0x18
} GPIOMode_TypeDef;

typedef enum
{
	GPIO_Speed_10MHz = 1,
	GPIO_Speed_2MHz,
	GPIO_Speed_50MHz
} GPIOSpeed_TypeDef;

typedef struct
{
	uint16_t GPIO_Pin;
	GPIOSpeed_TypeDef GPIO_Speed;
	GPIOMode_TypeDef GPIO_Mode;
} GPIO_InitTypeDef;

#define GPIO_Pin_0   ((uint16_t)0x0001)
#define GPIO_Pin_1   ((uint16_t)0x0002)
#define GPIO_Pin_2   ((uint16_t)0x0004)
#define GPIO_Pin_3   ((uint16_t)0x0008)
#define GPIO_Pin_4   ((uint16_t)0x0010)
#define GPIO_Pin_5   ((uint16_t)0x0020)
#define GPIO_Pin_8   ((uint16_t)0x0100)
#define GPIO_Pin_9   ((uint16_t)0x0200)
#define GPIO_Pin_11  ((uint16_t)0x0800)

#define RCC_APB2Periph_GPIOA  ((uint32_t)0x00000004)
#define RCC_APB2Periph_GPIOB  ((uint32_t)0x00000008)
#define RCC_APB2Periph_GPIOC  ((uint32_t)0x00000010)
#define RCC_APB2Periph_GPIOD  ((uint32_t)0x00000020)

void RCC_APB2PeriphClockCmd(uint32_t periph, FunctionalState state);
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
void GPIO_WriteBit(GPIO_TypeDef *port, uint16_t pin, BitAction value);
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pin);
void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pin);
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pin);

#endif
