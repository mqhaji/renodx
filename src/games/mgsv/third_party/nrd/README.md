# NRD interfaces

This software contains source code provided by NVIDIA Corporation.

These files are copied without changes from NVIDIA NRD 4.18.0, commit `8f12a22fc90e6a3c7f5da4c9c3103633c842963a`. Only the three public C++ headers, shader packing helpers, generated shader configuration, and [upstream license](LICENSE.txt) are included. The full SDK is not a RenoDX submodule.

`NRDConfig.hlsli` records normal encoding 2 and roughness encoding 1 from the matching DLL build. Do not tune settings by editing these files. The MGSV adapter configures NRD through its public API. See the [reflection guide](../../lighting/reflections/README.md) for the runtime DLL and optional standalone SDK build.
