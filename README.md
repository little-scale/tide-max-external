# tide~ - Mutable Instruments Tides LFO/Envelope External

A Max/MSP external inspired by Mutable Instruments Tides 2, implementing asymmetric ramp generation with authentic waveshaping and envelope capabilities. This external demonstrates advanced C++/C integration patterns for Max SDK development.

## Features

- **Asymmetric Ramp Generator**: Variable slope control for complex waveforms
- **Shape Morphing**: Exponential, linear, and logarithmic curve types  
- **Dual Smoothness**: Low-pass filtering (< 0.5) and triangle wavefolding (> 0.5)
- **Three Ramp Modes**: AD (Attack-Decay), Loop (LFO), AR (Attack-Release)
- **Signal/Float Dual Inlets**: All parameters accept both control and audio rate modulation
- **Universal Binary**: Compatible with Intel and Apple Silicon Macs
- **Self-Contained**: No external dependencies, simplified Tides recreation

## Inlets

1. **Frequency** (Hz) - Oscillation/envelope frequency (0.001 - 1000 Hz)
2. **Trigger/Gate** - Trigger envelopes or gate for AR mode
3. **Shape** (0-1) - Curve morphing: 0=exponential, 0.5=linear, 1=logarithmic
4. **Slope** (0-1) - Attack/decay ratio: 0=fast attack, 1=fast decay
5. **Smooth** (0-1) - Processing: 0-0.5=filtering, 0.5-1=wavefolding

## Outlets

1. **Waveform** (signal) - Generated output

## Attributes

- `@mode` - Ramp mode: 0=AD, 1=Loop (default), 2=AR

## Usage Examples

```
tide~ @mode 1        // LFO mode (continuous looping)
tide~ @mode 0        // AD envelope (one-shot)
tide~ @mode 2        // AR envelope (gate-controlled)
```

## Technical Implementation

- **Architecture**: C wrapper around C++ DSP core using `extern "C"` pattern
- **Algorithm**: Simplified recreation of Tides 2 PolySlopeGenerator
- **Build**: Universal binary (x86_64 + ARM64) with CMake
- **Dependencies**: None (self-contained implementation)
- **Integration**: Demonstrates C++/Max external integration best practices
- **Memory Management**: Opaque pointer pattern for safe C++ object lifecycle

## Build Instructions

```bash
cd source/audio/tide~
mkdir build && cd build
cmake -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..
cmake --build .

# For M1/M2 Macs, codesign the external
codesign --force --deep -s - ../../../externals/tide~.mxo
```

### Build Requirements

- CMake 3.19 or later
- Xcode command line tools (macOS)
- Max SDK base (included in repository)

### Verification

```bash
# Check universal binary
file ../../../externals/tide~.mxo/Contents/MacOS/tide~

# Should output: 
# Mach-O universal binary with 2 architectures: [x86_64] [arm64]
```

## Files

- `tide~.c` - Main Max external implementation
- `tides_wrapper.cpp` - C++ DSP algorithm wrapper
- `CMakeLists.txt` - Build configuration
- `README.md` - This documentation

## Usage Examples

### Basic LFO
```
[phasor~ 0.1]    // Set frequency to 0.1 Hz
|
[tide~ @mode 1]  // Loop mode LFO
|
[*~ 0.3]         // Scale output
|
[dac~]
```

### Envelope Generator
```
[button]         // Manual trigger
|
[tide~ @mode 0]  // AD mode envelope
|
[*~]             // Apply to audio signal
|  \
|   [noise~]     // Audio source
|
[dac~]
```

### Shape Modulation
```
[phasor~ 0.05]   // Slow modulation
|
[tide~]          // Modulation LFO
|
[scale 0. 1. 0. 1.]  // Scale to shape range
|
[send shape]
|
[receive shape]
|
[tide~ @mode 1]  // Main LFO with modulated shape
|
[dac~]
```

## Parameter Details

### Shape Parameter (0-1)
- **0.0-0.4**: Exponential curves (fast start, slow end)
- **0.5**: Linear ramp (triangle wave in Loop mode)
- **0.6-1.0**: Logarithmic curves (slow start, fast end)

### Slope Parameter (0-1)  
- **0.0**: Fast attack, slow decay (sawtooth-like)
- **0.5**: Balanced attack/decay (triangle wave)
- **1.0**: Slow attack, fast decay (reverse sawtooth)

### Smooth Parameter (0-1)
- **0.0-0.5**: Low-pass filtering (increasingly smooth)
- **0.5**: No processing (clean signal)
- **0.5-1.0**: Triangle wave folding (increasingly complex)

## Development Notes

This implementation demonstrates several advanced Max SDK patterns:

1. **C++ Integration**: Clean separation between Max C interface and C++ DSP core
2. **Opaque Pointers**: Safe C++ object management in C structures
3. **Algorithm Recreation**: Simplified implementation capturing essential behavior
4. **Universal Binary**: Cross-architecture compatibility with CMake
5. **Signal/Float Handling**: Proper dual-inlet parameter control

For detailed development information, see `CLAUDE.md` in this directory.

## Compatibility

- **Max/MSP**: Version 8.0 or later
- **macOS**: 10.14 or later (universal binary includes Intel and Apple Silicon)
- **Windows**: Not currently supported (would require additional build configuration)

## See Also

- **Original Hardware**: Mutable Instruments Tides 2 module
- **Max SDK**: Documentation for mixed C/C++ externals
- **Related Externals**: `slewenv~` for envelope generation patterns
- **Help File**: `tide~.maxhelp` for interactive examples