# Noise

[fast_noise_gr8.h](fast_noise_gr8.h) contains a 128x128 IS-FAST noise texture with 32 layers and two 8-bit channels. Lighting samples it on the reduced-resolution pixel grid, advancing layers with frames and slices. Reconstruction Off keeps the frame index fixed.

[EA FAST](https://github.com/electronicarts/fastnoise) uses BSD-3-Clause. Its notice is included in the [header](fast_noise_gr8.h) and the addon's ReShade license panel. The exact upstream file for this texture has not been identified, so the license's applicability to these particular bytes remains unverified.
