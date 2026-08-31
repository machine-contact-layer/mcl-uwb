# MCL-UWB

`mcl-uwb` defines an optional Ultra-Wideband binding for the Machine Contact Layer.

UWB is especially relevant when precise ranging or spatially constrained communication is available, but it remains a transport binding rather than the definition of MCL.

## Scope

- MCL Link frame carriage over UWB-capable systems
- capability negotiation
- integration of ranging/spatial measurements as transport metadata
- session/context preservation across handoff
- conformance vectors

## Non-goals

MCL-UWB does not define UWB radio hardware, ranging algorithms, regulatory limits, or MCL semantic meaning.

## Status

Private research repository. Pre-v0.1. See [`spec/binding-v0.md`](spec/binding-v0.md).
