/*
 * MCL-UWB binding v0 tests.
 *
 * Endpoint canonicalisation, payload boundary discipline, distance arithmetic
 * including the overflow case, and the evidence-admissibility rules that keep
 * a ranging measurement from being mistaken for a trust decision.
 */

#include "mcl/uwb_binding.h"
#include "mcl/link.h"

#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do {                                      \
    ++tests_run;                                                   \
    if (!(cond)) {                                                 \
        ++tests_failed;                                            \
        printf("  FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
    }                                                              \
} while (0)

static const uint8_t k_presence[] = {
    0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x01u, 0x01u, 0x00u, 0x00u, 0x01u, 0x3Cu
};

static size_t build_frame(uint8_t *out, size_t capacity)
{
    mcl_link_frame_t f;
    size_t written = 0u;

    memset(&f, 0, sizeof(f));
    f.frame_class = MCL_LINK_CLASS_CONTACT;
    f.source_ref = 0x00FFEE11u;
    f.payload = k_presence;
    f.payload_len = (uint16_t)sizeof(k_presence);

    if (mcl_link_frame_encode(&f, out, capacity, &written) != MCL_LINK_OK) {
        return 0u;
    }
    return written;
}

static void test_endpoint_round_trip(void)
{
    mcl_uwb_endpoint_t tx, rx;
    uint8_t buf[32];
    size_t written = 0u, consumed = 0u;
    unsigned i;

    printf("[TEST] endpoint round trip\n");

    memset(&tx, 0, sizeof(tx));
    tx.role = MCL_UWB_ROLE_RESPONDER;
    tx.channel = 9u;
    tx.preamble_code = 10u;
    tx.ranging_method = MCL_UWB_RANGING_DS_TWR;
    for (i = 0u; i < MCL_UWB_ADDRESS_SIZE; ++i) { tx.address[i] = (uint8_t)(0xA0u + i); }
    tx.session_id_hint = 0x1234u;
    tx.validity_s = 600u;

    CHECK(mcl_uwb_endpoint_encode(&tx, buf, sizeof(buf), &written) == MCL_UWB_OK,
          "encode");
    CHECK(written == MCL_UWB_ENDPOINT_SIZE, "fixed endpoint size");
    CHECK(mcl_uwb_endpoint_decode(buf, written, &rx, &consumed) == MCL_UWB_OK,
          "decode");
    CHECK(consumed == written, "consumed all");
    CHECK(rx.channel == 9u, "channel preserved");
    CHECK(rx.preamble_code == 10u, "preamble code preserved");
    CHECK(rx.ranging_method == MCL_UWB_RANGING_DS_TWR, "ranging method preserved");
    CHECK(rx.session_id_hint == 0x1234u, "session hint preserved");
    CHECK(memcmp(rx.address, tx.address, MCL_UWB_ADDRESS_SIZE) == 0,
          "address preserved");
}

static void test_endpoint_rejects_bad_values(void)
{
    mcl_uwb_endpoint_t ep;
    uint8_t buf[32];
    size_t written = 0u, consumed = 0u, n;

    printf("[TEST] endpoint rejects unknown role, method and zero channel\n");

    memset(&ep, 0, sizeof(ep));
    ep.role = MCL_UWB_ROLE_INITIATOR;
    ep.channel = 5u;
    ep.preamble_code = 9u;
    ep.ranging_method = MCL_UWB_RANGING_NONE;
    CHECK(mcl_uwb_endpoint_encode(&ep, buf, sizeof(buf), &written) == MCL_UWB_OK,
          "baseline encodes");

    ep.channel = 0u;
    CHECK(mcl_uwb_endpoint_encode(&ep, buf, sizeof(buf), &written) == MCL_UWB_ERR_RANGE,
          "channel zero rejected");
    ep.channel = 5u;

    ep.preamble_code = 0u;
    CHECK(mcl_uwb_endpoint_encode(&ep, buf, sizeof(buf), &written) == MCL_UWB_ERR_RANGE,
          "preamble code zero rejected");
    ep.preamble_code = 9u;

    ep.ranging_method = (mcl_uwb_ranging_method_t)MCL_UWB_RANGING_COUNT;
    CHECK(mcl_uwb_endpoint_encode(&ep, buf, sizeof(buf), &written) == MCL_UWB_ERR_RANGE,
          "unknown ranging method rejected");
    ep.ranging_method = MCL_UWB_RANGING_NONE;

    ep.role = (mcl_uwb_role_t)MCL_UWB_ROLE_COUNT;
    CHECK(mcl_uwb_endpoint_encode(&ep, buf, sizeof(buf), &written) == MCL_UWB_ERR_RANGE,
          "unknown role rejected");
    ep.role = MCL_UWB_ROLE_INITIATOR;

    (void)mcl_uwb_endpoint_encode(&ep, buf, sizeof(buf), &written);
    buf[1] = 0u;   /* channel zero arriving from the wire */
    CHECK(mcl_uwb_endpoint_decode(buf, written, &ep, &consumed) == MCL_UWB_ERR_NONCANONICAL,
          "channel zero rejected on decode");

    (void)mcl_uwb_endpoint_encode(&ep, buf, sizeof(buf), &written);
    for (n = 0u; n < written; ++n) {
        CHECK(mcl_uwb_endpoint_decode(buf, n, &ep, &consumed) == MCL_UWB_ERR_TRUNCATED,
              "short endpoint reports truncation");
    }
}

