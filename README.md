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


## Implementation status

The reference implementation is present, freestanding C99, with no allocation
and no global mutable state. It contains **no UWB driver**: how bytes reach the
medium is the integrator's decision. A binding describes a mapping; it does not
become a UWB driver.

Verified: builds under `/W4 /WX`, and the compiled object references no libc
symbol (no `memcpy`, `memset`, `malloc`, or stdio), so it links on a
freestanding target.

- `include/mcl/uwb_binding.h` — public API
- `src/uwb_binding.c` — implementation
- `tests/test_uwb_binding.c` — round trips and the negative cases

**Status: Research Draft.** Nothing here is frozen. Assigned transport id
`0x04` is provisional until Candidate Specification maturity.

## Status

Private research repository. Pre-v0.1. See [`spec/binding-v0.md`](spec/binding-v0.md).
