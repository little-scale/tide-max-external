/**
 * tide~ - Simplified Tides-based LFO Max External
 * 
 * Implements the core waveshaping algorithm from Mutable Instruments Tides 2,
 * simplified to a single LFO output with Tides' unique asymmetric ramps,
 * shape controls, and smoothness processing.
 * 
 * Features:
 * - Asymmetric ramp generator with variable slope (attack/decay ratio)
 * - 5 morphable shapes (linear, exponential, logarithmic, sine, arc-sine)
 * - Smoothness processing (2-pole LPF or wavefolder)
 * - Three ramp modes: AD (one-shot), Loop (continuous), AR (attack-release)
 */

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include <math.h>

// Constants
#define SHAPE_TABLE_SIZE 1024
#define NUM_SHAPES 5
#define PI 3.14159265358979323846

// Envelope modes
#define MODE_AD 0     // Attack-Decay (one-shot)
#define MODE_LOOP 1   // Loop (continuous)
#define MODE_AR 2     // Attack-Release (gate)

// Envelope stages
#define STAGE_IDLE 0
#define STAGE_ATTACK 1
#define STAGE_DECAY 2

// External object structure
typedef struct _tide {
    t_pxobject x_obj;
    
    // Core state
    double phase;           // 0.0 to 1.0
    double frequency;       // Hz
    double sr;             // Sample rate
    double sr_recip;       // 1/sr
    
    // Ramp generation
    int ramp_mode;         // 0=AD, 1=Loop, 2=AR
    int envelope_stage;    // 0=idle, 1=attack, 2=decay/release
    double prev_trig;      // For edge detection
    int gate_high;         // For AR mode sustain
    
    // Parameters (all 0-1 range, except frequency)
    double slope;          // Attack/decay ratio (0=fast attack, 1=fast decay)
    double shape_param;    // Morph between shape curves
    double smooth_param;   // Filter cutoff or fold amount
    
    // Float parameter storage for when no signal is connected
    double frequency_float; // Float value for frequency parameter
    double shape_float;     // Float value for shape parameter
    double smooth_float;    // Float value for smoothness parameter
    
    // Connection status for inlets
    int freq_has_signal;    // Whether frequency inlet has signal connected
    int shape_has_signal;   // Whether shape inlet has signal connected
    int smooth_has_signal;  // Whether smoothness inlet has signal connected
    
    // Waveshaping lookup tables
    double shape_lut[NUM_SHAPES][SHAPE_TABLE_SIZE];
    
    // Filter states for smoothness processing
    double lpf_z1, lpf_z2;      // 2-pole filter states
    
    // Inlet number for float message routing
    long m_inletnum;            // Current inlet number
    
} t_tide;

