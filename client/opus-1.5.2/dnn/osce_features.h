 
 

#ifndef OSCE_FEATURES_H
#define OSCE_FEATURES_H


#include "structs.h"
#include "opus_types.h"

#define OSCE_NUMBITS_BUGFIX

void osce_calculate_features(
    silk_decoder_state          *psDec,                          
    silk_decoder_control        *psDecCtrl,                      
    float                       *features,                       
    float                       *numbits,                        
    int                         *periods,                        
    const opus_int16            xq[],                            
    opus_int32                  num_bits                         
);


void osce_cross_fade_10ms(float *x_enhanced, float *x_in, int length);

#endif
