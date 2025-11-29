 
 

#ifndef DRED_DECODER_H
#define DRED_DECODER_H

#include "opus.h"
#include "dred_config.h"
#include "dred_rdovae.h"
#include "entcode.h"
#include "dred_rdovae_constants.h"

struct OpusDRED {
    float        fec_features[2*DRED_NUM_REDUNDANCY_FRAMES*DRED_NUM_FEATURES];
    float        state[DRED_STATE_DIM];
    float        latents[(DRED_NUM_REDUNDANCY_FRAMES/2)*DRED_LATENT_DIM];
    int          nb_latents;
    int          process_stage;
    int          dred_offset;
};


int dred_ec_decode(OpusDRED *dec, const opus_uint8 *bytes, int num_bytes, int min_feature_frames, int dred_frame_offset);

#endif