// Function prototypes
void *tide_new(t_symbol *s, long argc, t_atom *argv);
void tide_free(t_tide *x);
void tide_assist(t_tide *x, void *b, long m, long a, char *s);
void tide_dsp64(t_tide *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void tide_perform64(t_tide *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

// Message handlers
void tide_frequency(t_tide *x, double f);
void tide_slope(t_tide *x, double s);
void tide_mode(t_tide *x, long m);
void tide_trigger(t_tide *x);          // Trigger method for envelope modes
void tide_float(t_tide *x, double f);  // Float handler for inlet routing

// DSP utility functions
void tide_init_shapes(t_tide *x);
void tide_advance_phase(t_tide *x);
double tide_apply_shape(t_tide *x, double phase, double shape_param);
double tide_apply_smoothness(t_tide *x, double input, double smooth_param);

// Class pointer
static t_class *tide_class;

//--------------------------------------------------------------------------

void ext_main(void *r) {
    t_class *c = class_new("tide~", (method)tide_new, (method)tide_free, 
                          (long)sizeof(t_tide), 0L, A_GIMME, 0);
    
    class_addmethod(c, (method)tide_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)tide_assist, "assist", A_CANT, 0);
    
    // Message handlers
    class_addmethod(c, (method)tide_frequency, "frequency", A_FLOAT, 0);
    class_addmethod(c, (method)tide_slope, "slope", A_FLOAT, 0);
    class_addmethod(c, (method)tide_mode, "mode", A_LONG, 0);
    class_addmethod(c, (method)tide_trigger, "trigger", 0);
    class_addmethod(c, (method)tide_trigger, "bang", 0);  // Also respond to bang
    class_addmethod(c, (method)tide_float, "float", A_FLOAT, 0);
    
    class_dspinit(c);
    class_register(CLASS_BOX, c);
    tide_class = c;
}

//--------------------------------------------------------------------------

void *tide_new(t_symbol *s, long argc, t_atom *argv) {
    t_tide *x = (t_tide *)object_alloc(tide_class);
    
    if (x) {
        // Initialize DSP with 3 signal inlets: frequency, shape, smoothness
        dsp_setup((t_pxobject *)x, 3);
        
        // Create audio outlet
        outlet_new(x, "signal");
        
        // Initialize parameters with defaults
        x->frequency = 1.0;         // 1 Hz default
        x->slope = 0.5;             // Symmetric ramp
        x->ramp_mode = MODE_LOOP;   // Continuous looping
        x->envelope_stage = STAGE_IDLE;
        x->phase = 0.0;
        x->prev_trig = 0.0;
        x->gate_high = 0;
        
        // Initialize parameter storage
        x->shape_param = 0.0;       // Linear shape
        x->smooth_param = 0.0;      // No smoothness
        
        // Initialize float parameter storage
        x->frequency_float = 1.0;   // Default 1 Hz frequency
        x->shape_float = 0.0;       // Default linear shape
        x->smooth_float = 0.0;      // Default no smoothness
        
        // Initialize connection status (assume no signals connected initially)
        x->freq_has_signal = 0;
        x->shape_has_signal = 0;
        x->smooth_has_signal = 0;
        
        // Initialize filter states
        x->lpf_z1 = 0.0;
        x->lpf_z2 = 0.0;
        
        // Initialize waveshaping lookup tables
        tide_init_shapes(x);
        
        // Process arguments: [frequency] [slope] [mode]
        if (argc >= 1 && atom_gettype(argv) == A_FLOAT) {
            x->frequency = CLAMP(atom_getfloat(argv), 0.001, 100.0);
        }
        if (argc >= 2 && atom_gettype(argv + 1) == A_FLOAT) {
            x->slope = CLAMP(atom_getfloat(argv + 1), 0.001, 0.999);
        }
        if (argc >= 3 && atom_gettype(argv + 2) == A_LONG) {
            x->ramp_mode = CLAMP(atom_getlong(argv + 2), 0, 2);
        }
    }
    
    return x;
}

void tide_free(t_tide *x) {
    dsp_free((t_pxobject *)x);
}

void tide_assist(t_tide *x, void *b, long m, long a, char *s) {
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0: sprintf(s, "frequency Hz (signal/float)"); break;
            case 1: sprintf(s, "shape 0-1 (signal/float): morphs between curves"); break;
            case 2: sprintf(s, "smoothness 0-1 (signal/float): filter/folder"); break;
        }
    } else {
        sprintf(s, "LFO output signal");
    }
}

//--------------------------------------------------------------------------
// Message handlers

void tide_frequency(t_tide *x, double f) {
    x->frequency = CLAMP(f, 0.001, 100.0);
}

void tide_slope(t_tide *x, double s) {
    x->slope = CLAMP(s, 0.001, 0.999);  // Prevent division by zero
}

void tide_mode(t_tide *x, long m) {
    x->ramp_mode = CLAMP(m, 0, 2);
    // Reset envelope state when changing modes
    if (x->ramp_mode != MODE_LOOP) {
        x->envelope_stage = STAGE_IDLE;
        x->phase = 0.0;
    }
}

void tide_trigger(t_tide *x) {
    // Trigger envelope for AD and AR modes
    if (x->ramp_mode != MODE_LOOP) {
        x->phase = 0.0;
        x->envelope_stage = STAGE_ATTACK;
    }
}

void tide_float(t_tide *x, double f) {
    long inlet = ((t_pxobject *)x)->z_in;
    switch (inlet) {
        case 0:  // Frequency inlet
            x->frequency_float = CLAMP(f, 0.001, 100.0);
            break;
        case 1:  // Shape inlet
            x->shape_float = CLAMP(f, 0.0, 1.0);
            break;
        case 2:  // Smoothness inlet  
            x->smooth_float = CLAMP(f, 0.0, 1.0);
            break;
    }
}

