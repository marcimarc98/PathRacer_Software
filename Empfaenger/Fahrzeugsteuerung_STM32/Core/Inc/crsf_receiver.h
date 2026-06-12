#ifndef CRSF_RECEIVER_H
#define CRSF_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

/* Initialisiert USART1 fuer CRSF-Empfang. */
void crsf_receiver_init(void);

/* Verarbeitet RX- und Fehlerinterrupts des CRSF-UARTs. */
void crsf_receiver_irq_handler(void);

/* Sendet einen CRSF-Rahmen ueber denselben UART, z. B. fuer Telemetrie. */
bool crsf_receiver_send_frame(const uint8_t* frame, uint8_t length);

#endif
