# tide~ Project CLAUDE.md

## Project Completion Summary

**Status**: ✅ **COMPLETED** - Full implementation with documentation and testing

### Key Achievements
- **Complete Tides Algorithm Implementation**: Asymmetric ramp generator with variable slope control
- **Advanced Waveshaping**: 5 morphable shapes with smooth interpolation (linear, exponential, logarithmic, sine, arc-sine)
- **Dual Smoothness Processing**: 2-pole Butterworth LPF (0.0-0.5) + triangle wavefolder (0.5-1.0)
- **Three Ramp Modes**: AD (one-shot), Loop (continuous), AR (attack-release)
- **Signal/Float Dual Support**: All 3 inlets accept both signal and float inputs
- **Universal Binary**: Built for Intel + Apple Silicon architectures
- **Comprehensive Documentation**: README, help patch, and code comments

### Technical Discoveries
1. **Advanced Lookup Table Interpolation**: Smooth morphing between 5 different curve shapes using dual-table interpolation
2. **Triangle Wavefolder Algorithm**: Proper folding technique that maintains ±1 output range
3. **Inlet Configuration Design**: Frequency on inlet 0 for LFO objects (more intuitive than trigger)
4. **Asymmetric Phase Accumulator**: Variable slope control for attack/decay ratio without aliasing

### Implementation Highlights
- **Shape Interpolation**: `shape_param * (NUM_SHAPES - 1)` with fractional interpolation
- **Smoothness Mapping**: Clean 0.5 split between filter and wavefolder modes
- **Parameter Safety**: All inputs clamped, division-by-zero prevention, Nyquist limiting
- **Connection Detection**: Proper signal vs float handling using `count[]` array
- **Bipolar Output**: Correct -1 to +1 range for modulation applications

### Files Created/Modified
- `tide~.c` (568 lines) - Complete implementation
- `CMakeLists.txt` - Universal binary build configuration
- `README.md` - Comprehensive documentation with examples
- `tide~.maxhelp` - Interactive help patch
- Built `tide~.mxo` - Universal binary external

### Build Verification
```bash
cd source/audio/tide~/build
cmake -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..
cmake --build .
file ../../../externals/tide~.mxo/Contents/MacOS/tide~
# Result: Mach-O universal binary with 2 architectures
```

### User Feedback Integration
1. **Inlet 0 Frequency Control**: Changed from trigger to frequency (more intuitive)
2. **Inlet Count Fix**: Removed unnecessary proxy inlets (3 inlets total)
3. **Parameter Verification**: Confirmed shape and smoothness behaviors are correct

### Technical Patterns Established
- **Waveshaping Lookup Tables**: 1024-point tables with linear interpolation
- **Butterworth Filter Design**: Proper coefficient calculation with frequency mapping
- **Parameter Responsiveness**: Mathematical enhancement for subtle effects
- **Real-time Safety**: No allocations, pure computation in perform routine

---

**Next Session**: This project is complete and ready for production use. The external demonstrates advanced DSP techniques and serves as an excellent reference for future waveshaping implementations.