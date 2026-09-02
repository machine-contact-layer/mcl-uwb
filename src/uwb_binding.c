/*
 * MCL-UWB binding v0 reference implementation.
 *
 * Freestanding C99. No UWB driver, no allocation, no global mutable state.
 */

#include "mcl/uwb_binding.h"
#include "mcl/link.h"

static uint8_t mcl_uwb_endpoint_valid(const mcl_uwb_endpoint_t *endpoint)
{
    if (endpoint == NULL) {
        return 0u;
    }
    if (endpoint->role >= MCL_UWB_ROLE_COUNT) {
        return 0u;
    }
    if (endpoint->ranging_method >= MCL_UWB_RANGING_COUNT) {
        return 0u;
    }
    /* Channel 0 is not a valid UWB channel; an offer naming it is malformed. */
    if (endpoint->channel == 0u) {
        return 0u;
    }
    if (endpoint->preamble_code == 0u) {
        return 0u;
    }
    return 1u;
}

mcl_uwb_status_t mcl_uwb_endpoint_encode(
    const mcl_uwb_endpoint_t *endpoint,
    uint8_t *out,
    size_t out_capacity,
    size_t *written)
{
    size_t pos = 0u;
    size_t i;

    if (endpoint == NULL || out == NULL || written == NULL) {
        return MCL_UWB_ERR_INVALID_ARGUMENT;
    }
    if (mcl_uwb_endpoint_valid(endpoint) == 0u) {
        return MCL_UWB_ERR_RANGE;
    }
    if (out_capacity < (size_t)MCL_UWB_ENDPOINT_SIZE) {
        return MCL_UWB_ERR_RANGE;
    }

    out[pos++] = endpoint->role;
    out[pos++] = endpoint->channel;
    out[pos++] = endpoint->preamble_code;
    out[pos++] = endpoint->ranging_method;

    for (i = 0u; i < (size_t)MCL_UWB_ADDRESS_SIZE; ++i) {
        out[pos + i] = endpoint->address[i];
    }
    pos += (size_t)MCL_UWB_ADDRESS_SIZE;

    out[pos++] = (uint8_t)(endpoint->session_id_hint >> 8u);
    out[pos++] = (uint8_t)(endpoint->session_id_hint & 0xFFu);
    out[pos++] = (uint8_t)(endpoint->validity_s >> 8u);
    out[pos++] = (uint8_t)(endpoint->validity_s & 0xFFu);

    *written = pos;
    return MCL_UWB_OK;
}

mcl_uwb_status_t mcl_uwb_endpoint_decode(
    const uint8_t *in,
    size_t in_size,
    mcl_uwb_endpoint_t *endpoint,
    size_t *consumed)
{
    size_t pos = 0u;
    size_t i;

    if (in == NULL || endpoint == NULL || consumed == NULL) {
        return MCL_UWB_ERR_INVALID_ARGUMENT;
    }
    if (in_size < (size_t)MCL_UWB_ENDPOINT_SIZE) {
        return MCL_UWB_ERR_TRUNCATED;
    }

    endpoint->role = in[pos++];
    endpoint->channel = in[pos++];
    endpoint->preamble_code = in[pos++];
    endpoint->ranging_method = in[pos++];

    for (i = 0u; i < (size_t)MCL_UWB_ADDRESS_SIZE; ++i) {
        endpoint->address[i] = in[pos + i];
    }
    pos += (size_t)MCL_UWB_ADDRESS_SIZE;

    endpoint->session_id_hint =
        (uint16_t)(((uint16_t)in[pos] << 8u) | (uint16_t)in[pos + 1u]);
    pos += 2u;
    endpoint->validity_s =
        (uint16_t)(((uint16_t)in[pos] << 8u) | (uint16_t)in[pos + 1u]);
    pos += 2u;

    if (mcl_uwb_endpoint_valid(endpoint) == 0u) {
        return MCL_UWB_ERR_NONCANONICAL;
    }

    *consumed = pos;
    return MCL_UWB_OK;
}

uint8_t mcl_uwb_fits_payload(size_t frame_size)
{
    return (frame_size != 0u && frame_size <= (size_t)MCL_UWB_MAX_PAYLOAD) ? 1u : 0u;
}

mcl_uwb_status_t mcl_uwb_payload_validate(
    const uint8_t *payload,
    size_t payload_size)
{
    mcl_link_frame_t frame;
    size_t consumed = 0u;
    mcl_link_status_t st;

    if (payload == NULL) {
        return MCL_UWB_ERR_INVALID_ARGUMENT;
    }
    if (payload_size > (size_t)MCL_UWB_MAX_PAYLOAD) {
        return MCL_UWB_ERR_RANGE;
    }

    st = mcl_link_frame_decode(payload, payload_size, &frame, &consumed);
    if (st != MCL_LINK_OK) {
        return (st == MCL_LINK_ERR_RANGE) ? MCL_UWB_ERR_TRUNCATED : MCL_UWB_ERR_RANGE;
    }
    if (consumed != payload_size) {
        /* One frame per UWB data frame. Trailing bytes are not ours. */
        return MCL_UWB_ERR_NONCANONICAL;
    }

    return MCL_UWB_OK;
}

mcl_uwb_status_t mcl_uwb_distance_from_tof(
    uint32_t time_of_flight_ps,
    uint16_t *distance_mm)
{
    /*
     * distance_mm = tof_ps * c_mm_per_ns / 1000
     *
     * Computed in 64-bit so the intermediate cannot wrap. A wrapped value here
     * would present as a small, entirely plausible distance, which is the worst
     * possible failure mode for a measurement that feeds proximity decisions.
     */
    uint64_t mm;

    if (distance_mm == NULL) {
        return MCL_UWB_ERR_INVALID_ARGUMENT;
    }

    mm = ((uint64_t)time_of_flight_ps * (uint64_t)MCL_UWB_SPEED_OF_LIGHT_MM_PER_NS)
         / 1000u;

    if (mm > 0xFFFFu) {
        return MCL_UWB_ERR_RANGE;
    }

    *distance_mm = (uint16_t)mm;
    return MCL_UWB_OK;
}

uint8_t mcl_uwb_ranging_is_admissible_evidence(const mcl_uwb_ranging_t *ranging)
{
    if (ranging == NULL) {
        return 0u;
    }

    /* No ranging was performed, so there is nothing to weigh. */
    if (ranging->ranging_method == MCL_UWB_RANGING_NONE) {
        return 0u;
    }
    if (ranging->ranging_method >= MCL_UWB_RANGING_COUNT) {
        return 0u;
    }

    /*
     * Single-sided two-way ranging does not cancel clock drift between the two
     * peers, so its error can be large and is not bounded by anything the
     * receiver controls. It is useful for coarse presence, not as evidence.
     */
    if (ranging->ranging_method == MCL_UWB_RANGING_SS_TWR) {
        return 0u;
    }

    /*
     * Without an authenticated scrambled timestamp sequence, the timing figure
     * is an engineering measurement and carries no adversarial guarantee.
     */
    if (ranging->authenticated_sts == 0u) {
        return 0u;
    }

    if (ranging->confidence < 50u) {
        return 0u;
    }

    return 1u;
}
