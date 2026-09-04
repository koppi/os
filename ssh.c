/**
 * @file ssh.c
 * @brief In-kernel SSHv2 server (see ssh.h for the algorithm/scope summary).
 *
 * Everything here runs synchronously on the `net` thread: @ref ssh_tick
 * polls the listening socket, and once a client is accepted the whole
 * session (version/KEX/auth/shell) runs to completion in
 * @ref ssh_serve_connection before returning to the caller. All sizable
 * buffers are function-local `static` (not stack) since the 16 KiB kernel
 * stack has no room for multi-KB packet buffers, and there is only ever one
 * session in flight so that costs nothing.
 */
#include <ssh.h>

#include <tcp.h>
#include <net.h>
#include <vfs.h>
#include <commands.h>
#include <kconsole.h>
#include <printf.h>
#include <log.h>
#include <csprng.h>
#include <sha2.h>
#include <curve25519.h>
#include <ed25519.h>
#include <aes128.h>
#include <lib/string.h>

/* ---- configuration ------------------------------------------------- */
#define SSH_PORT            22
/* No user database exists in this OS -- a single fixed credential stands
 * in for one. Change these if you don't want the defaults. */
#define SSH_USERNAME        "koppi"
#define SSH_PASSWORD        "os"
#define HOSTKEY_PATH        "/hda/sshkey"   /* short: this FAT driver is 8.3-only */
#define SSH_MAX_PACKET      8192
#define SSH_MAX_AUTH_TRIES  6
#define SSH_MAX_DATA_CHUNK  4096

/* ---- SSH message numbers (RFC 4253 / 4252 / 4254) ------------------- */
#define SSH_MSG_SERVICE_REQUEST            5
#define SSH_MSG_SERVICE_ACCEPT             6
#define SSH_MSG_KEXINIT                    20
#define SSH_MSG_NEWKEYS                    21
#define SSH_MSG_KEX_ECDH_INIT              30
#define SSH_MSG_KEX_ECDH_REPLY             31
#define SSH_MSG_USERAUTH_REQUEST           50
#define SSH_MSG_USERAUTH_FAILURE           51
#define SSH_MSG_USERAUTH_SUCCESS           52
#define SSH_MSG_GLOBAL_REQUEST             80
#define SSH_MSG_REQUEST_FAILURE            82
#define SSH_MSG_CHANNEL_OPEN               90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION  91
#define SSH_MSG_CHANNEL_OPEN_FAILURE       92
#define SSH_MSG_CHANNEL_DATA               94
#define SSH_MSG_CHANNEL_EOF                96
#define SSH_MSG_CHANNEL_CLOSE              97
#define SSH_MSG_CHANNEL_REQUEST            98
#define SSH_MSG_CHANNEL_SUCCESS            99
#define SSH_MSG_CHANNEL_FAILURE            100

typedef struct {
    int      h;
    uint32_t seq_tx, seq_rx;
    int      enc_tx, enc_rx;
    aes128_ctx ctx_tx, ctx_rx;
    uint8_t  counter_tx[16], counter_rx[16];
    uint8_t  mac_key_tx[32], mac_key_rx[32];
} ssh_conn_t;

static int listen_h = -1;
static int session_count = 0;

const char *ssh_status_str(void) {
    static char buf[64];
    snprintf(buf, sizeof buf, "listening on :%d (%d session%s served)",
             SSH_PORT, session_count, session_count == 1 ? "" : "s");
    return buf;
}

/* ------------------------------------------------------------------ *
 *  Small helpers                                                      *
 * ------------------------------------------------------------------ */
static int bytes_eq(const uint8_t *a, const uint8_t *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static int recv_exact(int h, uint8_t *buf, int n) {
    int got = 0;
    while (got < n) {
        int r = tcp_recv(h, buf + got, n - got);
        if (r > 0) { got += r; continue; }
        if (r == 0) {
            if (tcp_is_open(h))
                continue;           /* idle timeout, not a real EOF -- keep waiting */
            return -1;
        }
        return -1;
    }
    return got;
}

static int send_all(int h, const uint8_t *buf, int n) {
    int off = 0;
    while (off < n) {
        int w = tcp_send(h, buf + off, n - off);
        if (w <= 0)
            return -1;
        off += w;
    }
    return off;
}

/* ---- wire-format cursors -------------------------------------------- */
typedef struct { const uint8_t *p; int len; int off; } reader_t;

static uint8_t rd_byte(reader_t *r) { return (r->off < r->len) ? r->p[r->off++] : 0; }

static uint32_t rd_u32(reader_t *r) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v = (v << 8) | rd_byte(r);
    return v;
}

