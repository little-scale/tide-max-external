/**
 * C++ wrapper for Tides PolySlopeGenerator
 * Provides C interface for Max external
 */

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

// Include Max headers for post() function
extern "C" {
#include "ext.h"
}

// Create basic stmlib dependencies that Tides needs
namespace stmlib {
    typedef unsigned char GateFlags;
    
    // Basic DSP functions that Tides expects
    inline void ParameterInterpolate(const float* src, float* dst, float coefficient, size_t size) {
        while (size--) {
            *dst += (*src - *dst) * coefficient;
            src++;
            dst++;
        }
    }
    
    class HysteresisQuantizer {
    public:
        HysteresisQuantizer() : previous_value_(0) { }
        
        int Process(float value, float hysteresis) {
            int quantized = static_cast<int>(value + 0.5f);
            if (abs(quantized - previous_value_) <= hysteresis) {
                quantized = previous_value_;
            }
            previous_value_ = quantized;
            return quantized;
        }
        
    private:
        int previous_value_;
    };
}

// Include Tides headers after stmlib setup
#define TIDES_POLY_SLOPE_GENERATOR_H_
namespace tides {

// Reproduce necessary Tides enums and structures
enum RampMode {
    RAMP_MODE_AD,
    RAMP_MODE_LOOPING,
    RAMP_MODE_AR,
    RAMP_MODE_LAST
};

enum OutputMode {
    OUTPUT_MODE_GATES,
    OUTPUT_MODE_AMPLITUDE,
    OUTPUT_MODE_SLOPE_PHASE,
    OUTPUT_MODE_FREQUENCY,
    OUTPUT_MODE_LAST,
};

enum Range {
    RANGE_CONTROL,
    RANGE_AUDIO,
    RANGE_LAST
};

// Simplified PolySlopeGenerator implementation
// This is a minimal version that captures the core Tides algorithm
class PolySlopeGenerator {
public:
    enum { num_channels = 4 };
    
    struct OutputSample {
        float channel[num_channels];
    };

    PolySlopeGenerator() {
        Init();
    }
    
    ~PolySlopeGenerator() { }
    
    void Init() {
        frequency_ = 0.01f;
        pw_ = 0.5f;
        shift_ = 0.0f;
        shape_ = 0.0f;
        fold_ = 0.0f;
        
        // Initialize ramp generator state
        phase_ = 0.0f;
        ramp_value_ = 0.0f;
        rising_ = true;
        
        // Initialize filter state
        filter_lp_1_ = 0.0f;
        filter_lp_2_ = 0.0f;
    }
    
    void Render(
        RampMode ramp_mode,
        OutputMode output_mode,
        Range range,
        float frequency,
        float pw,
        float shape,
        float smoothness,
        float shift,
        const stmlib::GateFlags* gate_flags,
        const float* ramp,
        OutputSample* out,
        size_t size) {
        
        // Store parameters
        frequency_ = std::max(frequency, 0.0f);  // Allow zero frequency
        pw_ = std::max(0.001f, std::min(0.999f, pw));
        shape_ = std::max(0.0f, std::min(1.0f, shape));
        shift_ = std::max(0.0f, std::min(1.0f, shift));  // Phase offset parameter
        
        // Debug parameters once
        static int debug_count = 0;
        debug_count++;
        if (debug_count == 100) {  // After 100 calls
            post("Tides params: freq=%.9f, pw=%.3f, shape=%.3f", frequency_, pw_, shape_);
        }
        
        
        for (size_t i = 0; i < size; i++) {
            // Handle gate logic for different modes
            bool gate_high = gate_flags && (*gate_flags & 0x02);
            bool gate_rising = gate_flags && (*gate_flags & 0x01);
            
            if (ramp_mode == RAMP_MODE_AD) {
                if (gate_rising) {
                    phase_ = 0.0f;
                    rising_ = true;
                }
            } else if (ramp_mode == RAMP_MODE_AR) {
                if (gate_rising) {
                    phase_ = 0.0f;
                    rising_ = true;
                } else if (!gate_high && rising_) {
                    rising_ = false;
                }
            }
            
            // Generate ramp
            float ramp_output = GenerateRamp(ramp_mode, frequency_, shift_);
            
            // Apply shaping
            float shaped = ApplyShaping(ramp_output, shape_, pw_);
            
            // Apply smoothing (filtering or folding)
            float final_output = ApplySmoothing(shaped, smoothness);
            
            // Fill output channels (all same for now)
            out[i].channel[0] = final_output;
            out[i].channel[1] = final_output;
            out[i].channel[2] = final_output;
            out[i].channel[3] = final_output;
        }
    }

private:
    float frequency_;
    float pw_;
    float shift_;
    float shape_;
    float fold_;
    
    // Ramp generator state
    double phase_;  // Use double for better precision
    float ramp_value_;
    bool rising_;
    
    // Filter state
    float filter_lp_1_;
    float filter_lp_2_;
    
    // Track rising/falling phase for shaping
    bool in_rising_phase_;
    
