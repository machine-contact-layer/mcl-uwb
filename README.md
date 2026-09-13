<p align="center">
  <img src="https://raw.githubusercontent.com/machine-contact-layer/.github/main/profile/banner.png" alt="Machine Contact Layer (MCL) banner: black and white checkerboard with the OJOBIT wordmark" width="100%">
</p>

<h1 align="center">MCL-UWB</h1>

<p align="center"><strong>The Ultra-Wideband binding: specified and unit-tested, not yet run on UWB hardware.</strong></p>

<p align="center">
  Experimental Ultra-Wideband (UWB) transport binding for the Machine Contact
  Layer (MCL): a specification and freestanding C99 reference for carrying
  machine-to-machine contact frames and ranging metadata over UWB radios.
</p>

<p align="center">
  <a href="https://github.com/machine-contact-layer/mcl-uwb/actions/workflows/ci.yml"><img alt="CI status" src="https://github.com/machine-contact-layer/mcl-uwb/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/machine-contact-layer/mcl-uwb/blob/main/LICENSE"><img alt="License: Apache-2.0" src="https://img.shields.io/badge/license-Apache--2.0-blue"></a>
  <img alt="Maturity: Research Draft" src="https://img.shields.io/badge/maturity-Research%20Draft-lightgrey">
  <img alt="Language: freestanding C99" src="https://img.shields.io/badge/C99-freestanding-informational">
</p>

<p align="center">
  <a href="https://github.com/machine-contact-layer/mcl-sdk"><b>SDK</b></a> ·
  <a href="https://github.com/machine-contact-layer/mcl-core"><b>MCL overview</b></a> ·
  <a href="spec/binding-v0.md"><b>Specification</b></a> ·
  <a href="https://github.com/machine-contact-layer/mcl-core/blob/main/REPORTING.md"><b>Report a defect</b></a>
</p>

---

UWB offers something the other bindings cannot: distance measurements that are
hard to fake. That makes it interesting for contact between machines that need
to know a peer is physically near, rather than merely reachable.

It is part of the [Machine Contact Layer](https://github.com/machine-contact-layer/mcl-core),
an open protocol for machine-to-machine discovery, contact and transport
migration.

> **Research Draft.** This binding has not been run on UWB hardware and nothing
> in it is frozen. For a transport you can build on today, use
> [mcl-ip](https://github.com/machine-contact-layer/mcl-ip) or
> [mcl-ble](https://github.com/machine-contact-layer/mcl-ble). If you have UWB
> radios and find where the specification is wrong,
> [report it](https://github.com/machine-contact-layer/mcl-core/blob/main/REPORTING.md).

## What MCL-UWB provides

- **Carriage of MCL Link frames** over UWB-capable systems
- **Ranging measurements as transport metadata**, with explicit admissibility
  rules for when a measurement may count as proximity evidence
- **Session preservation across a handoff**, like every MCL binding
- **A freestanding C99 reference** with no allocation, no global mutable state
  and no UWB driver: you connect it to the radio stack you already have

It does not define UWB radio hardware, ranging algorithms, regulatory limits or
MCL semantics.

## Use it

- [`spec/binding-v0.md`](spec/binding-v0.md) — the binding specification
- [`include/mcl/uwb_binding.h`](include/mcl/uwb_binding.h) — public API
- [`src/uwb_binding.c`](src/uwb_binding.c) — implementation
- [`tests/test_uwb_binding.c`](tests/test_uwb_binding.c) — round trips and refusal cases

The library builds under `/W4 /WX`, and the compiled object references no libc
symbol, so it links on a freestanding target.

## Maturity

| | Status |
|---|---|
| [`spec/binding-v0.md`](spec/binding-v0.md) | **Research Draft.** Nothing here is frozen. |
| Transport identifier `4` (`MCL_UWB`) | Assigned for use, not frozen |
| Hardware runs | None yet |

## Why this binding exposes no distance field

The API deliberately offers **no verified-distance field and no
proximity-proved flag**, because those are the easiest things to get wrong.

A ranging measurement is admissible as proximity evidence only when ranging was
actually performed, by a method that cancels clock drift, with authenticated
timestamps, at moderate confidence. There is no relaxed mode. Distance
conversion refuses to overflow rather than wrapping into a small and plausible
distance.

A non-line-of-sight measurement stays admissible on purpose: a reflected path is
longer than the direct one, so it can only overstate distance.

None of this defeats a relay, which forwards valid traffic between two locations
in real time. Ranging is evidence for local policy to weigh, never proof of
co-presence. See [`SECURITY.md`](https://github.com/machine-contact-layer/mcl-core/blob/main/SECURITY.md).

## Related repositories

[mcl-core](https://github.com/machine-contact-layer/mcl-core) ·
[mcl-link](https://github.com/machine-contact-layer/mcl-link) ·
[mcl-sdk](https://github.com/machine-contact-layer/mcl-sdk) ·
[mcl-ble](https://github.com/machine-contact-layer/mcl-ble) ·
[mcl-ip](https://github.com/machine-contact-layer/mcl-ip)

## License

Apache-2.0. See [`LICENSE`](LICENSE).
