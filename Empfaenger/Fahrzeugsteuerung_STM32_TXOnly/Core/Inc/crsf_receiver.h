#ifndef CRSF_RECEIVER_H
#define CRSF_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

void crsf_receiver_init(void);
void crsf_receiver_irq_handler(void);
bool crsf_receiver_send_frame(const uint8_t* frame, uint8_t length);

#endif
