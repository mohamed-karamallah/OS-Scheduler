# PCM Signal Processing MATLAB Project
Communications 1 (EECS306) - Spring 2025
Cairo University Faculty of Engineering

## Project Description
This project implements a PCM (Pulse Code Modulation) signal processing system, including:
- Signal sampling at 20% above Nyquist rate
- Uniform quantization with 8 levels
- Bipolar NRZ line coding
- Bit rate calculation
- TDM multiplexing of 5 signals

## Requirements
- MATLAB 2007B or later
- Signal Processing Toolbox

## How to Run
1. Open MATLAB
2. Navigate to the directory containing `pcm_project.m`
3. Run the script by typing `pcm_project` in the MATLAB command window

## Output
The script will generate four figures:
1. Analog, sampled, and quantized signals
2. Recovered signal after low-pass filtering
3. PCM binary sequence and bipolar NRZ line coded signal
4. TDM multiplexed signal for 5 channels

The console will display:
- Simulated bit rate (based on PCM sequence length)
- Theoretical bit rate
- TDM bit rate for 5 multiplexed channels

## Explanation of Line Code Choice
Bipolar NRZ (Non-Return to Zero) was chosen because:
1. It has better power efficiency than Unipolar NRZ (requires less signal power)
2. It has no DC component, which is beneficial for transmission
3. It provides good error detection capability
4. It has a more efficient bandwidth utilization compared to other line codes
5. Implementation complexity is reasonable

