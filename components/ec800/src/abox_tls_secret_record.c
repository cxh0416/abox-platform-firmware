#include "abox_tls_secret_record.h"
#include <string.h>
static int ascii_text(const char *s, size_t capacity, size_t exact)
{
    size_t i;
    for (i = 0; i < capacity; ++i) {
        unsigned char ch = (unsigned char)s[i];
        if (!ch) return i && (!exact || i == exact);
        if (ch < 0x21 || ch > 0x7e || ch == '"' || ch == '\\' || ch == ':') return 0;
    }
    return 0;
}
int ABoxTlsCredentials_Valid(const ABoxTlsCredentials *p)
{
    if (!p) return 0;
    if (p->mode == ABOX_TLS_AUTH_CA) return 1;
    return p->mode == ABOX_TLS_AUTH_PSK && p->generation &&
           ascii_text(p->identity, sizeof(p->identity), 0) &&
           ascii_text(p->secret, sizeof(p->secret), 32);
}
#define SECRET_RECORD_MAGIC 0x50534B31U
static uint32_t checksum(const ABoxTlsCredentials *c)
{
    const unsigned char *p = (const unsigned char *)c;
    uint32_t value = 2166136261U;
    unsigned i;
    for (i = 0; i < sizeof(*c); ++i) value = (value ^ p[i]) * 16777619U;
    return value;
}
void ABoxTlsSecretRecord_Encode(ABoxTlsSecretRecord *r, const ABoxTlsCredentials *c)
{
    memset(r, 0, sizeof(*r));
    r->magic = SECRET_RECORD_MAGIC;
    /* Do not persist indeterminate padding. */
    r->credentials.mode = c->mode;
    r->credentials.generation = c->generation;
    memcpy(r->credentials.identity, c->identity, sizeof(c->identity));
    memcpy(r->credentials.secret, c->secret, sizeof(c->secret));
    r->checksum = checksum(&r->credentials);
}
int ABoxTlsSecretRecord_Decode(const ABoxTlsSecretRecord *r, ABoxTlsCredentials *c)
{
    const unsigned char *p = (const unsigned char *)r;
    unsigned i;
    memset(c, 0, sizeof(*c));
    for (i = 0; i < sizeof(*r) && p[i] == 0xff; ++i) {}
    if (i == sizeof(*r)) return 1;
    if (r->magic == SECRET_RECORD_MAGIC && r->checksum == checksum(&r->credentials) &&
        ABoxTlsCredentials_Valid(&r->credentials)) {
        *c = r->credentials; return 1;
    }
    c->mode = 0xff; return 0;
}
