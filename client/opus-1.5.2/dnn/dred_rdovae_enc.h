 
 

#ifndef DRED_RDOVAE_ENC_H
#define DRED_RDOVAE_ENC_H

#include "dred_rdovae.h"

#include "dred_rdovae_enc_data.h"

struct RDOVAEEncStruct {
    int initialized;
    float gru1_state[ENC_GRU1_STATE_SIZE];
    float gru2_state[ENC_GRU2_STATE_SIZE];
    float gru3_state[ENC_GRU3_STATE_SIZE];
    float gru4_state[ENC_GRU4_STATE_SIZE];
    float gru5_state[ENC_GRU5_STATE_SIZE];
    float conv1_state[ENC_CONV1_STATE_SIZE];
    float conv2_state[2*ENC_CONV2_STATE_SIZE];
    float conv3_state[2*ENC_CONV3_STATE_SIZE];
    float conv4_state[2*ENC_CONV4_STATE_SIZE];
    float conv5_state[2*ENC_CONV5_STATE_SIZE];
};

void dred_rdovae_encode_dframe(RDOVAEEncState *enc_state, const RDOVAEEnc *model, float *latents, float *initial_state, const float *input, int arch);


#endif
