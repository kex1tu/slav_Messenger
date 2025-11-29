 
 

#ifndef DRED_ENCODER_H
#define DRED_ENCODER_H

#include "lpcnet.h"
#include "dred_config.h"
#include "dred_rdovae.h"
#include "entcode.h"
#include "lpcnet_private.h"
#include "dred_rdovae_enc.h"
#include "dred_rdovae_enc_data.h"

#define RESAMPLING_ORDER 8

typedef struct {
    RDOVAEEnc model;
    LPCNetEncState lpcnet_enc_state;
    RDOVAEEncState rdovae_enc;
    int loaded;
    opus_int32 Fs;
    int channels;

#define DREDENC_RESET_START input_buffer
    float input_buffer[2*DRED_DFRAME_SIZE];
    int input_buffer_fill;
    int dred_offset;
    int latent_offset;
    int last_extra_dred_offset;
    float latents_buffer[DRED_MAX_FRAMES * DRED_LATENT_DIM];
    int latents_buffer_fill;
    float state_buffer[DRED_MAX_FRAMES * DRED_STATE_DIM];
    float resample_mem[RESAMPLING_ORDER + 1];
} DREDEnc;

int dred_encoder_load_model(DREDEnc* enc, const void *data, int len);
void dred_encoder_init(DREDEnc* enc, opus_int32 Fs, int channels);
void dred_encoder_reset(DREDEnc* enc);

void dred_deinit_encoder(DREDEnc *enc);

void dred_compute_latents(DREDEnc *enc, const float *pcm, int frame_size, int extra_delay, int arch);

int dred_encode_silk_frame(DREDEnc *enc, unsigned char *buf, int max_chunks, int max_bytes, int q0, int dQ, int qmax, unsigned char *activity_mem, int arch);

#endif
