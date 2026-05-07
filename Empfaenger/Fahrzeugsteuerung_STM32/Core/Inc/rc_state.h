#ifndef RC_STATE_H
#define RC_STATE_H

#include <stdbool.h>
#include <stdint.h>

#define RC_STATE_NUM_CHANNELS 16U
#define RC_STATE_NUM_BUTTONS  7U

typedef struct
{
  uint16_t channels[RC_STATE_NUM_CHANNELS];
  int lenkung_us;
  int gas_us;
  int bremse_us;
  bool knopf[RC_STATE_NUM_BUTTONS];
  uint16_t knopfmaske;
  uint32_t last_update_ms;
  bool signal_valid;
} rc_state_t;

void rc_state_init(void);
void rc_state_update_from_channels(const uint16_t channels[RC_STATE_NUM_CHANNELS], uint32_t now_ms);
void rc_state_mark_signal_lost(void);
void rc_state_snapshot(rc_state_t* out_state);
bool rc_state_signal_is_recent(const rc_state_t* state, uint32_t now_ms, uint32_t timeout_ms);

#endif
