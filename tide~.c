/**
    @file
    tide~: Mutable Instruments Tides-based LFO/envelope generator external
    Implements the authentic Tides 2 PolySlopeGenerator algorithm
    @ingroup examples
*/

#include "ext.h"        // standard Max include, always required
#include "ext_obex.h"   // required for "new" style objects
#include "z_dsp.h"      // required for MSP objects

// Forward declaration for C++ Tides interface
#ifdef __cplusplus
extern "C" {
#endif

// C wrapper functions for Tides C++ code
void* tides_create(void);
void tides_destroy(void* tides_obj);
void tides_init(void* tides_obj);
void tides_reset_phase(void* tides_obj);
void tides_render(void* tides_obj, int ramp_mode, int output_mode, int range,
                  float frequency, float pw, float shape, float smoothness, float shift,
                  unsigned char gate_flags, float* output);

#ifdef __cplusplus
}
#endif

// struct to represent the object's state
typedef struct _tide
{
    t_pxobject ob;                  // the object itself (t_pxobject for MSP)
    
    // Tides DSP object (opaque pointer to C++ object)
    void* poly_slope_generator;
    
    
    // Float parameters for control (set via messages)
    double frequency_float;         // Frequency in Hz
    double shape_float;             // Shape parameter (0-1)
    double slope_float;             // Slope parameter (0-1, was called 'pw' in Tides)
    double smooth_float;            // Smoothness parameter (0-1)
    double phase_float;             // Phase offset (0-1)
    double freq_scale;              // Frequency scaling factor
    
    // Signal connection status (following lores~ pattern)
    short freq_has_signal;          // 1 if frequency inlet has signal connection
    short shape_has_signal;         // 1 if shape inlet has signal connection
    short slope_has_signal;         // 1 if slope inlet has signal connection
    short smooth_has_signal;        // 1 if smooth inlet has signal connection
    short phase_has_signal;         // 1 if phase inlet has signal connection
    
    
    // Gate flags (unused in loop mode but needed for Tides interface)
    unsigned char gate_flags;       // Tides gate flags
    
    // Phase reset flag
    short reset_phase;              // 1 to reset phase on next sample
    
    // Sample rate
    double sample_rate;
    
} t_tide;

// Method prototypes
void* tide_new(t_symbol* s, long argc, t_atom* argv);
void tide_free(t_tide* x);
void tide_assist(t_tide* x, void* b, long m, long a, char* s);
void tide_float(t_tide* x, double f);
void tide_bang(t_tide* x);
void tide_dsp64(t_tide* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void tide_perform64(t_tide* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);


// Global class pointer variable
static t_class* tide_class = NULL;

//----------------------------------------------------------------------------------------------

void ext_main(void* r)
{
    // Object creation
    t_class* c = class_new("tide~", (method)tide_new, (method)tide_free, (long)sizeof(t_tide), 0L, A_GIMME, 0);

    class_addmethod(c, (method)tide_float, "float", A_FLOAT, 0);
    class_addmethod(c, (method)tide_bang, "bang", 0);
    class_addmethod(c, (method)tide_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)tide_assist, "assist", A_CANT, 0);
    
    // Add frequency scaling attribute
    CLASS_ATTR_DOUBLE(c, "freqscale", 0, t_tide, freq_scale);
    CLASS_ATTR_FILTER_MIN(c, "freqscale", 0.0001);
    CLASS_ATTR_FILTER_MAX(c, "freqscale", 1.0);
    CLASS_ATTR_DEFAULT(c, "freqscale", 0, "1.0");
    CLASS_ATTR_LABEL(c, "freqscale", 0, "Frequency Scale");
    CLASS_ATTR_SAVE(c, "freqscale", 0);
    
    class_dspinit(c);


    class_register(CLASS_BOX, c);
    tide_class = c;
}

//----------------------------------------------------------------------------------------------

void* tide_new(t_symbol* s, long argc, t_atom* argv)
{
    t_tide* x = (t_tide*)object_alloc(tide_class);

    if (x) {
        dsp_setup((t_pxobject*)x, 5);  // 5 inlets: freq, shape, slope, smooth, phase
        outlet_new(x, "signal");        // 1 outlet: waveform


        // Create Tides C++ object
        x->poly_slope_generator = tides_create();
        if (x->poly_slope_generator) {
            tides_init(x->poly_slope_generator);
        }

        // Initialize parameters with defaults
        x->frequency_float = 1.0;       // 1 Hz
        x->shape_float = 0.0;           // Linear
        x->slope_float = 0.5;           // Balanced attack/decay
        x->smooth_float = 0.0;          // No smoothing
        x->phase_float = 0.0;           // No phase offset
        x->freq_scale = 1.0;            // Default to 1.0 (no scaling)
        
        // Initialize connection status (assume no signals connected initially)
        x->freq_has_signal = 0;
        x->shape_has_signal = 0;
        x->slope_has_signal = 0;
        x->smooth_has_signal = 0;
        x->phase_has_signal = 0;
        
        x->gate_flags = 0;
        x->reset_phase = 0;
        x->sample_rate = 44100.0;

        // Process attributes
        attr_args_process(x, argc, argv);
    }

    return x;
}

