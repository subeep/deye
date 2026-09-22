#include <stdint.h>
#include <turbofec/turbo.h>
#include <turbofec/rate_match.h>

/* Descrambled soft bits (positive = one) -> 176 bytes, including CRC24A. */
int drone_turbo_decode(int8_t *bits, uint8_t *out) {
    int8_t a[1412], b[1412], c[1412];
    struct lte_rate_matcher *m = lte_rate_matcher_alloc();
    struct tdecoder *d = alloc_tdec();
    if (!m || !d) {
        if (m) lte_rate_matcher_free(m);
        if (d) free_tdec(d);
        return -1;
    }
    struct lte_rate_matcher_io io = {1412, 7200, {a, b, c}, bits};
    int rc = lte_rate_match_rv(m, &io, 0);
    if (!rc) rc = lte_turbo_decode(d, 1408, 6, out, a, b, c);
    free_tdec(d);
    lte_rate_matcher_free(m);
    return rc;
}
