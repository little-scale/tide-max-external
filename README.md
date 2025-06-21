# tide~ - Tides-inspired LFO External

A Max/MSP external inspired by the core waveshaping algorithms from Mutable Instruments Tides 2, reimplemented as a single LFO output with unique asymmetric ramps, shape controls, and smoothness processing.

## Credits and Inspiration

**Inspired by**: [Mutable Instruments Tides](https://mutable-instruments.net/modules/tides/) by Émilie Gillet  
**Original Design**: © Mutable Instruments 2013-2020  
**This Implementation**: Educational/research purposes - original algorithmic interpretation  
**No Direct Code**: This is a clean-room implementation based on published specifications

## Features

### Core Waveshaping
- **Asymmetric ramp generator** with variable slope (attack/decay ratio)
- **5 morphable shapes**: linear, exponential, logarithmic, sine, arc-sine
- **Smoothness processing**: 2-pole low-pass filter or wavefolder
- **Three ramp modes**: AD (one-shot), Loop (continuous), AR (attack-release)

### Signal Processing
- **Frequency range**: 0.001 to 100 Hz
- **Bipolar output**: -1 to +1 range for musical modulation
- **Sample-accurate triggering**: Edge detection for envelope modes
- **Universal binary**: Compatible with Intel and Apple Silicon Macs

## Inlets

1. **Frequency** (signal/float) - LFO frequency in Hz (0.001-100)
2. **Shape** (signal/float) - 0-1 morphs between wave shapes  
3. **Smoothness** (signal/float) - 0-1 controls filter/folder amount

**Note**: All inlets accept both signal and float inputs. When a signal is connected, it takes priority over float messages. When no signal is connected, float messages control the parameter.

## Outlet

1. **LFO Output** (signal) - Shaped and smoothed ramp signal

## Parameters

### Message Control
- `frequency <float>` - LFO frequency in Hz (0.001-100.0, default: 1.0)
- `slope <float>` - Attack/decay ratio 0-1 (0=fast attack, 1=fast decay, default: 0.5)
- `mode <int>` - Ramp mode: 0=AD, 1=Loop, 2=AR (default: 1=Loop)
- `trigger` or `bang` - Manually trigger envelope in AD/AR modes

### Arguments
Object can be created with optional arguments: `tide~ [frequency] [slope] [mode]`

Examples:
- `tide~` - Default: 1Hz, symmetric, loop mode
- `tide~ 2.0` - 2Hz frequency
- `tide~ 1.5 0.3` - 1.5Hz with fast attack/slow decay
- `tide~ 0.5 0.7 0` - 0.5Hz with slow attack/fast decay in AD mode

## Shape Parameter (Inlet 2)

Morphs between 5 different curve shapes:
- **0.0** - Linear ramp
- **0.25** - Exponential (fast start, slow end)
- **0.5** - Logarithmic (slow start, fast end)  
- **0.75** - Sine curve (smooth S-curve)
- **1.0** - Arc-sine (inverse S-curve)

Values between these points smoothly interpolate between curves.

## Smoothness Parameter (Inlet 3)

Controls post-processing of the shaped ramp:
- **0.0-0.5** - 2-pole low-pass filter (0.0=bypass, 0.5=20kHz cutoff)
- **0.5-1.0** - Wavefolder (0.5=bypass, 1.0=maximum folding)

## Ramp Modes

### Loop Mode (1) - Default
Continuous looping ramp with asymmetric attack/decay based on slope parameter.

### AD Mode (0) - One-shot
Triggered envelope that runs attack then decay once per trigger.
- Rising edge on inlet 1 starts envelope
- Envelope runs to completion regardless of gate state

### AR Mode (2) - Attack-Release  
Gate-controlled envelope with sustain.
- Rising edge starts attack phase
- High gate sustains at peak
- Falling edge starts release phase

## Implementation Notes

### Algorithm Details
- **Phase accumulator**: Asymmetric timing based on slope parameter
- **Shape lookup tables**: 1024-point tables for each curve type
- **Smoothness filter**: Butterworth 2-pole design with frequency mapping
- **Wavefolder**: Triangle-wave folding algorithm with gain control

### Performance
- **Real-time safe**: No memory allocations in audio thread
- **Optimized**: Efficient table lookups and parameter caching
- **Thread safe**: Pure computational processing in perform routine

## Usage Examples

### Basic LFO
```
tide~ 1.0 0.5 1  // 1Hz symmetric loop
```

### Envelope Generator
```
tide~ 2.0 0.2 0  // Fast 2Hz attack-decay envelope
```

### Float Message Control
```
// Send float messages to control parameters
[2.0( -> [tide~]  // Frequency 2Hz via inlet 1
[0.5( -> [tide~]  // Shape parameter via inlet 2
[0.3( -> [tide~]  // Smoothness parameter via inlet 3
```

### Modulated Waveshaper
```
// Connect shape and smoothness modulation
phasor~ 0.1 -> tide~ 1.0 0.3 1
```

### Triggered Envelope
```
// Manual triggering for envelopes
[trigger( -> [tide~ 2.0 0.3 0]  // AD mode
[bang( -> [tide~ 1.5 0.6 2]     // AR mode
```

## Technical Specifications

- **Sample rate**: Any supported by Max/MSP
- **Bit depth**: 64-bit double precision processing
- **Latency**: Zero latency
- **CPU usage**: Optimized for real-time performance
- **Architecture**: Universal binary (Intel + Apple Silicon)

---

## Development Credits

**Algorithm Research**: Based on Mutable Instruments (Émilie Gillet) Tides specifications and community documentation  
**Implementation**: Clean-room development using Max SDK patterns  
**AI Assistant**: [Claude Code](https://claude.ai/code) for implementation and documentation  
**Max SDK**: [Cycling '74 Max SDK](https://github.com/Cycling74/max-sdk) framework  

*Captures the essence of Tides' unique character through original algorithmic interpretation!*