#include "pkt.h"
#include <string.h>

size_t pkt_encode(uint8_t *out, size_t out_cap, const pkt_hdr_t *hdr,
                   const uint8_t *payload, uint8_t payload_len)
{
    size_t sig_len = (hdr->type == PKT_TYPE_DATA) ? PKT_SIG_LEN : 0;
    size_t total = PKT_HDR_LEN + payload_len + sig_len;
    if (total > out_cap) {
        return 0;
    }

    pkt_hdr_t h = *hdr;
    h.payload_len = payload_len;
    memcpy(out, &h, PKT_HDR_LEN);

    if (payload_len) {
        memcpy(out + PKT_HDR_LEN, payload, payload_len);
    }
    if (sig_len) {
        // TODO: prawdziwy podpis kryptograficzny w kroku 2, na razie zera
        memset(out + PKT_HDR_LEN + payload_len, 0, sig_len);
    }
    return total;
}

bool pkt_decode(const uint8_t *in, size_t in_len, pkt_hdr_t *hdr_out, const uint8_t **payload_out)
{
    if (in_len < PKT_HDR_LEN) {
        return false;
    }
    memcpy(hdr_out, in, PKT_HDR_LEN);

    size_t sig_len = (hdr_out->type == PKT_TYPE_DATA) ? PKT_SIG_LEN : 0;
    size_t expected = PKT_HDR_LEN + hdr_out->payload_len + sig_len;
    if (in_len < expected) {
        return false;
    }

    *payload_out = in + PKT_HDR_LEN;
    return true;
}