//----------------------------------------------------------------------------------------------

void tide_free(t_tide* x)
{
    if (x->poly_slope_generator) {
        tides_destroy(x->poly_slope_generator);
    }
    
    
    dsp_free((t_pxobject*)x);
}

//----------------------------------------------------------------------------------------------

void tide_assist(t_tide* x, void* b, long m, long a, char* s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0: strcpy(s, "(signal/float/bang) Frequency (Hz) / Reset Phase"); break;
            case 1: strcpy(s, "(signal/float) Shape (0-1)"); break;
            case 2: strcpy(s, "(signal/float) Slope (0-1)"); break;
            case 3: strcpy(s, "(signal/float) Smooth (0-1)"); break;
            case 4: strcpy(s, "(signal/float) Phase (0-1)"); break;
        }
    }
    else {
        strcpy(s, "(signal) Waveform Output");
    }
}

//----------------------------------------------------------------------------------------------

void tide_float(t_tide* x, double f)
{
    // Route float messages to specific inlets
    long inlet = proxy_getinlet((t_object*)x);
    
    switch (inlet) {
        case 0: 
            x->frequency_float = CLAMP(f, 0.000001, 1000.0);  // Allow down to 0.000001 Hz
            break;
        case 1: 
            x->shape_float = CLAMP(f, 0.0, 1.0);
            break;
        case 2: 
            x->slope_float = CLAMP(f, 0.0, 1.0);
            break;
        case 3: 
            x->smooth_float = CLAMP(f, 0.0, 1.0);
            break;
        case 4: 
            x->phase_float = CLAMP(f, 0.0, 1.0);
            break;
    }
}

//----------------------------------------------------------------------------------------------

void tide_bang(t_tide* x)
{
    // Reset phase to 0 for synchronization
    // Check which inlet received the bang (should be inlet 0 for frequency/reset)
    long inlet = proxy_getinlet((t_object*)x);
    
    if (inlet == 0) {  // Frequency inlet accepts bangs for phase reset
        x->reset_phase = 1;  // Set flag to reset phase on next audio sample
        post("tide~: phase reset");
    }
}

//----------------------------------------------------------------------------------------------

void tide_dsp64(t_tide* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    x->sample_rate = samplerate;
    
    // Store signal connection status (following lores~ pattern)
    x->freq_has_signal = count[0];
    x->shape_has_signal = count[1]; 
    x->slope_has_signal = count[2];
    x->smooth_has_signal = count[3];
    x->phase_has_signal = count[4];
    
    object_method(dsp64, gensym("dsp_add64"), x, tide_perform64, 0, NULL);
}

//----------------------------------------------------------------------------------------------

void tide_perform64(t_tide* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    // Input buffers for all 5 inlets
    double* freq_in = ins[0];
    double* shape_in = ins[1];
    double* slope_in = ins[2];
    double* smooth_in = ins[3];
    double* phase_in = ins[4];

    // Output buffer
    double* out = outs[0];

    // Check if Tides object exists
    if (!x->poly_slope_generator) {
        // Output silence if Tides object failed to create
        for (long i = 0; i < sampleframes; i++) {
            out[i] = 0.0;
        }
        return;
    }

    // Process each sample
    for (long i = 0; i < sampleframes; i++) {
        // Choose signal vs float for each inlet (following lores~ pattern)
        double current_freq = x->freq_has_signal ? freq_in[i] : x->frequency_float;
        current_freq *= x->freq_scale;  // Apply frequency scaling
        double current_shape = x->shape_has_signal ? shape_in[i] : x->shape_float;
        double current_slope = x->slope_has_signal ? slope_in[i] : x->slope_float;
        double current_smooth = x->smooth_has_signal ? smooth_in[i] : x->smooth_float;
        double current_phase = x->phase_has_signal ? phase_in[i] : x->phase_float;

        // Convert frequency from Hz to normalized phase increment per sample
        float norm_frequency = (float)(current_freq / x->sample_rate);
        norm_frequency = CLAMP(norm_frequency, 0.0f, 0.5f);  // Remove lower limit

        // Handle phase reset from bang message
        if (x->reset_phase) {
            tides_reset_phase(x->poly_slope_generator);
            x->reset_phase = 0;  // Clear the flag
        }
        
        // No gate detection needed for loop mode - just clear flags
        x->gate_flags = 0;

        // Prepare output buffer for Tides (4 channels, we use first)
        float tides_output[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        // Call Tides render function (Loop mode only)
        tides_render(
            x->poly_slope_generator,
            1,                          // ramp_mode (1=Loop only) 
            1,                          // output_mode (1=AMPLITUDE for standard waveform)
            1,                          // range (1=AUDIO)
            norm_frequency,             // frequency (normalized)
            (float)current_slope,       // pw (pulse width/slope parameter)
            (float)current_shape,       // shape
            (float)current_smooth,      // smoothness
            (float)current_phase,       // shift (phase offset)
            x->gate_flags,              // gate flags
            tides_output                // output buffer
        );

        // Output the first channel
        out[i] = (double)tides_output[0];
    }
}