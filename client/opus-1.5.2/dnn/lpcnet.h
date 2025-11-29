 
 

#ifndef LPCNET_H_
#define LPCNET_H_

#include "opus_types.h"

#define NB_FEATURES 20
#define NB_TOTAL_FEATURES 36

 
#define LPCNET_FRAME_SIZE (160)

typedef struct LPCNetState LPCNetState;

typedef struct LPCNetDecState LPCNetDecState;

typedef struct LPCNetEncState LPCNetEncState;

typedef struct LPCNetPLCState LPCNetPLCState;


 
int lpcnet_decoder_get_size(void);

 
int lpcnet_decoder_init(LPCNetDecState *st);

void lpcnet_reset(LPCNetState *lpcnet);

 
LPCNetDecState *lpcnet_decoder_create(void);

 
void lpcnet_decoder_destroy(LPCNetDecState *st);

 
int lpcnet_decode(LPCNetDecState *st, const unsigned char *buf, opus_int16 *pcm);



 
int lpcnet_encoder_get_size(void);

 
int lpcnet_encoder_init(LPCNetEncState *st);

int lpcnet_encoder_load_model(LPCNetEncState *st, const void *data, int len);

 
LPCNetEncState *lpcnet_encoder_create(void);

 
void lpcnet_encoder_destroy(LPCNetEncState *st);

 
int lpcnet_encode(LPCNetEncState *st, const opus_int16 *pcm, unsigned char *buf);

 
int lpcnet_compute_single_frame_features(LPCNetEncState *st, const opus_int16 *pcm, float features[NB_TOTAL_FEATURES], int arch);


 
int lpcnet_compute_single_frame_features_float(LPCNetEncState *st, const float *pcm, float features[NB_TOTAL_FEATURES], int arch);

 
int lpcnet_get_size(void);

 
int lpcnet_init(LPCNetState *st);

 
LPCNetState *lpcnet_create(void);

 
void lpcnet_destroy(LPCNetState *st);

 
void lpcnet_synthesize(LPCNetState *st, const float *features, opus_int16 *output, int N);



int lpcnet_plc_init(LPCNetPLCState *st);
void lpcnet_plc_reset(LPCNetPLCState *st);

int lpcnet_plc_update(LPCNetPLCState *st, opus_int16 *pcm);

int lpcnet_plc_conceal(LPCNetPLCState *st, opus_int16 *pcm);

void lpcnet_plc_fec_add(LPCNetPLCState *st, const float *features);

void lpcnet_plc_fec_clear(LPCNetPLCState *st);

int lpcnet_load_model(LPCNetState *st, const void *data, int len);
int lpcnet_plc_load_model(LPCNetPLCState *st, const void *data, int len);

#endif
