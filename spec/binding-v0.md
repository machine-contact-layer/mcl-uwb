# MCL-UWB Binding v0

Status: **Research Draft**

## Purpose

Carry MCL Link frames over UWB-capable systems and optionally expose precise ranging/spatial measurements to the MCL session as transport metadata.

## Candidate capabilities

A UWB binding may expose:

- ranging support
- angle/position support where available
- connected/session mode
- broadcast/discovery support where available
- payload size / update rate
- security mode

## Spatial metadata

UWB-derived range or angle data is not automatically an MCL Core fact. The binding may surface calibrated measurements to an adapter, which can then create an `OBSERVATION`, `SPATIAL_SCOPE`, or related semantic object when appropriate.

## Handoff

An MCL session established on another binding may migrate data to UWB while retaining:

- session reference
- semantic context generation
- peer claim references
- priority semantics

Handoff does not imply trust.

## Open questions

- which UWB APIs expose sufficiently portable data/control across OEMs
- how precise-ranging metadata should be normalized
- whether UWB should be preferred for specific contact classes after acoustic bootstrap
- how to preserve MCL semantics across vendor-specific ranging stacks
