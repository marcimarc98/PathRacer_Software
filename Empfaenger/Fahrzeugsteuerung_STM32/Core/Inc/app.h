#ifndef APP_H
#define APP_H

/* Initialisiert alle Projektmodule des Fahrzeugcontrollers. */
void app_init(void);

/* Fuehrt einen zyklischen Steuerungsdurchlauf aus. */
void app_loop(void);

/* Weiterleitung fuer den CRSF-UART-Interrupt. */
void app_uart_irq_handler(void);

#endif
