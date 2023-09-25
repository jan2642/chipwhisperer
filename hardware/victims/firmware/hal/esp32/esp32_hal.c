#include "esp32_hal.h"
#include <stdint.h>

typedef enum {
    ETS_OK     = 0,
    ETS_FAILED = 1,
    ETS_PENDING = 2,
    ETS_BUSY = 3,
    ETS_CANCEL = 4,
} ETS_STATUS;

/* Prototypes of functions provided by the boot rom */

char uart_rx_one_char_block(void);
ETS_STATUS uart_rx_one_char(uint8_t *c);
void uart_tx_one_char(uint8_t c);
void gpio_output_set(uint32_t set_mask, uint32_t clear_mask, uint32_t enable_mask, uint32_t disable_mask);
void ets_install_uart_printf(void);

/* Registers */

#define RTC_CNTL_WDTCONFIG0_REG     *(uint32_t*)(0x3ff48000 + 0x008c)
#define RTC_CNTL_WDTWPROTECT_REG    *(uint32_t*)(0x3ff48000 + 0x00a4)
#define RTC_CNTL_CLK_CONF_REG       *(uint32_t*)(0x3ff48000 + 0x0070)
#define TIMG0_T0CONFIG_REG          *(uint32_t*)(0x3ff5f000 + 0x0000)
#define TIMG0_T0_WDTCONFIG0_REG     *(uint32_t*)(0x3ff5f000 + 0x0048)
#define TIMG0_T0_WDTCONFIG1_REG     *(uint32_t*)(0x3ff5f000 + 0x004c)
#define TIMG0_T0_WDTCONFIG2_REG     *(uint32_t*)(0x3ff5f000 + 0x0050)
#define TIMG0_T0_WDTCONFIG3_REG     *(uint32_t*)(0x3ff5f000 + 0x0054)
#define TIMG0_T0_WDTCONFIG4_REG     *(uint32_t*)(0x3ff5f000 + 0x0058)
#define TIMG0_T0_WDTCONFIG5_REG     *(uint32_t*)(0x3ff5f000 + 0x005c)
#define TIMG0_T0_WDTWPROTECT_REG    *(uint32_t*)(0x3ff5f000 + 0x0064)
#define TIMG0_T0_INT_ENA_REG        *(uint32_t*)(0x3ff5f000 + 0x0098)
#define SYSCON_SYSCLK_CONF_REG      *(uint32_t*)(0x3ff66000 + 0x0000)
#define UART0_CLKDIV_REG            *(uint32_t*)(0x3FF40000 + 0x0014)

void platform_init(void)
{
    /* Disable watchdog */
    RTC_CNTL_WDTWPROTECT_REG = 0x050d83aa1;
    RTC_CNTL_WDTCONFIG0_REG =  0x0;

    /* Disable TIMG0_T0 watchdog */
    TIMG0_T0_WDTWPROTECT_REG = 0x050d83aa1;
    TIMG0_T0_INT_ENA_REG =     0x0;
    TIMG0_T0CONFIG_REG =       0x0;
    TIMG0_T0_WDTCONFIG0_REG =  0x0;
    TIMG0_T0_WDTCONFIG1_REG =  0xffffffff;
    TIMG0_T0_WDTCONFIG2_REG =  0xffffffff;
    TIMG0_T0_WDTCONFIG3_REG =  0xffffffff;
    TIMG0_T0_WDTCONFIG4_REG =  0xffffffff;
    TIMG0_T0_WDTCONFIG5_REG =  0xffffffff;

    /* Set CPU & APB to the same clock as XTAL */
    RTC_CNTL_CLK_CONF_REG &= ~(0b11 << 27);
    SYSCON_SYSCLK_CONF_REG = 0;
}

static void set_baudrate(int baud)
{
	const uint32_t sclk = 7370130;              /* Default OpenADC frequency */
    const uint32_t max_div = (1 << 12) - 1;
#define DIV_UP(a, b) (((a) + (b) - 1) / (b))
    int sclk_div = DIV_UP(sclk, max_div * baud);
#undef DIV_UP
    uint32_t clk_div = (sclk << 4) / (baud * sclk_div);

	UART0_CLKDIV_REG = ((clk_div & 0xf) << 20) | (clk_div >> 4);
}

void init_uart(void)
{
    ets_install_uart_printf();

#if SS_VER == SS_VER_2_1
    set_baudrate(230400);
#else
    set_baudrate(38400);
#endif
}

void putch(char c)
{
    uart_tx_one_char(c);
}

char getch(void)
{
    char c;
#if 0
    c = uart_rx_one_char_block();
#else
    ETS_STATUS r;
    do {
        r = uart_rx_one_char(&c);
    } while (r != ETS_OK);
#endif
    return c;
}

void trigger_low(void)
{
    gpio_output_set(0, (1 << 4), (1 << 4), 0);
}

void trigger_high(void)
{
    gpio_output_set((1 << 4), 0, (1 << 4), 0);
}

void trigger_setup(void)
{
    trigger_low();

#if 0
    gpio_output_set((1 << 23), 0, (1 << 23), 0); /* Enable LED1 */
    gpio_output_set((1 << 18), 0, (1 << 18), 0); /* Enable LED2 */
#endif
    gpio_output_set((1 << 5), 0, (1 << 5), 0); /* Enable LED3 */
}

