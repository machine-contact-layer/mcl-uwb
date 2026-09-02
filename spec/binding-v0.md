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


## Endpoint offer

Canonical encoding, network byte order, fixed 16 bytes:

```text
u8   role               0 initiator, 1 responder
u8   channel            UWB channel; 0 is not a valid channel
u8   preamble_code      0 is not a valid code
u8   ranging_method     0 none, 1 SS-TWR, 2 DS-TWR, 3 TDoA
u8   address[8]
u16  session_id_hint
u16  validity_s         0 means unspecified, not infinite
```

## Carriage

One Link frame per UWB data frame, with the frame boundary as the payload
boundary; trailing bytes are rejected. 802.15.4 payloads are small, so a Tier-0
Link frame fits and a larger one may not.

This binding deliberately defines no fragmentation. A peer needing to move more
than a governing frame's worth of data should negotiate a richer transport,
which is what `TRANSPORT_OFFER` exists for.

## Ranging evidence, and its limits

UWB is the transport in the MCL family best suited to secure ranging, which
makes it the easiest place in the whole architecture to overclaim.

The charter is explicit that reception is not identity, not authority, and not
trust. A distance measurement is evidence about a channel, never proof about a
peer.

Accordingly this binding exposes **no** "verified distance" and **no** "proximity
proved" flag. It carries an observation together with the conditions it was
taken under:

```text
time_of_flight_ps
distance_mm            derived, for convenience only
ranging_method
authenticated_sts      whether a scrambled timestamp sequence was used
nlos                   whether the radio reported non-line-of-sight
confidence             0..100, radio-reported
```

An observation is admissible as *proximity evidence* for local policy only when
ranging was actually performed, the method cancels inter-peer clock drift
(double-sided, not single-sided), the timestamp sequence was authenticated, and
radio confidence is at least moderate. Anything weaker is an engineering
measurement, not a security statement.

A non-line-of-sight reading stays admissible on purpose: a reflected path is
longer than the direct one, so NLOS can only overstate distance, never
understate it.

Distance bounding has a substantial adversarial literature and this binding
implements none of it. Relay and distance-reduction attacks remain possible
against an unauthenticated ranging exchange. Nothing here may be cited as
proof of physical co-presence.
