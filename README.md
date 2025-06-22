# tide~ - Mutable Instruments Tides LFO/Envelope External

A Max/MSP external inspired by Mutable Instruments Tides 2, implementing asymmetric ramp generation with authentic waveshaping and envelope capabilities. This external demonstrates advanced C++/C integration patterns for Max SDK development.

## Features

- **Asymmetric Ramp Generator**: Variable slope control for complex waveforms
- **Shape Morphing**: Exponential, linear, and logarithmic curve types  
- **Dual Smoothness**: Low-pass filtering (< 0.5) and triangle wavefolding (> 0.5)
- **Sub-Hz Frequencies**: Support down to 0.000001 Hz (11.5 days per cycle) with double precision
- **Phase Synchronization**: Bang input for instant phase reset and LFO sync
- **Phase Offset**: 5th inlet for quadrature relationships and stereo effects
- **Frequency Scaling**: `@freqscale` attribute for ultra-slow modulation rates
- **Signal/Float Dual Inlets**: All parameters accept both control and audio rate modulation
- **Universal Binary**: Compatible with Intel and Apple Silicon Macs
- **Self-Contained**: No external dependencies, simplified Tides recreation

## Inlets

1. **Frequency/Reset** (signal/float/bang) - Oscillation frequency (0.000001 - 1000 Hz) or bang to reset phase
2. **Shape** (signal/float, 0-1) - Curve morphing: 0=exponential, 0.5=linear, 1=logarithmic
3. **Slope** (signal/float, 0-1) - Attack/decay ratio: 0=fast attack, 1=fast decay
4. **Smooth** (signal/float, 0-1) - Processing: 0-0.5=filtering, 0.5-1=wavefolding
5. **Phase** (signal/float, 0-1) - Phase offset for quadrature and stereo effects

## Outlets

1. **Waveform** (signal) - Generated output (-1 to +1)

## Attributes

- `@freqscale` (float, 0.0001-1.0) - Frequency scaling factor for ultra-slow rates (default: 1.0)

## Usage Examples

### Basic LFO with Phase Reset
```
[0.5]            // 0.5 Hz frequency
|
[tide~]          // Basic LFO
|
[*~ 0.3]         // Scale output
|
[dac~]

[bang]           // Phase reset
|
[tide~]          // Synchronized LFO
```

### Ultra-Low Frequency Generation
```
[0.1]                    // Base frequency 0.1 Hz
|
[tide~ @freqscale 0.001] // Results in 0.0001 Hz (2.78 hours per cycle)
|
[scope~]                 // Visualize slow movement
```

### Synchronized Multiple LFOs
```
[bang]     // Master sync
|
[t b b b]  // Distribute to multiple LFOs
|  |  |
|  |  [0.25]    // 90° phase shift
|  |  |
|  |  [tide~ 1] // Quadrature LFO
|  |
|  [tide~ 1]    // Main LFO  
|
[metro 4000]    // Reset every 4 seconds
```

### Stereo Phase Effects
```
[phasor~ 0.1]   // Frequency control
|
[tide~]         // Left channel LFO
|
[*~ source~]    // Apply to audio
|
[dac~ 1]

[0.25]          // 90° phase offset
|
[tide~ 0.1]     // Right channel LFO (quadrature)
|
[*~ source~]    // Apply to audio  
|
[dac~ 2]
```

### Long-Duration Automation
```
[0.000001]      // 1 micro-Hz = ~11.5 days per cycle
|
[tide~]         // Extremely slow automation
|
[scale -1. 1. 0. 1.]  // Convert to 0-1 range
|
[pack 0. 200]   // Control cutoff over days
|
[line~]
|
[lores~ source~ 0.7]
```

### Complex Modulation Network
```
[phasor~ 0.01]      // Master clock (100 second cycle)
|
[tide~]             // Master LFO
|
[scale -1. 1. 0. 1.] // Shape modulation
|
[pack f f]
|   |
|   [r master-freq]  // Frequency from elsewhere
|
[tide~]             // Modulated LFO
|
[*~ audio-source~]
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
- **0.0-0.1**: Clean signal (minimal processing)
- **0.1-0.5**: Low-pass filtering (increasingly smooth)
- **0.5-1.0**: Triangle wave folding (increasingly complex harmonics)

### Phase Parameter (0-1)
- **0.0**: No phase offset (0°)
- **0.25**: Quadrature offset (90°)
- **0.5**: Inverted phase (180°)
- **0.75**: Reverse quadrature (270°)

### Frequency Scaling (@freqscale)
- **1.0**: Normal operation (default)
- **0.1**: 10x slower frequencies
- **0.01**: 100x slower frequencies  
- **0.001**: 1000x slower frequencies
- **0.0001**: 10,000x slower frequencies

## Advanced Applications

### Live Performance Sync
```
// MIDI-triggered phrase sync
[notein]
|
[select 60]  // C4 triggers reset
|
[bang]
|
[t b b b]    // Reset multiple LFOs simultaneously
```

### Polyrhythmic Patterns
```
[metro 1000]     // 1 second intervals
|
[counter 0 7]    // 8-step pattern
|
[select 0 4]     // Reset on beats 1 and 5
|
[bang]
|
[tide~ 0.125]    // 8-second cycle, reset every 4 beats
```

### Geological Time Scales
```
[0.000000001]    // Nano-Hz frequencies
|
[tide~ @freqscale 0.0001]  // 316 years per cycle
|
[scope~]         // Visualize ultra-slow drift
```

## Technical Implementation

- **Architecture**: C wrapper around C++ DSP core using `extern "C"` pattern
- **Algorithm**: Simplified recreation of Tides 2 PolySlopeGenerator
- **Precision**: Double precision phase accumulation for sub-Hz frequencies
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

- `tide~.c` - Main Max external implementation with bang sync
- `tides_wrapper.cpp` - C++ DSP algorithm wrapper with double precision
- `CMakeLists.txt` - Build configuration
- `README.md` - This documentation
- `CLAUDE.md` - Complete development history and patterns

## Development Patterns Demonstrated

This implementation showcases several advanced Max SDK patterns:

1. **C++ Integration**: Clean separation between Max C interface and C++ DSP core
2. **Opaque Pointers**: Safe C++ object management in C structures
3. **Double Precision**: Sub-Hz frequency support with phase accumulation
4. **Bang Message Handling**: Phase synchronization via message system
5. **Attribute System**: Frequency scaling attribute implementation
6. **Signal/Float Dual Inlets**: Comprehensive parameter control patterns
7. **Universal Binary**: Cross-architecture compatibility with CMake
8. **Algorithm Recreation**: Essential behavior capture without dependencies

## Synchronization Applications

- **Multiple LFO Sync**: Phase-lock multiple tide~ objects for complex modulation
- **Musical Timing**: Reset LFOs at musical boundaries (downbeats, phrase changes)
- **Live Performance**: Manual phase control for rhythmic manipulation
- **Sequencer Integration**: Time-locked automation with external sequencers
- **Polyrhythmic Composition**: Complex interlocking pattern generation
- **Long-Form Works**: Geological time scale evolution with sync points

## Compatibility

- **Max/MSP**: Version 8.0 or later
- **macOS**: 10.14 or later (universal binary includes Intel and Apple Silicon)
- **Windows**: Not currently supported (would require additional build configuration)

## See Also

- **Original Hardware**: Mutable Instruments Tides 2 module
- **Max SDK**: Documentation for mixed C/C++ externals
- **Related Externals**: `slewenv~` for envelope generation patterns
- **Help File**: `tide~.maxhelp` for interactive examples
- **Development Notes**: `CLAUDE.md` for complete technical implementation details