static void test_payload_boundary(void)
{
    uint8_t frame[160];
    uint8_t padded[192];
    size_t frame_size;

    printf("[TEST] one Link frame per UWB payload\n");

    frame_size = build_frame(frame, sizeof(frame));
    CHECK(frame_size > 0u, "frame built");
    CHECK(mcl_uwb_fits_payload(frame_size) == 1u, "Tier-0 frame fits a UWB payload");
    CHECK(mcl_uwb_payload_validate(frame, frame_size) == MCL_UWB_OK,
          "exact payload accepted");

    memcpy(padded, frame, frame_size);
    padded[frame_size] = 0x00u;
    CHECK(mcl_uwb_payload_validate(padded, frame_size + 1u) == MCL_UWB_ERR_NONCANONICAL,
          "trailing byte rejected");

    CHECK(mcl_uwb_payload_validate(frame, frame_size - 1u) == MCL_UWB_ERR_TRUNCATED,
          "truncated payload reports truncation");

    memcpy(padded, frame, frame_size);
    padded[0] = (uint8_t)((padded[0] & 0xF0u) | 0x0Fu);
    CHECK(mcl_uwb_payload_validate(padded, frame_size) == MCL_UWB_ERR_NONCANONICAL,
          "unknown frame class is non-canonical, not truncated");

    memcpy(padded, frame, frame_size);
    /* Major 2, not 1. Link major 1 is CUT and is accepted now, so this case
     * has to name a major that is genuinely unassigned or it stops testing
     * anything. */
    padded[0] = (uint8_t)((2u << 4u) | (padded[0] & 0x0Fu));
    CHECK(mcl_uwb_payload_validate(padded, frame_size) == MCL_UWB_ERR_UNSUPPORTED,
          "an unassigned Link major is reported as unsupported");

    /*
     * And the Stable major IS carried. Built properly rather than by rewriting
     * the nibble: the frame check covers the version byte, so a rewritten
     * major fails the CRC and would be refused for the wrong reason -- which
     * would make this look like a passing test of something it never touched.
     */
    {
        mcl_link_frame_t f;
        uint8_t stable[128];
        size_t stable_size = 0u;

        memset(&f, 0, sizeof(f));
        f.frame_class = MCL_LINK_CLASS_DATA;
        f.flags = MCL_LINK_FLAG_SEQUENCE | MCL_LINK_FLAG_FRAME_CHECK;
        f.source_ref = 0x0A0B0C0Du;
        f.sequence = 9u;
        f.payload = k_presence;
        f.payload_len = (uint16_t)sizeof(k_presence);
        CHECK(mcl_link_frame_encode_at_major(MCL_LINK_STABLE_MAJOR, &f, stable,
                                             sizeof(stable), &stable_size)
                  == MCL_LINK_OK,
              "a Stable-major frame encodes");
        CHECK(mcl_uwb_payload_validate(stable, stable_size) == MCL_UWB_OK,
              "and this binding carries it");
    }
    CHECK(mcl_uwb_payload_validate(NULL, frame_size) == MCL_UWB_ERR_INVALID_ARGUMENT,
          "null payload rejected");

    CHECK(mcl_uwb_fits_payload(MCL_UWB_MAX_PAYLOAD + 1u) == 0u,
          "oversize frame does not fit");
    CHECK(mcl_uwb_fits_payload(0u) == 0u, "empty does not fit");
}

