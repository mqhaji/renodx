# MGSV general-gameplay light-flicker comparison

The historical state-profile and cold-start A/B investigation is retired; its capture evidence remains in the ignored
analysis archive. This page is retained only to redirect existing references, not to request another comparison.

- FSR Legacy Compute State and its compute-only branch are removed after successful user validation with Legacy Off.
- DLSS now activates lazily on selection; the old blanket runtime-isolation builds are not current behavior.
- The later `0x200DBED9` light-VS workaround is removed. The original light VS and guarded native paths remain.
- Character warble and rapid night-vision skinned-mesh flicker are not declared fixed by those changes.

See [current implementation and limitations](README.md) and [outstanding investigation work](ROADMAP.md).