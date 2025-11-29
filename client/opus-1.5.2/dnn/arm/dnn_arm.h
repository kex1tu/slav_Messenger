 
 

#ifndef DNN_ARM_H
#define DNN_ARM_H

#include "cpu_support.h"
#include "opus_types.h"

void compute_linear_dotprod(const LinearLayer *linear, float *out, const float *in);
void compute_linear_neon(const LinearLayer *linear, float *out, const float *in);

void compute_activation_neon(float *output, const float *input, int N, int activation);
void compute_activation_dotprod(float *output, const float *input, int N, int activation);

void compute_conv2d_neon(const Conv2dLayer *conv, float *out, float *mem, const float *in, int height, int hstride, int activation);
void compute_conv2d_dotprod(const Conv2dLayer *conv, float *out, float *mem, const float *in, int height, int hstride, int activation);

#if defined(OPUS_ARM_PRESUME_DOTPROD)

#define OVERRIDE_COMPUTE_LINEAR
#define compute_linear(linear, out, in, arch) ((void)(arch),compute_linear_dotprod(linear, out, in))

#elif defined(OPUS_ARM_PRESUME_NEON_INTR) && !defined(OPUS_ARM_MAY_HAVE_DOTPROD)

#define OVERRIDE_COMPUTE_LINEAR
#define compute_linear(linear, out, in, arch) ((void)(arch),compute_linear_neon(linear, out, in))

#elif defined(OPUS_HAVE_RTCD) && (defined(OPUS_ARM_MAY_HAVE_DOTPROD) || defined(OPUS_ARM_MAY_HAVE_NEON))

extern void (*const DNN_COMPUTE_LINEAR_IMPL[OPUS_ARCHMASK + 1])(
                    const LinearLayer *linear,
                    float *out,
                    const float *in
                    );
#define OVERRIDE_COMPUTE_LINEAR
#define compute_linear(linear, out, in, arch) \
    ((*DNN_COMPUTE_LINEAR_IMPL[(arch) & OPUS_ARCHMASK])(linear, out, in))


#endif

#if defined(OPUS_ARM_PRESUME_NEON)

#define OVERRIDE_COMPUTE_ACTIVATION
#define compute_activation(output, input, N, activation, arch) ((void)(arch),compute_activation_neon(output, input, N, activation))
#define OVERRIDE_COMPUTE_CONV2D
#define compute_conv2d(conv, out, mem, in, height, hstride, activation, arch) ((void)(arch),compute_conv2d_neon(conv, out, mem, in, height, hstride, activation))

#elif defined(OPUS_HAVE_RTCD) && (defined(OPUS_ARM_MAY_HAVE_DOTPROD) || defined(OPUS_ARM_MAY_HAVE_NEON))

extern void (*const DNN_COMPUTE_ACTIVATION_IMPL[OPUS_ARCHMASK + 1])(
                    float *output,
                    const float *input,
                    int N,
                    int activation
                    );
#define OVERRIDE_COMPUTE_ACTIVATION
#define compute_activation(output, input, N, activation, arch) \
    ((*DNN_COMPUTE_ACTIVATION_IMPL[(arch) & OPUS_ARCHMASK])(output, input, N, activation))


extern void (*const DNN_COMPUTE_CONV2D_IMPL[OPUS_ARCHMASK + 1])(
                    const Conv2dLayer *conv,
                    float *out,
                    float *mem,
                    const float *in,
                    int height,
                    int hstride,
                    int activation
                    );
#define OVERRIDE_COMPUTE_CONV2D
#define compute_conv2d(conv, out, mem, in, height, hstride, activation, arch) \
    ((*DNN_COMPUTE_CONV2D_IMPL[(arch) & OPUS_ARCHMASK])(conv, out, mem, in, height, hstride, activation))


#endif


#endif  