    float GenerateRamp(RampMode mode, float frequency, float phase_shift = 0.0f) {
        phase_ += (double)frequency;  // Cast to double for accumulation
        
        
        if (mode == RAMP_MODE_LOOPING) {
            // Wrap phase
            while (phase_ >= 1.0) {  // Use 1.0 (double literal)
                phase_ -= 1.0;
            }
            
            // Apply phase offset - convert to float for calculations
            float float_phase = (float)phase_;  // Convert to float
            float effective_phase = fmodf(float_phase + phase_shift, 1.0f);
            
            // Track which portion we're in
            in_rising_phase_ = (effective_phase < pw_);
            
            // Generate asymmetric ramp using pw (slope) parameter
            if (in_rising_phase_) {
                // Rising portion: 0 to 1 over pw fraction of cycle
                ramp_value_ = effective_phase / pw_;
            } else {
                // Falling portion: 1 to 0 over (1-pw) fraction of cycle
                ramp_value_ = 1.0f - (effective_phase - pw_) / (1.0f - pw_);
            }
            
            // Convert to bipolar output (-1 to +1) for audio
            ramp_value_ = ramp_value_ * 2.0f - 1.0f;
        } else if (mode == RAMP_MODE_AD) {
            if (rising_ && phase_ < pw_) {
                // Attack phase
                ramp_value_ = phase_ / pw_;
            } else if (rising_ && phase_ >= pw_) {
                rising_ = false;
                ramp_value_ = 1.0f - (phase_ - pw_) / (1.0f - pw_);
            } else if (!rising_) {
                // Decay phase
                ramp_value_ = std::max(0.0f, 1.0f - (float)(phase_ - pw_) / (1.0f - pw_));
            }
        } else if (mode == RAMP_MODE_AR) {
            if (rising_ && phase_ < pw_) {
                // Attack phase
                ramp_value_ = phase_ / pw_;
            } else if (rising_ && phase_ >= pw_) {
                ramp_value_ = 1.0f;
            } else if (!rising_) {
                // Release phase  
                ramp_value_ = std::max(0.0f, ramp_value_ - (float)frequency / (1.0f - pw_));
            }
        }
        
        return ramp_value_;  // Return bipolar output (-1 to +1)
    }
    
    float ApplyShaping(float input, float shape, float slope) {
        // Convert bipolar input back to unipolar for shaping
        float unipolar = (input + 1.0f) * 0.5f;
        float shaped;
        
        if (shape < 0.1f) {
            shaped = unipolar;  // Linear
        } else if (shape < 0.5f) {
            // Exponential curves
            float curve = (shape - 0.1f) / 0.4f;
            if (in_rising_phase_) {
                shaped = powf(unipolar, 1.0f + curve * 2.0f);
            } else {
                // Invert the curve for falling phase
                shaped = 1.0f - powf(1.0f - unipolar, 1.0f + curve * 2.0f);
            }
        } else if (shape > 0.5f) {
            // Logarithmic curves
            float curve = (shape - 0.5f) * 2.0f;
            if (in_rising_phase_) {
                shaped = 1.0f - powf(1.0f - unipolar, 1.0f + curve * 2.0f);
            } else {
                // Invert the curve for falling phase
                shaped = powf(unipolar, 1.0f + curve * 2.0f);
            }
        } else {
            shaped = unipolar;  // Linear at exactly 0.5
        }
        
        // Convert back to bipolar
        return shaped * 2.0f - 1.0f;
    }
    
    float ApplySmoothing(float input, float smoothness) {
        if (smoothness < 0.1f) {
            // Near 0: no processing (our default should be clean)
            return input;
        } else if (smoothness < 0.5f) {
            // Low-pass filtering for smoothness 0.1 to 0.5
            float cutoff = (smoothness - 0.1f) / 0.4f; // 0 to 1
            cutoff = cutoff * cutoff; // Square for more control
            cutoff = std::max(cutoff, 0.01f); // Minimum cutoff to avoid silence
            
            // Simple 2-pole filter
            filter_lp_1_ += (input - filter_lp_1_) * cutoff;
            filter_lp_2_ += (filter_lp_1_ - filter_lp_2_) * cutoff;
            return filter_lp_2_;
        } else if (smoothness > 0.5f) {
            // Wave folding for smoothness > 0.5
            float fold_amount = (smoothness - 0.5f) * 2.0f;
            float folded = input * (1.0f + fold_amount * 8.0f);
            
            // Triangle folding
            while (folded > 1.0f || folded < -1.0f) {
                if (folded > 1.0f) {
                    folded = 2.0f - folded;
                } else if (folded < -1.0f) {
                    folded = -2.0f - folded;
                }
            }
            return folded;
        } else {
            // Exactly 0.5: no processing
            return input;
        }
    }
};

} // namespace tides

// C interface functions
extern "C" {

void* tides_create(void) {
    try {
        return new tides::PolySlopeGenerator();
    } catch (...) {
        return nullptr;
    }
}

void tides_destroy(void* tides_obj) {
    if (tides_obj) {
        delete static_cast<tides::PolySlopeGenerator*>(tides_obj);
    }
}

void tides_init(void* tides_obj) {
    if (tides_obj) {
        static_cast<tides::PolySlopeGenerator*>(tides_obj)->Init();
    }
}

void tides_render(void* tides_obj, int ramp_mode, int output_mode, int range,
                  float frequency, float pw, float shape, float smoothness, float shift,
                  unsigned char gate_flags, float* output) {
    
    if (!tides_obj || !output) return;
    
    tides::PolySlopeGenerator* poly = static_cast<tides::PolySlopeGenerator*>(tides_obj);
    tides::PolySlopeGenerator::OutputSample out_sample;
    
    stmlib::GateFlags flags = gate_flags;
    
    poly->Render(
        static_cast<tides::RampMode>(ramp_mode),
        static_cast<tides::OutputMode>(output_mode),
        static_cast<tides::Range>(range),
        frequency, pw, shape, smoothness, shift,
        &flags,
        nullptr,  // external ramp (not used)
        &out_sample,
        1  // size = 1 sample
    );
    
    // Copy first channel to output
    output[0] = out_sample.channel[0];
}

} // extern "C"