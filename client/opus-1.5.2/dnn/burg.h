 

#ifndef BURG_H
#define BURG_H


float silk_burg_analysis(               
    float          A[],                 
    const float    x[],                 
    const float    minInvGain,          
    const int      subfr_length,        
    const int      nb_subfr,            
    const int      D                    
);

#endif