static void test_distance_arithmetic(void)
{
    uint16_t mm = 0u;

    printf("[TEST] time of flight converts to distance without wrapping\n");

    /* 1 ns of flight is very nearly 0.3 m. */
    CHECK(mcl_uwb_distance_from_tof(1000u, &mm) == MCL_UWB_OK, "1 ns converts");
    CHECK(mm == 299u, "1 ns is 299 mm");

    CHECK(mcl_uwb_distance_from_tof(0u, &mm) == MCL_UWB_OK, "zero converts");
    CHECK(mm == 0u, "zero is zero");

    /* 10 m is about 33.4 ns. */
    CHECK(mcl_uwb_distance_from_tof(33445u, &mm) == MCL_UWB_OK, "10 m converts");
    CHECK(mm > 9900u && mm < 10100u, "10 m is about 10000 mm");

    /*
     * The critical case. A very long time of flight must be refused, never
     * wrapped into a small and entirely plausible distance.
     */
    CHECK(mcl_uwb_distance_from_tof(0xFFFFFFFFu, &mm) == MCL_UWB_ERR_RANGE,
          "overflow refused rather than wrapped");
    CHECK(mcl_uwb_distance_from_tof(1000000u, &mm) == MCL_UWB_ERR_RANGE,
          "300 m exceeds the representable range and is refused");
    CHECK(mcl_uwb_distance_from_tof(1000u, NULL) == MCL_UWB_ERR_INVALID_ARGUMENT,
          "null output rejected");
}

static void test_ranging_evidence_rules(void)
{
    mcl_uwb_ranging_t r;

    printf("[TEST] ranging admissibility is conservative by construction\n");

    /* The one admissible shape: double-sided, authenticated, confident. */
    memset(&r, 0, sizeof(r));
    r.ranging_method = MCL_UWB_RANGING_DS_TWR;
    r.authenticated_sts = 1u;
    r.confidence = 90u;
    CHECK(mcl_uwb_ranging_is_admissible_evidence(&r) == 1u,
          "authenticated double-sided ranging is admissible");

    /* Everything else is not. */
    r.authenticated_sts = 0u;
    CHECK(mcl_uwb_ranging_is_admissible_evidence(&r) == 0u,
          "unauthenticated timestamps are not evidence");
    r.authenticated_sts = 1u;

    r.ranging_method = MCL_UWB_RANGING_SS_TWR;
    CHECK(mcl_uwb_ranging_is_admissible_evidence(&r) == 0u,
          "single-sided ranging is not evidence, clock drift dominates");

    r.ranging_method = MCL_UWB_RANGING_NONE;
    CHECK(mcl_uwb_ranging_is_admissible_evidence(&r) == 0u,
          "no ranging performed is not evidence");

    r.ranging_method = MCL_UWB_RANGING_DS_TWR;
    r.confidence = 49u;
    CHECK(mcl_uwb_ranging_is_admissible_evidence(&r) == 0u,
          "low radio confidence is not evidence");
    r.confidence = 50u;
    CHECK(mcl_uwb_ranging_is_admissible_evidence(&r) == 1u,
          "confidence threshold is inclusive");

    r.ranging_method = (uint8_t)MCL_UWB_RANGING_COUNT;
    CHECK(mcl_uwb_ranging_is_admissible_evidence(&r) == 0u,
          "unknown ranging method is not evidence");

    CHECK(mcl_uwb_ranging_is_admissible_evidence(NULL) == 0u,
          "null observation is not evidence");

    /*
     * An NLOS measurement stays admissible on purpose. A reflected path is
     * longer than the direct one, so it can only overstate distance. Treating
     * it as inadmissible would discard a safe reading; treating it as exact
     * would be the error, and that is the caller's to avoid.
     */
    memset(&r, 0, sizeof(r));
    r.ranging_method = MCL_UWB_RANGING_DS_TWR;
    r.authenticated_sts = 1u;
    r.confidence = 80u;
    r.nlos = 1u;
    CHECK(mcl_uwb_ranging_is_admissible_evidence(&r) == 1u,
          "NLOS can only overstate distance, so it remains admissible");
}

int main(void)
{
    printf("MCL-UWB binding v0 tests\n");
    printf("========================\n");

    test_endpoint_round_trip();
    test_endpoint_rejects_bad_values();
    test_payload_boundary();
    test_distance_arithmetic();
    test_ranging_evidence_rules();

    printf("\n%d checks, %d failed\n", tests_run, tests_failed);
    return (tests_failed == 0) ? 0 : 1;
}
