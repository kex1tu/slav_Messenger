 
 

#ifndef FARGAN_H
#define FARGAN_H

#include "freq.h"
#include "fargan_data.h"
#include "pitchdnn.h"

#define FARGAN_CONT_SAMPLES 320
#define FARGAN_NB_SUBFRAMES 4
#define FARGAN_SUBFRAME_SIZE 40
#define FARGAN_FRAME_SIZE (FARGAN_NB_SUBFRAMES*FARGAN_SUBFRAME_SIZE)
#define FARGAN_COND_SIZE (COND_NET_FDENSE2_OUT_SIZE/FARGAN_NB_SUBFRAMES)
#define FARGAN_DEEMPHASIS 0.85f

#define SIG_NET_INPUT_SIZE (FARGAN_COND_SIZE+2*FARGAN_SUBFRAME_SIZE+4)
#define SIG_NET_FWC0_STATE_SIZE (2*SIG_NET_INPUT_SIZE)

#define FARGAN_MAX_RNN_NEURONS SIG_NET_GRU1_OUT_SIZE
typedef struct {
  FARGAN model;
  int arch;
  int cont_initialized;
  float deemph_mem;
  float pitch_buf[PITCH_MAX_PERIOD];
  float cond_conv1_state[COND_NET_FCONV1_STATE_SIZE];
  float fwc0_mem[SIG_NET_FWC0_STATE_SIZE];
  float gru1_state[SIG_NET_GRU1_STATE_SIZE];
  float gru2_state[SIG_NET_GRU2_STATE_SIZE];
  float gru3_state[SIG_NET_GRU3_STATE_SIZE];
  int last_period;
} FARGANState;

void fargan_init(FARGANState *st);
int fargan_load_model(FARGANState *st, const void *data, int len);

void fargan_cont(FARGANState *st, const float *pcm0, const float *features0);

void fargan_synthesize(FARGANState *st, float *pcm, const float *features);
void fargan_synthesize_int(FARGANState *st, opus_int16 *pcm, const float *features);


#endif  
