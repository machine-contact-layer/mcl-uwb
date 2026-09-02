#ifndef MCL_UWB_BINDING_H
#define MCL_UWB_BINDING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MCL-UWB binding v0 (research draft).
 *
 * Carriage of MCL Link frames over Ultra-Wideband, the endpoint representation
 * an MCL TRANSPORT_OFFER references, and a ranging-evidence record.
 *
 * ----------------------------------------------------------------------------
 * The security boundary, which is the reason this binding exists
 * ----------------------------------------------------------------------------
 *
 * UWB is the transport in the MCL family best suited to secure ranging, and
 * that makes it the easiest place to overclaim. The charter is explicit:
 * reception is not identity, is not authority, and is not trust. A distance
 * measurement is evidence about a channel, never proof about a peer.
 *
 * So this binding deliberately does NOT expose a "verified distance" or a
 * "proximity proved" flag. It exposes a measurement together with the
 * conditions under which it was taken, and it requires the caller to record
 * whether a cryptographically scrambled timestamp sequence was actually used.
 * Local policy decides what that is worth. A binding that returned a boolean
 * would be inviting every integrator to treat an unauthenticated time-of-flight
 * figure as an access-control decision.
 *
 * Distance bounding has a substantial adversarial literature and this module
 * implements none of it. Relay and distance-reduction attacks remain possible
 * against an unauthenticated ranging exchange.
 *
 * Contains no UWB driver and no OS calls. Freestanding C99, no allocation,
 * no global mutable state.
 */

typedef int32_t mcl_uwb_status_t;
enum {
    MCL_UWB_OK = 0,
    MCL_UWB_ERR_INVALID_ARGUMENT = 1,
    MCL_UWB_ERR_RANGE = 2,
    MCL_UWB_ERR_TRUNCATED = 3,
    MCL_UWB_ERR_NONCANONICAL = 4,
    MCL_UWB_ERR_UNSUPPORTED = 5
};

/* Transport identifier assigned to this binding in mcl-link. */
#define MCL_UWB_TRANSPORT_ID 0x04u

/* ---------- Session roles and profiles ---------- */

typedef uint8_t mcl_uwb_role_t;
enum {
    MCL_UWB_ROLE_INITIATOR = 0u,
    MCL_UWB_ROLE_RESPONDER = 1u,
    MCL_UWB_ROLE_COUNT     = 2u
};

/*
 * Ranging method, in the sense used by IEEE 802.15.4z.
 * Single-sided two-way ranging is sensitive to clock drift between peers;
 * double-sided cancels most of it. Which was used changes how much a
 * measurement is worth, so it travels with the measurement.
 */
typedef uint8_t mcl_uwb_ranging_method_t;
enum {
    MCL_UWB_RANGING_NONE       = 0u,   /* data carriage only, no ranging performed */
    MCL_UWB_RANGING_SS_TWR     = 1u,   /* single-sided two-way ranging */
    MCL_UWB_RANGING_DS_TWR     = 2u,   /* double-sided two-way ranging */
    MCL_UWB_RANGING_TDOA       = 3u,   /* time difference of arrival */
    MCL_UWB_RANGING_COUNT      = 4u
};

#define MCL_UWB_ADDRESS_SIZE 8u

/*
 * Endpoint offer referenced by an MCL TRANSPORT_OFFER.
 *
 * Canonical encoding:
 *
 *   u8   role
 *   u8   channel            UWB channel number
 *   u8   preamble_code
 *   u8   ranging_method
 *   u8   address[8]
 *   u16  session_id_hint
 *   u16  validity_s         0 means unspecified, not infinite
 */
typedef struct {
    mcl_uwb_role_t role;
    uint8_t channel;
    uint8_t preamble_code;
    mcl_uwb_ranging_method_t ranging_method;
    uint8_t address[MCL_UWB_ADDRESS_SIZE];
    uint16_t session_id_hint;
    uint16_t validity_s;
} mcl_uwb_endpoint_t;

#define MCL_UWB_ENDPOINT_SIZE 16u

mcl_uwb_status_t mcl_uwb_endpoint_encode(
    const mcl_uwb_endpoint_t *endpoint,
    uint8_t *out,
    size_t out_capacity,
    size_t *written);

mcl_uwb_status_t mcl_uwb_endpoint_decode(
    const uint8_t *in,
    size_t in_size,
    mcl_uwb_endpoint_t *endpoint,
    size_t *consumed);

/* ---------- Carriage ---------- */

/*
 * A UWB data frame carries exactly one Link frame. Like a datagram, the frame
 * boundary is the payload boundary and trailing bytes are rejected.
 *
 * 802.15.4 payloads are small, so a Tier-0 Link frame fits and a larger one may
 * not. This binding does not define fragmentation: a peer needing to move more
 * than a governing frame's worth of data should negotiate a richer transport,
 * which is what TRANSPORT_OFFER is for.
 */
#define MCL_UWB_MAX_PAYLOAD 127u

mcl_uwb_status_t mcl_uwb_payload_validate(
    const uint8_t *payload,
    size_t payload_size);

/* Whether a Link frame of this size can be carried at all. */
uint8_t mcl_uwb_fits_payload(size_t frame_size);

/* ---------- Ranging evidence ---------- */

/*
 * The speed of light in air, used to convert time of flight to distance.
 * One nanosecond is very nearly 0.3 metres, which is why UWB can range at all
 * and why acoustic transports cannot do this.
 */
#define MCL_UWB_SPEED_OF_LIGHT_MM_PER_NS 299u

/*
 * A ranging observation and the conditions it was taken under.
 *
 * `authenticated_sts` records whether the exchange used a cryptographically
 * scrambled timestamp sequence. It is the single most important field here:
 * without it, a time-of-flight figure is an engineering measurement and not a
 * security statement, and a receiver must not use it as one.
 *
 * `nlos` records whether the driver reported a non-line-of-sight condition. A
 * reflected path is longer than the direct one, so an NLOS measurement can only
 * ever overstate distance, never understate it.
 */
typedef struct {
    uint32_t time_of_flight_ps;   /* picoseconds, as reported by the radio */
    uint16_t distance_mm;         /* derived, for convenience only */
    uint8_t ranging_method;
    uint8_t authenticated_sts;    /* 1 if a scrambled timestamp sequence was used */
    uint8_t nlos;                 /* 1 if the radio reported non-line-of-sight */
    uint8_t confidence;           /* 0..100, radio-reported quality */
} mcl_uwb_ranging_t;

/*
 * Convert time of flight to distance. Returns MCL_UWB_ERR_RANGE if the result
 * would not fit, rather than wrapping into a small and dangerously plausible
 * distance.
 */
mcl_uwb_status_t mcl_uwb_distance_from_tof(
    uint32_t time_of_flight_ps,
    uint16_t *distance_mm);

/*
 * Whether a ranging observation is admissible as *proximity evidence* for local
 * policy. This is not a trust decision and not an authorisation: it reports
 * whether the measurement is even worth considering.
 *
 * An observation is inadmissible when no ranging was performed, when the method
 * is single-sided (clock drift dominates), when the timestamp sequence was not
 * authenticated, or when the radio reported low confidence. A caller that wants
 * to accept a weaker observation may inspect the fields itself; this function
 * deliberately does not offer a "relaxed" mode.
 */
uint8_t mcl_uwb_ranging_is_admissible_evidence(const mcl_uwb_ranging_t *ranging);

#ifdef __cplusplus
}
#endif

#endif /* MCL_UWB_BINDING_H */
