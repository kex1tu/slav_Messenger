 
 

#ifndef OSCE_H
#define OSCE_H


#include "opus_types.h"
 
#ifndef DISABLE_LACE
#include "lace_data.h"
#endif
#ifndef DISABLE_NOLACE
#include "nolace_data.h"
#endif
#include "nndsp.h"
#include "nnet.h"
#include "osce_structs.h"
#include "structs.h"

#define OSCE_METHOD_NONE 0
#ifndef DISABLE_LACE
#define OSCE_METHOD_LACE 1
#endif
#ifndef DISABLE_NOLACE
#define OSCE_METHOD_NOLACE 2
#endif

#if !defined(DISABLE_NOLACE)
#define OSCE_DEFAULT_METHOD OSCE_METHOD_NOLACE
#define OSCE_MAX_RNN_NEURONS NOLACE_FNET_GRU_STATE_SIZE
#elif !defined(DISABLE_LACE)
#define OSCE_DEFAULT_METHOD OSCE_METHOD_LACE
#define OSCE_MAX_RNN_NEURONS LACE_FNET_GRU_STATE_SIZE
#else
#define OSCE_DEFAULT_METHOD OSCE_METHOD_NONE
#define OSCE_MAX_RNN_NEURONS 0
#endif




 


void osce_enhance_frame(
    OSCEModel                   *model,                          
    silk_decoder_state          *psDec,                          
    silk_decoder_control        *psDecCtrl,                      
    opus_int16                  xq[],                            
    opus_int32                  num_bits,                        
    int                         arch                             
);


int osce_load_models(OSCEModel *hModel, const void *data, int len);
void osce_reset(silk_OSCE_struct *hOSCE, int method);


#endif