//--------------------------------------------------------------------------
// DSP setup and processing

void tide_dsp64(t_tide *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags) {
    x->sr = samplerate;
    x->sr_recip = 1.0 / samplerate;
    
    // Check which inlets have signals connected
    x->freq_has_signal = count[0];     // Inlet 0: frequency
    x->shape_has_signal = count[1];    // Inlet 1: shape
    x->smooth_has_signal = count[2];   // Inlet 2: smoothness
    
    object_method(dsp64, gensym("dsp_add64"), x, tide_perform64, 0, NULL);
}

void tide_perform64(t_tide *x, t_object *dsp64, double **ins, long numins, 
                    double **outs, long numouts, long sampleframes, 
                    long flags, void *userparam) {
    double *freq_in = ins[0];
    double *shape_in = ins[1];
    double *smooth_in = ins[2];
    double *out = outs[0];
    
    for (int i = 0; i < sampleframes; i++) {
        // Use signal input if connected, otherwise use float parameter
        double frequency = x->freq_has_signal ? CLAMP(freq_in[i], 0.001, 100.0) : x->frequency_float;
        double shape = x->shape_has_signal ? CLAMP(shape_in[i], 0.0, 1.0) : x->shape_float;
        double smooth = x->smooth_has_signal ? CLAMP(smooth_in[i], 0.0, 1.0) : x->smooth_float;
        
        // Update frequency for this sample
        x->frequency = frequency;
        
        // Advance phase
        tide_advance_phase(x);
        
        // Generate output
        double output = 0.0;
        
        if (x->envelope_stage != STAGE_IDLE || x->ramp_mode == MODE_LOOP) {
            // Apply shape transformation
            output = tide_apply_shape(x, x->phase, shape);
            
            // Apply smoothness processing
            output = tide_apply_smoothness(x, output, smooth);
        }
        
        out[i] = output;
    }
}

//--------------------------------------------------------------------------
// Shape lookup table generation

void tide_init_shapes(t_tide *x) {
    for (int i = 0; i < SHAPE_TABLE_SIZE; i++) {
        double phase = i / (double)(SHAPE_TABLE_SIZE - 1);
        
        // Linear (no shaping)
        x->shape_lut[0][i] = phase;
        
        // Exponential (fast start, slow end)
        x->shape_lut[1][i] = 1.0 - exp(-5.0 * phase);
        x->shape_lut[1][i] /= (1.0 - exp(-5.0)); // Normalize
        
        // Logarithmic (slow start, fast end)
        x->shape_lut[2][i] = log(1.0 + 9.0 * phase) / log(10.0);
        
        // Sine (smooth S-curve)
        x->shape_lut[3][i] = sin(phase * PI * 0.5);
        
        // Arc-sine (inverse S-curve)
        if (phase < 0.999) {
            x->shape_lut[4][i] = asin(phase) / (PI * 0.5);
        } else {
            x->shape_lut[4][i] = 1.0;
        }
    }
}

//--------------------------------------------------------------------------
// Phase accumulator with asymmetric slope

void tide_advance_phase(t_tide *x) {
    double phase_increment = x->frequency * x->sr_recip;
    
    if (x->ramp_mode == MODE_LOOP) {
        // Continuous looping
        if (x->phase < x->slope) {
            // Rising phase (attack)
            x->phase += phase_increment / x->slope;
        } else {
            // Falling phase (decay)
            x->phase += phase_increment / (1.0 - x->slope);
        }
        
        if (x->phase >= 1.0) {
            x->phase -= 1.0;
        }
    } else {
        // Envelope modes (AD or AR)
        if (x->envelope_stage == STAGE_ATTACK) {
            x->phase += phase_increment / x->slope;
            if (x->phase >= 1.0) {
                x->phase = 1.0;
                x->envelope_stage = STAGE_DECAY;
            }
        } else if (x->envelope_stage == STAGE_DECAY) {
            if (x->ramp_mode == MODE_AR && x->gate_high) {
                // Hold at sustain
                x->phase = 1.0;
            } else {
                // Decay/Release
                x->phase -= phase_increment / (1.0 - x->slope);
                if (x->phase <= 0.0) {
                    x->phase = 0.0;
                    x->envelope_stage = STAGE_IDLE;
                }
            }
        }
    }
}

