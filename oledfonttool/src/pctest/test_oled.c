/* PC 端验证：把 OLED 驱动跑一遍，还原出 128x64 显存，再打印成图
 *
 * 前提：这是一个"不用真机就能验证驱动"的测试台，需要用到 Keil 工程里的
 *       OLED.c / OLED.h（驱动本体）和字库。所以它必须和工程在一起跑。
 *
 * 目录关系（默认假设工具就在工程里面）：
 *     <工程>\MyFreeRTOS_C8T6\Hardware\   <- 驱动在这里
 *     <工程>\oledfonttool\src\pctest\    <- 本文件在这里
 *   所以下面用 ..\..\..\MyFreeRTOS_C8T6\Hardware 指过去。
 *   如果你的目录层级不同，改这两处的相对路径即可（或者换成绝对路径）。
 *
 * 编译（在 pctest 目录下）：
 *   set PATH=D:\Qt\Tools\mingw1310_64\bin;%PATH%
 *   gcc -std=c99 -O1 -Wall -I. -I..\..\..\MyFreeRTOS_C8T6\Hardware ^
 *       test_oled.c ..\..\..\MyFreeRTOS_C8T6\Hardware\OLED.c -o test_oled.exe
 *   test_oled.exe
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "stm32f10x.h"
#include "OLED.h"

/*==================== GPIO 桩 + I2C 从机模拟 ====================*/

static GPIO_TypeDef ga, gb, gc, gd;
GPIO_TypeDef *const GPIOA = &ga;
GPIO_TypeDef *const GPIOB = &gb;
GPIO_TypeDef *const GPIOC = &gc;
GPIO_TypeDef *const GPIOD = &gd;

#define SCL_PIN  GPIO_Pin_8      /* PB8 */
#define SDA_PIN  GPIO_Pin_9      /* PB9 */

static uint8_t scl_level = 1, sda_level = 1;
static uint8_t ack_phase = 0;

/* I2C 收包状态机 */
static uint8_t bit_cnt = 0, cur_byte = 0, byte_cnt = 0;
static uint8_t addr_matched = 0, is_data = 0;
static uint8_t cmd = 0xFF, col = 0, page = 0, started = 0;

/* SSD1306 显存：8 页 x 128 列 */
static uint8_t gram[8][128];

void RCC_APB2PeriphClockCmd(uint32_t periph, FunctionalState state)
{
	(void)periph; (void)state;
}

void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init)
{
	(void)port; (void)init;
}

void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pin)   { (void)port; (void)pin; }
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pin) { (void)port; (void)pin; }
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pin)
{
	(void)port; (void)pin; return 1;
}

static void i2c_byte_received(uint8_t b)
{
	/* 第一个字节是地址，第二个是控制字节，之后是命令或数据 */
	if (byte_cnt == 0)
	{
		addr_matched = ((b & 0xFE) == 0x78);       /* 0x78 或 0x79 */
	}
	else if (byte_cnt == 1)
	{
		is_data = ((b & 0x40) != 0);               /* 0x40=数据, 0x00=命令 */
	}
	else if (addr_matched)
	{
		if (is_data)
		{
			if (page < 8 && col < 128)
			{
				gram[page][col] = b;
			}
			if (col < 127) col++;                  /* SSD1306 写完自动右移 */
		}
		else
		{
			if ((b & 0xF8) == 0xB0)      page = b & 0x07;        /* 0xB0~0xB7 设页 */
			else if ((b & 0xF0) == 0x00) col = (col & 0xF0) | (b & 0x0F);  /* 低 4 位 */
			else if ((b & 0xF0) == 0x10) col = (col & 0x0F) | ((b & 0x0F) << 4); /* 高 4 位 */
			else                         cmd = b;
		}
	}
	byte_cnt++;
}

static void i2c_start(void)
{
	byte_cnt = 0; bit_cnt = 0; cur_byte = 0; ack_phase = 0; started = 1;
}

static void i2c_stop(void)
{
	started = 0;
}

/* 每个 SCL 上升沿取一位（高位在前） */
static void i2c_clock_bit(void)
{
	if (!started) return;
	if (ack_phase)          /* 第 9 个时钟是应答位，跳过，否则后面全错位 */
	{
		ack_phase = 0;
		return;
	}
	cur_byte = (uint8_t)((cur_byte << 1) | sda_level);
	if (++bit_cnt == 8)
	{
		bit_cnt = 0;
		i2c_byte_received(cur_byte);
		cur_byte = 0;
		ack_phase = 1;          /* 接下来那个时钟是应答位，不是数据，要丢掉 */
	}
}

void GPIO_WriteBit(GPIO_TypeDef *port, uint16_t pin, BitAction value)
{
	uint8_t v = (value == Bit_SET) ? 1 : 0;
	if (port == GPIOB && pin == SCL_PIN)
	{
		if (v == 1 && scl_level == 0)
		{
			i2c_clock_bit();                       /* 上升沿采一位 */
		}
		scl_level = v;
	}
	else if (port == GPIOB && pin == SDA_PIN)
	{
		/* START: SCL 高时 SDA 下降；STOP: SCL 高时 SDA 上升 */
		if (scl_level == 1)
		{
			if (sda_level == 1 && v == 0) i2c_start();
			else if (sda_level == 0 && v == 1) i2c_stop();
		}
		sda_level = v;
	}
}

/*==================== 把显存打印出来 ====================*/

static void show_gram(void)
{
	int p, y, x;
	for (p = 0; p < 8; p++)
	{
		for (y = 0; y < 8; y++)
		{
			printf("  ");
			for (x = 0; x < 128; x++)
			{
				putchar((gram[p][x] & (1 << y)) ? '#' : '.');
			}
			putchar('\n');
		}
	}
}

int main(void)
{
	int i;

	memset(gram, 0, sizeof(gram));

	OLED_Init();
	printf("=== OLED_Init() 完成, 上电应为全黑 ===\n");
	{
		int n = 0;
		for (i = 0; i < 8; i++) { int x; for (x = 0; x < 128; x++) if (gram[i][x]) n++; }
		printf("非零字节数 = %d (应为 0)\n", n);
	}

	OLED_Clear();

	OLED_ShowString(1, 1, "ASCII OK:");
	OLED_ShowChar(1, 10, 'A');
	OLED_ShowString(1, 12, "1234");
	show_gram();

	printf("\n=== 汉字（单字，居中）===\n");
	memset(gram, 0, sizeof(gram));
	OLED_ShowChinese(2, 4, "中");
	show_gram();

	printf("\n=== 汉字串 \"嵌入式开发网中文测试\" ===\n");
	memset(gram, 0, sizeof(gram));
	OLED_ShowChineseString(1, 1, "嵌入式开发网中文测试");
	show_gram();

	printf("\n=== 中英混排 \"温度:25℃ 正常\" ===\n");
	memset(gram, 0, sizeof(gram));
	OLED_ShowChineseString(1, 1, "温度:25℃ 正常");
	show_gram();

	printf("\n=== 字库里没有的字（应留空不花屏）===\n");
	memset(gram, 0, sizeof(gram));
	OLED_ShowChineseString(1, 1, "龘犇");
	{
		int n = 0;
		for (i = 0; i < 8; i++) { int x; for (x = 0; x < 128; x++) if (gram[i][x]) n++; }
		printf("非零字节数 = %d (应为 0)\n", n);
	}

	return 0;
}
