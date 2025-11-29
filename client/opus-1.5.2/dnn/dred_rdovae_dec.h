 
 

#ifndef DRED_RDOVAE_DEC_H
#define DRED_RDOVAE_DEC_H

#include "dred_rdovae.h"
#include "dred_rdovae_dec_data.h"
#include "dred_rdovae_stats_data.h"

struct RDOVAEDecStruct {
  int initialized;
  float gru1_state[DEC_GRU1_STATE_SIZE];
  float gru2_state[DEC_GRU2_STATE_SIZE];
  float gru3_state[DEC_GRU3_STATE_SIZE];
  float gru4_state[DEC_GRU4_STATE_SIZE];
  float gru5_state[DEC_GRU5_STATE_SIZE];
  float conv1_state[DEC_CONV1_STATE_SIZE];
  float conv2_state[DEC_CONV2_STATE_SIZE];
  float conv3_state[DEC_CONV3_STATE_SIZE];
  float conv4_state[DEC_CONV4_STATE_SIZE];
  float conv5_state[DEC_CONV5_STATE_SIZE];
};

void dred_rdovae_dec_init_states(RDOVAEDecState *h, const RDOVAEDec *model, const float * initial_state, int arch);
void dred_rdovae_decode_qframe(RDOVAEDecState *h, const RDOVAEDec *model, float *qframe, const float * z, int arch);
void DRED_rdovae_decode_all(const RDOVAEDec *model, float *features, const float *state, const float *latents, int nb_latents, int arch);

#endif