//--------------------------------------------------------------------------
// Shape interpolation and transformation

double tide_apply_shape(t_tide *x, double phase, double shape_param) {
    // Determine if we're in attack or decay section
    int rising = (phase < x->slope);
    double normalized_phase;
    
    if (rising) {
        normalized_phase = phase / x->slope;
    } else {
        normalized_phase = (phase - x->slope) / (1.0 - x->slope);
    }
    
    // Clamp normalized phase
    normalized_phase = CLAMP(normalized_phase, 0.0, 1.0);
    
    // Shape parameter selects between the 5 shapes
    double shape_index = shape_param * (NUM_SHAPES - 1);
    int shape_a = (int)shape_index;
    int shape_b = shape_a + 1;
    double shape_mix = shape_index - shape_a;
    
    if (shape_b >= NUM_SHAPES) shape_b = NUM_SHAPES - 1;
    
    // Table lookup with interpolation
    double table_pos = normalized_phase * (SHAPE_TABLE_SIZE - 1);
    int index = (int)table_pos;
    double fract = table_pos - index;
    
    if (index >= SHAPE_TABLE_SIZE - 1) {
        index = SHAPE_TABLE_SIZE - 1;
        fract = 0.0;
    }
    
    // Interpolate within each table
    double val_a = x->shape_lut[shape_a][index] * (1.0 - fract) + 
                   x->shape_lut[shape_a][index + 1] * fract;
    double val_b = x->shape_lut[shape_b][index] * (1.0 - fract) + 
                   x->shape_lut[shape_b][index + 1] * fract;
    
    // Mix between shapes
    double result = val_a * (1.0 - shape_mix) + val_b * shape_mix;
    
    // Apply to attack or decay and convert to bipolar
    if (rising) {
        return result * 2.0 - 1.0;  // 0-1 -> -1 to +1 for attack
    } else {
        return (1.0 - result) * 2.0 - 1.0;  // Inverted for decay
    }
}

//--------------------------------------------------------------------------
// Smoothness processing: 2-pole LPF or wavefolder

double tide_apply_smoothness(t_tide *x, double input, double smooth_param) {
    if (smooth_param < 0.5) {
        // Low-pass filter mode (0.0 to 0.5)
        double cutoff_norm = smooth_param * 2.0;  // 0 to 1
        
        if (cutoff_norm < 0.01) {
            // Bypass filter for very low values
            return input;
        }
        
        double cutoff_hz = 20.0 * pow(1000.0, cutoff_norm);  // 20Hz to 20kHz
        
        // Simple 2-pole Butterworth LPF
        double omega = 2.0 * PI * cutoff_hz * x->sr_recip;
        if (omega > PI) omega = PI;  // Nyquist limit
        
        double cos_omega = cos(omega);
        double alpha = sin(omega) / sqrt(2.0);
        
        double b0 = (1.0 - cos_omega) / 2.0;
        double b1 = 1.0 - cos_omega;
        double b2 = b0;
        double a0 = 1.0 + alpha;
        double a1 = -2.0 * cos_omega;
        double a2 = 1.0 - alpha;
        
        // Direct Form II
        double w = input - (a1/a0) * x->lpf_z1 - (a2/a0) * x->lpf_z2;
        double output = (b0/a0) * w + (b1/a0) * x->lpf_z1 + (b2/a0) * x->lpf_z2;
        
        x->lpf_z2 = x->lpf_z1;
        x->lpf_z1 = w;
        
        return output;
        
    } else {
        // Wavefolder mode (0.5 to 1.0)
        double fold_amount = (smooth_param - 0.5) * 2.0;  // 0 to 1
        double gain = 1.0 + fold_amount * 4.0;  // 1 to 5
        
        double folded = input * gain;
        
        // Folding algorithm - ensures output stays within -1 to +1
        while (folded > 1.0 || folded < -1.0) {
            if (folded > 1.0) {
                folded = 2.0 - folded;
            } else if (folded < -1.0) {
                folded = -2.0 - folded;
            }
        }
        
        return folded;
    }
}