static const uint8_t *rd_string(reader_t *r, uint32_t *outlen) {
    uint32_t n = rd_u32(r);
    if (r->off > r->len || n > (uint32_t)(r->len - r->off)) {
        *outlen = 0;
        return r->p + r->off;
    }
    const uint8_t *p = r->p + r->off;
    r->off += (int)n;
    *outlen = n;
    return p;
}

static int wr_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
    return 4;
}

static int wr_string(uint8_t *p, const void *data, int len) {
    wr_u32(p, (uint32_t)len);
    if (len)
        memcpy(p + 4, (void *)data, len);
    return 4 + len;
}

static int wr_str_c(uint8_t *p, const char *s) {
    return wr_string(p, s, (int)strlen(s));
}

/* ------------------------------------------------------------------ *
 *  Host key (Ed25519, persisted to disk so it survives reboots)       *
 * ------------------------------------------------------------------ */
static uint8_t hk_seed[32], hk_pub[32], hk_scalar[32], hk_prefix[32];
static int hk_ready;

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_decode(const char *in, uint8_t *out, int n) {
    for (int i = 0; i < n; i++) {
        int hi = hex_val(in[i * 2]);
        int lo = hex_val(in[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

static void hex_encode(const uint8_t *in, int n, char *out) {
    static const char *d = "0123456789abcdef";
    for (int i = 0; i < n; i++) {
        out[i * 2]     = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 15];
    }
}

static void ensure_hostkey(void) {
    if (hk_ready)
        return;

    int loaded = 0;
    file *f = vfs_file_open(HOSTKEY_PATH, "r");
    if (f->type == FS_FILE) {
        static char buf[512];
        memset(buf, 0, sizeof buf);
        vfs_file_read(f, buf);
        if (hex_decode(buf, hk_seed, 32))
            loaded = 1;
    }
    vfs_file_close(f);

    if (!loaded) {
        csprng_bytes(hk_seed, 32);
        vfs_touch(HOSTKEY_PATH);
        file *wf = vfs_file_open(HOSTKEY_PATH, "w");
        if (wf->type == FS_FILE) {
            char hexbuf[65];
            hex_encode(hk_seed, 32, hexbuf);
            hexbuf[64] = 0;
            vfs_file_write(wf, hexbuf);
        } else {
            klogf(LOG_WARNING, "ssh: could not persist host key to %s\n", HOSTKEY_PATH);
        }
        vfs_file_close(wf);
        klogf(LOG_INFO, "ssh: generated a new host key\n");
    }

    ed25519_derive(hk_seed, hk_pub, hk_scalar, hk_prefix);
    hk_ready = 1;
}

/* ------------------------------------------------------------------ *
 *  Binary packet protocol (RFC 4253 section 6)                        *
 * ------------------------------------------------------------------ */
static void hmac_seq_packet(const uint8_t key[32], uint32_t seq,
                            const uint8_t len_field[4], const uint8_t *content,
                            int content_len, uint8_t out[32]) {
    uint8_t k0[64];
    memset(k0, 0, sizeof k0);
    memcpy(k0, (void *)key, 32);
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; i++) {
        ipad[i] = (uint8_t)(k0[i] ^ 0x36);
        opad[i] = (uint8_t)(k0[i] ^ 0x5c);
    }
    uint8_t seqbuf[4];
    wr_u32(seqbuf, seq);

    sha256_ctx c;
    uint8_t inner[32];
    sha256_init(&c);
    sha256_update(&c, ipad, 64);
    sha256_update(&c, seqbuf, 4);
    sha256_update(&c, len_field, 4);
    sha256_update(&c, content, content_len);
    sha256_final(&c, inner);

    sha256_init(&c);
    sha256_update(&c, opad, 64);
    sha256_update(&c, inner, 32);
    sha256_final(&c, out);
}

/** @brief Frame, MAC, encrypt (if active) and send one SSH packet. */
static int send_packet(ssh_conn_t *sc, const uint8_t *payload, int paylen) {
    int block = sc->enc_tx ? 16 : 8;

    int pad = block - ((5 + paylen) % block);
    if (pad < 4)
        pad += block;
    int content_len = 1 + paylen + pad;   /* padding_length + payload + padding */

    static uint8_t buf[4 + SSH_MAX_PACKET + 32];
    if (5 + paylen + pad > (int)sizeof buf)
        return -1;

    wr_u32(buf, (uint32_t)content_len);
    buf[4] = (uint8_t)pad;
    memcpy(buf + 5, (void *)payload, paylen);
    csprng_bytes(buf + 5 + paylen, pad);

    int total = 4 + content_len;

    uint8_t mac[32];
    if (sc->enc_tx)
        hmac_seq_packet(sc->mac_key_tx, sc->seq_tx, buf, buf + 4, content_len, mac);

    if (sc->enc_tx)
        aes128_ctr_xor(&sc->ctx_tx, sc->counter_tx, buf, buf, (uint32_t)total);

    if (send_all(sc->h, buf, total) < 0)
        return -1;
    if (sc->enc_tx && send_all(sc->h, mac, 32) < 0)
        return -1;

    sc->seq_tx++;
    return 0;
}

/** @brief Receive, decrypt (if active) and verify one SSH packet's payload. */
static int recv_packet(ssh_conn_t *sc, uint8_t *payload_out, int *payload_len, int maxlen) {
    int block = sc->enc_rx ? 16 : 8;

    uint8_t first[16];
    if (recv_exact(sc->h, first, block) < 0)
        return -1;
    if (sc->enc_rx)
        aes128_ctr_xor(&sc->ctx_rx, sc->counter_rx, first, first, 16);

    uint32_t packet_length = ((uint32_t)first[0] << 24) | ((uint32_t)first[1] << 16) |
                             ((uint32_t)first[2] << 8) | first[3];
    if (packet_length < 1 || packet_length > (uint32_t)SSH_MAX_PACKET)
        return -1;

    static uint8_t content[SSH_MAX_PACKET + 32];
    int have = block - 4;
    memcpy(content, first + 4, have);

    int need = (int)packet_length - have;
    if (need < 0 || have + need > (int)sizeof content)
        return -1;
    if (need > 0) {
        if (recv_exact(sc->h, content + have, need) < 0)
            return -1;
        if (sc->enc_rx)
            aes128_ctr_xor(&sc->ctx_rx, sc->counter_rx, content + have, content + have, (uint32_t)need);
    }

    if (sc->enc_rx) {
        uint8_t mac[32], expect[32];
        if (recv_exact(sc->h, mac, 32) < 0)
            return -1;
        hmac_seq_packet(sc->mac_key_rx, sc->seq_rx, first, content, (int)packet_length, expect);
        if (!bytes_eq(mac, expect, 32))
            return -1;
    }

    int padding_length = content[0];
    int paylen = (int)packet_length - 1 - padding_length;
    if (paylen < 0 || paylen > maxlen)
        return -1;
    memcpy(payload_out, content + 1, paylen);
    *payload_len = paylen;

    sc->seq_rx++;
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Key exchange (curve25519-sha256, RFC 8731) + derivation (RFC 4253) *
 * ------------------------------------------------------------------ */
static void hash_str(sha256_ctx *c, const void *data, int len) {
    uint8_t lb[4];
    wr_u32(lb, (uint32_t)len);
    sha256_update(c, lb, 4);
    sha256_update(c, data, len);
}

/** @brief Feed a big-endian integer into a running hash as an SSH mpint. */
static void hash_mpint(sha256_ctx *c, const uint8_t *be, int len) {
    int start = 0;
    while (start < len - 1 && be[start] == 0)
        start++;
    if (len == 0 || start == len) {
        uint8_t z[4] = { 0, 0, 0, 0 };
        sha256_update(c, z, 4);
        return;
    }
    int need_zero = (be[start] & 0x80) ? 1 : 0;
    uint32_t clen = (uint32_t)(len - start) + (uint32_t)need_zero;
    uint8_t lb[4];
    wr_u32(lb, clen);
    sha256_update(c, lb, 4);
    if (need_zero) {
        uint8_t z = 0;
        sha256_update(c, &z, 1);
    }
    sha256_update(c, be + start, len - start);
}

static int build_kexinit(uint8_t *buf) {
    int n = 0;
    buf[n++] = SSH_MSG_KEXINIT;
    csprng_bytes(buf + n, 16);
    n += 16;
    n += wr_str_c(buf + n, "curve25519-sha256");
    n += wr_str_c(buf + n, "ssh-ed25519");
    n += wr_str_c(buf + n, "aes128-ctr");
    n += wr_str_c(buf + n, "aes128-ctr");
    n += wr_str_c(buf + n, "hmac-sha2-256");
    n += wr_str_c(buf + n, "hmac-sha2-256");
    n += wr_str_c(buf + n, "none");
    n += wr_str_c(buf + n, "none");
    n += wr_str_c(buf + n, "");
    n += wr_str_c(buf + n, "");
    buf[n++] = 0;                 /* first_kex_packet_follows */
    n += wr_u32(buf + n, 0);      /* reserved */
    return n;
}

static int list_has(const uint8_t *data, uint32_t len, const char *name) {
    int nlen = (int)strlen(name);
    uint32_t i = 0;
    while (i < len) {
        uint32_t start = i;
        while (i < len && data[i] != ',')
            i++;
        if ((int)(i - start) == nlen && bytes_eq(data + start, (const uint8_t *)name, nlen))
            return 1;
        i++;
    }
    return 0;
}

static int verify_kexinit_algos(const uint8_t *payload, int len) {
    if (len < 17)
        return 0;
    reader_t r = { payload + 17, len - 17, 0 };   /* skip msg type + 16-byte cookie */
    uint32_t l;
    const uint8_t *s;
    s = rd_string(&r, &l); if (!list_has(s, l, "curve25519-sha256")) return 0;
    s = rd_string(&r, &l); if (!list_has(s, l, "ssh-ed25519"))       return 0;
    s = rd_string(&r, &l); if (!list_has(s, l, "aes128-ctr"))        return 0;
    s = rd_string(&r, &l); if (!list_has(s, l, "aes128-ctr"))        return 0;
    s = rd_string(&r, &l); if (!list_has(s, l, "hmac-sha2-256"))     return 0;
    s = rd_string(&r, &l); if (!list_has(s, l, "hmac-sha2-256"))     return 0;
    return 1;
}

static int build_ecdh_reply(uint8_t *buf, const uint8_t q_s[32], const uint8_t sig[64]) {
    int n = 0;
    buf[n++] = SSH_MSG_KEX_ECDH_REPLY;

    uint8_t ks[4 + 11 + 4 + 32];
    int ksn = 0;
    ksn += wr_str_c(ks + ksn, "ssh-ed25519");
    ksn += wr_string(ks + ksn, hk_pub, 32);
    n += wr_string(buf + n, ks, ksn);

    n += wr_string(buf + n, q_s, 32);

    uint8_t sb[4 + 11 + 4 + 64];
    int sbn = 0;
    sbn += wr_str_c(sb + sbn, "ssh-ed25519");
    sbn += wr_string(sb + sbn, sig, 64);
    n += wr_string(buf + n, sb, sbn);

    return n;
}

static void compute_exchange_hash(const char *vc, const char *vs,
                                  const uint8_t *i_c, int i_c_len,
                                  const uint8_t *i_s, int i_s_len,
                                  const uint8_t *q_c, const uint8_t *q_s,
                                  const uint8_t *shared_be, uint8_t H[32]) {
    sha256_ctx c;
    sha256_init(&c);
    hash_str(&c, vc, (int)strlen(vc));
    hash_str(&c, vs, (int)strlen(vs));
    hash_str(&c, i_c, i_c_len);
    hash_str(&c, i_s, i_s_len);

    uint8_t ks[4 + 11 + 4 + 32];
    int ksn = 0;
    ksn += wr_str_c(ks + ksn, "ssh-ed25519");
    ksn += wr_string(ks + ksn, hk_pub, 32);
    hash_str(&c, ks, ksn);

    hash_str(&c, q_c, 32);
    hash_str(&c, q_s, 32);
    hash_mpint(&c, shared_be, 32);

    sha256_final(&c, H);
}

static void kdf(const uint8_t *K_be, int K_be_len, const uint8_t H[32], char letter,
                const uint8_t session_id[32], uint8_t *out, int outlen) {
    sha256_ctx c;
    sha256_init(&c);
    hash_mpint(&c, K_be, K_be_len);
    sha256_update(&c, H, 32);
    uint8_t l = (uint8_t)letter;
    sha256_update(&c, &l, 1);
    sha256_update(&c, session_id, 32);
    uint8_t full[32];
    sha256_final(&c, full);
    memcpy(out, full, outlen);
}

static void derive_keys(ssh_conn_t *sc, const uint8_t *K_be, const uint8_t H[32],
                        const uint8_t session_id[32]) {
    uint8_t iv_c2s[16], iv_s2c[16], enc_c2s[16], enc_s2c[16], mac_c2s[32], mac_s2c[32];
    kdf(K_be, 32, H, 'A', session_id, iv_c2s, 16);
    kdf(K_be, 32, H, 'B', session_id, iv_s2c, 16);
    kdf(K_be, 32, H, 'C', session_id, enc_c2s, 16);
    kdf(K_be, 32, H, 'D', session_id, enc_s2c, 16);
    kdf(K_be, 32, H, 'E', session_id, mac_c2s, 32);
    kdf(K_be, 32, H, 'F', session_id, mac_s2c, 32);

    memcpy(sc->counter_rx, iv_c2s, 16);
    aes128_init(&sc->ctx_rx, enc_c2s);
    memcpy(sc->mac_key_rx, mac_c2s, 32);

    memcpy(sc->counter_tx, iv_s2c, 16);
    aes128_init(&sc->ctx_tx, enc_s2c);
    memcpy(sc->mac_key_tx, mac_s2c, 32);
}

/* ------------------------------------------------------------------ *
 *  User authentication (RFC 4252) -- fixed username/password          *
 * ------------------------------------------------------------------ */
static int do_userauth(ssh_conn_t *sc) {
    static uint8_t p[SSH_MAX_PACKET];
    int plen;

    if (recv_packet(sc, p, &plen, sizeof p) < 0)
        return -1;
    if (plen < 1 || p[0] != SSH_MSG_SERVICE_REQUEST)
        return -1;

    uint8_t resp[32];
    int n = 0;
    resp[n++] = SSH_MSG_SERVICE_ACCEPT;
    n += wr_str_c(resp + n, "ssh-userauth");
    if (send_packet(sc, resp, n) < 0)
        return -1;

    for (int tries = 0; tries < SSH_MAX_AUTH_TRIES; tries++) {
        if (recv_packet(sc, p, &plen, sizeof p) < 0)
            return -1;
        if (plen < 1 || p[0] != SSH_MSG_USERAUTH_REQUEST)
            return -1;

        reader_t r = { p + 1, plen - 1, 0 };
        uint32_t ulen, slen, mlen;
        const uint8_t *uname  = rd_string(&r, &ulen);
        (void)rd_string(&r, &slen);
        const uint8_t *method = rd_string(&r, &mlen);

        int ok = 0;
        if (mlen == 8 && bytes_eq(method, (const uint8_t *)"password", 8)) {
            (void)rd_byte(&r);
            uint32_t pwlen;
            const uint8_t *pw = rd_string(&r, &pwlen);
            if (ulen == (uint32_t)strlen(SSH_USERNAME) &&
                bytes_eq(uname, (const uint8_t *)SSH_USERNAME, (int)ulen) &&
                pwlen == (uint32_t)strlen(SSH_PASSWORD) &&
                bytes_eq(pw, (const uint8_t *)SSH_PASSWORD, (int)pwlen))
                ok = 1;
        }

        if (ok) {
            uint8_t s[1] = { SSH_MSG_USERAUTH_SUCCESS };
            return send_packet(sc, s, 1);
        }

        uint8_t f[32];
        int fn = 0;
        f[fn++] = SSH_MSG_USERAUTH_FAILURE;
        fn += wr_str_c(f + fn, "password");
        f[fn++] = 0;
        if (send_packet(sc, f, fn) < 0)
            return -1;
    }
    return -1;
}

/* ------------------------------------------------------------------ *
 *  Connection protocol (RFC 4254) + console bridging                  *
 * ------------------------------------------------------------------ */
static uint8_t out_buf[8192];
static int out_len;

static void capture_char(char c) {
    if (c == '\n' && out_len < (int)sizeof out_buf)
        out_buf[out_len++] = '\r';
    if (out_len < (int)sizeof out_buf)
        out_buf[out_len++] = (uint8_t)c;
}

static int channel_send_data(ssh_conn_t *sc, uint32_t peer_channel, const uint8_t *data, int len) {
    static uint8_t payload[9 + SSH_MAX_DATA_CHUNK];
    int off = 0;
    do {
        int chunk = len - off;
        if (chunk > SSH_MAX_DATA_CHUNK)
            chunk = SSH_MAX_DATA_CHUNK;
        int n = 0;
        payload[n++] = SSH_MSG_CHANNEL_DATA;
        n += wr_u32(payload + n, peer_channel);
        n += wr_string(payload + n, data + off, chunk);
        if (send_packet(sc, payload, n) < 0)
            return -1;
        off += chunk;
    } while (off < len);
    return 0;
}

static void flush_output(ssh_conn_t *sc, uint32_t peer_channel) {
    if (out_len > 0) {
        channel_send_data(sc, peer_channel, out_buf, out_len);
        out_len = 0;
    }
}

static void reply_channel_success(ssh_conn_t *sc, uint32_t peer_channel) {
    uint8_t p[5];
    p[0] = SSH_MSG_CHANNEL_SUCCESS;
    wr_u32(p + 1, peer_channel);
    send_packet(sc, p, 5);
}

static void reply_channel_failure(ssh_conn_t *sc, uint32_t peer_channel) {
    uint8_t p[5];
    p[0] = SSH_MSG_CHANNEL_FAILURE;
    wr_u32(p + 1, peer_channel);
    send_packet(sc, p, 5);
}

static void channel_send_eof_close(ssh_conn_t *sc, uint32_t peer_channel) {
    uint8_t p[5];
    p[0] = SSH_MSG_CHANNEL_EOF;
    wr_u32(p + 1, peer_channel);
    send_packet(sc, p, 5);
    p[0] = SSH_MSG_CHANNEL_CLOSE;
    wr_u32(p + 1, peer_channel);
    send_packet(sc, p, 5);
}

/** @brief Reply to a global request if it wants one; otherwise ignore it. */
static void handle_global_request(ssh_conn_t *sc, const uint8_t *payload, int paylen) {
    reader_t r = { payload + 1, paylen - 1, 0 };
    uint32_t l;
    (void)rd_string(&r, &l);
    uint8_t want_reply = rd_byte(&r);
    if (want_reply) {
        uint8_t resp[1] = { SSH_MSG_REQUEST_FAILURE };
        send_packet(sc, resp, 1);
    }
}

/** @brief Wait for the client to open a "session" channel and request either
 *  "shell" or "exec". @return 0 with @p peer_channel_out / @p is_exec_out /
 *  @p exec_cmd_out filled, or -1 if the client gave up first. */
static int wait_for_session_channel(ssh_conn_t *sc, uint32_t *peer_channel_out,
                                    int *is_exec_out, char *exec_cmd_out, int exec_cmd_max) {
    int channel_open = 0;
    uint32_t peer_channel = 0;

    while (1) {
        static uint8_t payload[SSH_MAX_PACKET];
        int paylen;
        if (recv_packet(sc, payload, &paylen, sizeof payload) < 0)
            return -1;
        if (paylen < 1)
            continue;
        uint8_t type = payload[0];

        if (type == SSH_MSG_CHANNEL_OPEN) {
            reader_t r = { payload + 1, paylen - 1, 0 };
            uint32_t tlen;
            const uint8_t *tname = rd_string(&r, &tlen);
            uint32_t sender_channel = rd_u32(&r);
            (void)rd_u32(&r);
            (void)rd_u32(&r);

            if (channel_open || tlen != 7 || !bytes_eq(tname, (const uint8_t *)"session", 7)) {
                uint8_t resp[32];
                int n = 0;
                resp[n++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
                n += wr_u32(resp + n, sender_channel);
                n += wr_u32(resp + n, 2);
                n += wr_str_c(resp + n, "administratively prohibited");
                n += wr_str_c(resp + n, "");
                send_packet(sc, resp, n);
                continue;
            }

            peer_channel = sender_channel;
            channel_open = 1;
            uint8_t resp[17];
            int n = 0;
            resp[n++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
            n += wr_u32(resp + n, peer_channel);
            n += wr_u32(resp + n, 0);
            n += wr_u32(resp + n, 1u << 20);
            n += wr_u32(resp + n, SSH_MAX_DATA_CHUNK);
            send_packet(sc, resp, n);

        } else if (type == SSH_MSG_CHANNEL_REQUEST && channel_open) {
            reader_t r = { payload + 1, paylen - 1, 0 };
            (void)rd_u32(&r);
            uint32_t tlen;
            const uint8_t *tname = rd_string(&r, &tlen);
            uint8_t want_reply = rd_byte(&r);

            if (tlen == 5 && bytes_eq(tname, (const uint8_t *)"shell", 5)) {
                if (want_reply) reply_channel_success(sc, peer_channel);
                *is_exec_out = 0;
                *peer_channel_out = peer_channel;
                return 0;
            } else if (tlen == 4 && bytes_eq(tname, (const uint8_t *)"exec", 4)) {
                uint32_t clen;
                const uint8_t *cmd = rd_string(&r, &clen);
                if ((int)clen >= exec_cmd_max)
                    clen = (uint32_t)(exec_cmd_max - 1);
                memcpy(exec_cmd_out, (void *)cmd, clen);
                exec_cmd_out[clen] = 0;
                if (want_reply) reply_channel_success(sc, peer_channel);
                *is_exec_out = 1;
                *peer_channel_out = peer_channel;
                return 0;
            } else {
                if (want_reply) reply_channel_success(sc, peer_channel);
            }

        } else if (type == SSH_MSG_GLOBAL_REQUEST) {
            handle_global_request(sc, payload, paylen);
        } else if (type == SSH_MSG_CHANNEL_CLOSE || type == SSH_MSG_CHANNEL_EOF) {
            return -1;
        }
        /* ignore anything else (e.g. window-adjust before a channel exists) */
    }
}

static void send_exit_status(ssh_conn_t *sc, uint32_t peer_channel, uint32_t status) {
    uint8_t p[32];
    int n = 0;
    p[n++] = SSH_MSG_CHANNEL_REQUEST;
    n += wr_u32(p + n, peer_channel);
    n += wr_str_c(p + n, "exit-status");
    p[n++] = 0;
    n += wr_u32(p + n, status);
    send_packet(sc, p, n);
}

static void run_exec(ssh_conn_t *sc, uint32_t peer_channel, char *cmd) {
    out_len = 0;
    ssh_output_hook = capture_char;
    console_exec(cmd);
    ssh_output_hook = 0;
    flush_output(sc, peer_channel);

    send_exit_status(sc, peer_channel, 0);

    channel_send_eof_close(sc, peer_channel);
}

static void run_shell(ssh_conn_t *sc, uint32_t peer_channel) {
    static char cmdbuf[256];
    int i = 0;

    out_len = 0;
    ssh_output_hook = capture_char;
    printf("\nkoppi's hobby OS -- console over ssh\n> ");
    flush_output(sc, peer_channel);

    while (1) {
        static uint8_t payload[SSH_MAX_PACKET];
        int paylen;
        if (recv_packet(sc, payload, &paylen, sizeof payload) < 0)
            break;
        if (paylen < 1)
            continue;
        uint8_t type = payload[0];

        if (type == SSH_MSG_CHANNEL_DATA) {
            reader_t r = { payload + 1, paylen - 1, 0 };
            (void)rd_u32(&r);
            uint32_t dlen;
            const uint8_t *data = rd_string(&r, &dlen);

            for (uint32_t k = 0; k < dlen; k++) {
                uint8_t d = data[k];
                if (d == '\r' || d == '\n') {
                    cmdbuf[i] = 0;
                    printf("\n");
                    if (i > 0 && (strcmp(cmdbuf, "exit") == 0 || strcmp(cmdbuf, "logout") == 0)) {
                        ssh_output_hook = 0;
                        flush_output(sc, peer_channel);
                        send_exit_status(sc, peer_channel, 0);
                        channel_send_eof_close(sc, peer_channel);
                        return;
                    }
                    if (i > 0)
                        console_exec(cmdbuf);
                    i = 0;
                    printf("> ");
                } else if (d == 0x08 || d == 0x7F) {
                    if (i > 0) {
                        i--;
                        printf("\b \b");
                    }
                } else if (d >= 0x20 && d < 0x7F && i < (int)sizeof(cmdbuf) - 1) {
                    cmdbuf[i++] = (char)d;
                    printf("%c", (char)d);
                }
            }
            flush_output(sc, peer_channel);

        } else if (type == SSH_MSG_CHANNEL_EOF || type == SSH_MSG_CHANNEL_CLOSE) {
            break;
        } else if (type == SSH_MSG_CHANNEL_REQUEST) {
            reader_t r = { payload + 1, paylen - 1, 0 };
            (void)rd_u32(&r);
            uint32_t tlen;
            (void)rd_string(&r, &tlen);
            uint8_t want_reply = rd_byte(&r);
            if (want_reply)
                reply_channel_failure(sc, peer_channel);
        } else if (type == SSH_MSG_GLOBAL_REQUEST) {
            handle_global_request(sc, payload, paylen);
        }
        /* ignore CHANNEL_WINDOW_ADJUST and anything else */
    }
    ssh_output_hook = 0;
}

/* ------------------------------------------------------------------ *
 *  Session entry point                                                *
 * ------------------------------------------------------------------ */
static int recv_version(int h, char *out, int maxlen) {
    int n = 0;
    while (n < maxlen - 1) {
        char c;
        if (recv_exact(h, (uint8_t *)&c, 1) < 0)
            return -1;
        if (c == '\n')
            break;
        if (c != '\r')
            out[n++] = c;
    }
    out[n] = 0;
    return n;
}

static void ssh_serve_connection(int h) {
    ssh_conn_t sc;
    memset(&sc, 0, sizeof sc);
    sc.h = h;

    ensure_hostkey();

    static const char VS[] = "SSH-2.0-koppiOS_1.0";
    char vs_line[32];
    int vs_line_len = snprintf(vs_line, sizeof vs_line, "%s\r\n", VS);
    if (send_all(h, (const uint8_t *)vs_line, vs_line_len) < 0)
        goto done;

    static char vc[256];
    if (recv_version(h, vc, sizeof vc) < 0)
        goto done;
    if (strncmp(vc, "SSH-2.0", 7) != 0 && strncmp(vc, "SSH-1.99", 8) != 0)
        goto done;

    static uint8_t i_s[512];
    int i_s_len = build_kexinit(i_s);
    if (send_packet(&sc, i_s, i_s_len) < 0)
        goto done;

    static uint8_t i_c[SSH_MAX_PACKET];
    int i_c_len;
    if (recv_packet(&sc, i_c, &i_c_len, sizeof i_c) < 0)
        goto done;
    if (i_c_len < 1 || i_c[0] != SSH_MSG_KEXINIT || !verify_kexinit_algos(i_c, i_c_len))
        goto done;

    static uint8_t ecdh_init[SSH_MAX_PACKET];
    int ecdh_init_len;
    if (recv_packet(&sc, ecdh_init, &ecdh_init_len, sizeof ecdh_init) < 0)
        goto done;
    if (ecdh_init_len < 1 || ecdh_init[0] != SSH_MSG_KEX_ECDH_INIT)
        goto done;

    reader_t r = { ecdh_init + 1, ecdh_init_len - 1, 0 };
    uint32_t qc_len;
    const uint8_t *q_c = rd_string(&r, &qc_len);
    if (qc_len != 32)
        goto done;

    uint8_t eph_priv[32];
    csprng_bytes(eph_priv, 32);
    uint8_t q_s[32];
    curve25519_x25519(q_s, eph_priv, curve25519_base_u);
    /* The shared secret's raw octets (as X25519 outputs them) are used
     * directly as the "big-endian" mpint for the exchange hash / KDF -- SSH
     * implementations (e.g. paramiko's kex_curve25519.py) do NOT byte-swap
     * this value despite X25519's own little-endian convention. */
    uint8_t shared[32];
    curve25519_x25519(shared, eph_priv, q_c);

    uint8_t H[32];
    compute_exchange_hash(vc, VS, i_c, i_c_len, i_s, i_s_len, q_c, q_s, shared, H);

    uint8_t sig[64];
    ed25519_sign(hk_scalar, hk_prefix, hk_pub, H, 32, sig);

    static uint8_t reply[300];
    int reply_len = build_ecdh_reply(reply, q_s, sig);
    if (send_packet(&sc, reply, reply_len) < 0)
        goto done;

    uint8_t newkeys[1] = { SSH_MSG_NEWKEYS };
    if (send_packet(&sc, newkeys, 1) < 0)
        goto done;
    derive_keys(&sc, shared, H, H /* session_id == H on the first kex */);
    sc.enc_tx = 1;

    static uint8_t nk[16];
    int nk_len;
    if (recv_packet(&sc, nk, &nk_len, sizeof nk) < 0)
        goto done;
    if (nk_len < 1 || nk[0] != SSH_MSG_NEWKEYS)
        goto done;
    sc.enc_rx = 1;

    if (do_userauth(&sc) < 0)
        goto done;

    {
        uint32_t peer_channel;
        int is_exec;
        static char exec_cmd[256];
        if (wait_for_session_channel(&sc, &peer_channel, &is_exec, exec_cmd, sizeof exec_cmd) < 0)
            goto done;
        if (is_exec)
            run_exec(&sc, peer_channel, exec_cmd);
        else
            run_shell(&sc, peer_channel);
    }

done:
    ssh_output_hook = 0;
    tcp_close(h);
}

/* ------------------------------------------------------------------ *
 *  net-thread entry point                                             *
 * ------------------------------------------------------------------ */
void ssh_tick(void) {
    if (!net_is_up())
        return;

    if (listen_h < 0) {
        listen_h = tcp_listen(SSH_PORT);
        if (listen_h < 0)
            return;
        klogf(LOG_INFO, "ssh: listening on :%d\n", SSH_PORT);
    }

    int h = tcp_accept(listen_h);
    if (h < 0)
        return;

    session_count++;
    ssh_serve_connection(h);
}
