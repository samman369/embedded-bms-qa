#ifndef APP_H
#define APP_H

#include <stdint.h>

void     app_init(void);
void     app_uart_rx_isr(uint8_t byte);  /* called from the UART RX interrupt */
void     app_poll(void);                 /* called from the main loop          */
void     app_tick(void);                 /* called every BMS_TICK_MS           */
uint16_t app_rx_overflow_count(void);

#endif /* APP_H */
