/**
 * @file nfs.c
 * @brief In-kernel NFSv4.1 client (RFC 8881) over the minimal TCP stack.
 *
 * Layering:
 *   - a tiny XDR cursor (px.. encode / gx.. decode helpers, big-endian,
 *     4-byte aligned);
 *   - ONC-RPC record marking + AUTH_SYS credential over one persistent TCP
 *     connection to port 2049;
 *   - one SEQUENCE-wrapped COMPOUND per VFS operation, single session slot;
 *   - EXCHANGE_ID / CREATE_SESSION / RECLAIM_COMPLETE session bring-up, redone
 *     automatically when the server reports a dead/bad session.
 *
 * All socket traffic happens on the `net` thread: @ref nfs_boot_tick runs in
 * @ref net_thread directly, console-driven calls are marshalled through
 * @ref net_exec by the nfs_vfs_* wrappers.
 */
#include <nfs.h>
#include <vfs.h>
#include <net.h>
#include <tcp.h>
#include <dns.h>

#include <io.h>
#include <pit.h>
#include <log.h>
#include <printf.h>
#include <lib/string.h>
#include "cpu.h"

/* ------------------------------------------------------------------ *
 *  NFSv4 protocol constants                                           *
 * ------------------------------------------------------------------ */
#define OP_ACCESS 3
#define OP_CLOSE 4
#define OP_COMMIT 5
#define OP_GETATTR 9
#define OP_GETFH 10
#define OP_LOOKUP 15
#define OP_OPEN 18
#define OP_PUTFH 22
#define OP_PUTROOTFH 24
#define OP_READ 25
#define OP_READDIR 26
#define OP_REMOVE 28
#define OP_WRITE 38
#define OP_EXCHANGE_ID 42
#define OP_CREATE_SESSION 43
#define OP_SEQUENCE 53
#define OP_RECLAIM_COMPLETE 58

#define NFS4_OK 0
#define NFS4ERR_NOENT 2

#define NF4REG 1
#define NF4DIR 2

#define FATTR4_TYPE   1
#define FATTR4_SIZE   4
#define FATTR4_FILEID 20
#define FATTR4_MODE   33

#define OPEN4_SHARE_ACCESS_READ  1
#define OPEN4_SHARE_ACCESS_BOTH  3
#define OPEN4_SHARE_ACCESS_WANT_NO_DELEG 0x200
#define OPEN4_NOCREATE 0
#define OPEN4_CREATE   1
#define UNCHECKED4     0
#define CLAIM_NULL     0

#define WRITE_FILE_SYNC4 2

/* ------------------------------------------------------------------ *
 *  XDR cursor                                                         *
 * ------------------------------------------------------------------ */
typedef struct { uint8_t *b; int cap; int pos; int err; } xdr_t;

/**
 * @brief Append a big-endian uint32_t.
 *
 * On overflow it sets the cursor's sticky @c err and writes nothing. None of
 * the encode helpers is checked at the call site: a COMPOUND is built
 * optimistically and @ref co_send refuses to transmit if @c err ended up set.
 */
static void px32(xdr_t *x, uint32_t v) {
    if (x->pos + 4 > x->cap) { x->err = 1; return; }
    x->b[x->pos++] = v >> 24; x->b[x->pos++] = v >> 16;
    x->b[x->pos++] = v >> 8;  x->b[x->pos++] = v;
}
/** @brief Append a big-endian uint64_t, as XDR requires: high word first. */
static void px64(xdr_t *x, uint64_t v) {
    px32(x, (uint32_t)(v >> 32)); px32(x, (uint32_t)v);
}
/**
 * @brief Append @p n raw bytes, zero-padded to the next 4-byte boundary.
 *
 * This is XDR's fixed-length opaque: no length prefix. Use @ref popaque when
 * the length has to go on the wire.
 */
static void pbytes(xdr_t *x, const void *p, int n) {
    int pad = (4 - (n & 3)) & 3;
    if (n < 0 || x->pos + n + pad > x->cap) { x->err = 1; return; }
    memcpy(x->b + x->pos, (void *)p, n);
    x->pos += n;
    while (pad--) x->b[x->pos++] = 0;
}
/** @brief Append a variable-length opaque: a uint32_t length, then the padded bytes. */
static void popaque(xdr_t *x, const void *p, int n) { px32(x, (uint32_t)n); pbytes(x, p, n); }
/** @brief Append a NUL-terminated string as an XDR opaque, without the NUL. */
static void pstr(xdr_t *x, const char *s) { popaque(x, s, strlen(s)); }
/**
 * @brief Overwrite the uint32_t at byte @p off, for a length filled in later.
 *
 * Used for the RPC record mark, the AUTH_SYS body length and the COMPOUND
 * operation count. @p off is assumed to be inside an already-written region,
 * so unlike @ref px32 this does not check the cursor's capacity.
 */
static void patch32(xdr_t *x, int off, uint32_t v) {
    x->b[off] = v >> 24; x->b[off + 1] = v >> 16;
    x->b[off + 2] = v >> 8; x->b[off + 3] = v;
}

/**
 * @brief Decode a big-endian uint32_t.
 * @return The value, or 0 with the cursor's sticky @c err set if the buffer
 *         ran out — so a 0 return is not by itself an error.
 */
static uint32_t gx32(xdr_t *x) {
    if (x->pos + 4 > x->cap) { x->err = 1; return 0; }
    uint32_t v = ((uint32_t)x->b[x->pos] << 24) | ((uint32_t)x->b[x->pos + 1] << 16) |
                 ((uint32_t)x->b[x->pos + 2] << 8) | x->b[x->pos + 3];
    x->pos += 4;
    return v;
}
/** @brief Decode a big-endian uint64_t as two words, high first. */
static uint64_t gx64(xdr_t *x) { uint64_t h = gx32(x); return (h << 32) | gx32(x); }
/**
 * @brief Consume @p n bytes of fixed-length opaque.
 * @param x   Cursor.
 * @param dst Destination, or 0 to skip the field without copying it.
 * @param n   Byte count; callers pass a multiple of 4, so no padding is read.
 */
static void graw(xdr_t *x, uint8_t *dst, int n) {          /* fixed array, n%4==0 */
    if (x->pos + n > x->cap) { x->err = 1; return; }
    if (dst) memcpy(dst, x->b + x->pos, n);
    x->pos += n;
}
/**
 * @brief Consume a variable-length opaque and optionally copy it out.
 * @param x   Cursor.
 * @param dst Destination, or 0 to skip the field.
 * @param max Capacity of @p dst.
 * @return The length the server declared, or -1 on a decode error.
 *
 * The cursor always advances past the padded field, but the copy happens only
 * if the value fits — so a return greater than @p max means @p dst was left
 * untouched, and callers that care (@ref nfs_resolve with a file handle) must
 * compare the result against their buffer size rather than trust it.
 */
static int gopaque(xdr_t *x, uint8_t *dst, int max) {
    uint32_t n = gx32(x);
    if (x->err) return -1;
    int m = (n + 3) & ~3u;
    if (x->pos + m > x->cap) { x->err = 1; return -1; }
    if (dst && (int)n <= max) memcpy(dst, x->b + x->pos, n);
    x->pos += m;
    return (int)n;
}

/* ------------------------------------------------------------------ *
 *  Connection + session state                                         *
 * ------------------------------------------------------------------ */
typedef struct { uint8_t data[128]; uint32_t len; } nfs_fh_t;
typedef struct { uint32_t type; uint64_t size; uint32_t mode; uint64_t fileid; } nfs_attr_t;

static int      nfs_conn = -1;
static int      nfs_mounted;
static int      establishing;
static uint32_t g_xid = 0x6b6f7000u;   /* "kop\0" */

static uint64_t nfs_clientid;
static uint32_t nfs_exch_seqid;
static uint8_t  nfs_sessionid[16];
static uint32_t slot_seq = 1;
static nfs_fh_t nfs_root_fh;

/* Path components between the NFSv4 pseudo-root (PUTROOTFH) and the export.
 * Empty when the server exports "/nfs" with fsid=0 (pseudo-root IS the export);
 * ["nfs"] when the server exports "/" with "nfs" as a real subdirectory.
 * Determined once by nfs_probe_export(). */
static char exp_comp[4][64];
static int  exp_n;

static uint8_t callbuf[16384] __attribute__((aligned(4)));
static uint8_t replybuf[16384] __attribute__((aligned(4)));
static xdr_t   X, R;
static int     co_opcnt_off, co_nop;

/* open-file table: file.current_cluster indexes this */
#define NFS_MAX_OPEN 8
static struct nofile {
    int      used;
    nfs_fh_t fh;
    uint64_t off, size;
    uint8_t  stateid[16];
    int      wrote;
} noft[NFS_MAX_OPEN];

/* ------------------------------------------------------------------ *
 *  RPC framing                                                        *
 * ------------------------------------------------------------------ */
/**
 * @brief Append an AUTH_SYS credential: uid 0, gid 0, no aux groups.
 *
 * The body length is back-patched once the contents are known. Exports
 * mapping root (root_squash) will therefore see this client as nobody.
 */
static void auth_sys(xdr_t *x) {
    px32(x, 1);                        /* AUTH_SYS */
    int lenoff = x->pos; px32(x, 0);   /* body length, patched below */
    int s = x->pos;
    px32(x, 0);                        /* stamp */
    pstr(x, "koppi-os");               /* machinename */
    px32(x, 0);                        /* uid */
    px32(x, 0);                        /* gid */
    px32(x, 0);                        /* aux gids: count 0 */
    patch32(x, lenoff, (uint32_t)(x->pos - s));
}

/**
 * @brief Start a new COMPOUND in @ref callbuf: RPC header, then minorversion 1.
 * @param xid_out Receives the XID assigned to this call, for @ref co_send.
 *
 * Leaves a placeholder for the record mark and for the operation count, both
 * patched at send time, and resets the operation counter. There is one call
 * buffer, so only one COMPOUND may be under construction at a time — which the
 * single @ref nfs_busy lock guarantees.
 */
static void co_begin(uint32_t *xid_out) {
    X.b = callbuf; X.cap = sizeof callbuf; X.pos = 0; X.err = 0;
    px32(&X, 0);                       /* record mark placeholder */
    uint32_t xid = ++g_xid;
    if (xid_out) *xid_out = xid;
    px32(&X, xid);
    px32(&X, 0);                       /* msg_type CALL */
    px32(&X, 2);                       /* rpcvers */
    px32(&X, 100003);                  /* prog: NFS */
    px32(&X, 4);                       /* vers */
    px32(&X, 1);                       /* proc: COMPOUND */
    auth_sys(&X);
    px32(&X, 0); px32(&X, 0);          /* verf: AUTH_NONE, length 0 */
    px32(&X, 0);                       /* COMPOUND tag "" */
    px32(&X, 1);                       /* minorversion 1 */
    co_opcnt_off = X.pos; px32(&X, 0); /* op count, patched at send */
    co_nop = 0;
}

/** @brief Append an operation number and count it for the COMPOUND's
 *         back-patched op count. */
static void op(uint32_t n) { px32(&X, n); co_nop++; }

/**
 * @brief Read exactly @p n bytes from the NFS connection.
 * @return 0 once @p n bytes are in @p buf, -1 on error or if the peer stalls.
 *
 * @c tcp_recv() returns 0 when nothing has arrived yet rather than blocking,
 * so a few empty reads are tolerated; five in a row are treated as a dead
 * connection, which is what breaks a stuck mount out of a spin.
 */
static int recv_all(uint8_t *buf, int n) {
    int got = 0, zeros = 0;
    while (got < n) {
        int r = tcp_recv(nfs_conn, buf + got, n - got);
        if (r > 0) { got += r; zeros = 0; }
        else if (r == 0) { if (++zeros > 4) return -1; }
        else return -1;
    }
    return 0;
}

/**
 * @brief Frame + send the assembled COMPOUND, read the whole RPC reply back
 *        into @ref replybuf and position @ref R past the RPC/COMPOUND headers.
 * @return the COMPOUND status, or -1 on a transport/format error.
 */
static int co_send(uint32_t xid) {
    patch32(&X, co_opcnt_off, (uint32_t)co_nop);
    patch32(&X, 0, 0x80000000u | (uint32_t)(X.pos - 4));
    if (X.err || nfs_conn < 0) return -1;
    if (tcp_send(nfs_conn, callbuf, X.pos) != X.pos) return -1;

    int off = 0;
    for (;;) {
        uint8_t mk[4];
        if (recv_all(mk, 4) < 0) return -1;
        uint32_t mark = ((uint32_t)mk[0] << 24) | ((uint32_t)mk[1] << 16) |
                        ((uint32_t)mk[2] << 8) | mk[3];
        int flen = (int)(mark & 0x7fffffffu);
        if (flen < 0 || off + flen > (int)sizeof replybuf) return -1;
        if (recv_all(replybuf + off, flen) < 0) return -1;
        off += flen;
        if (mark & 0x80000000u) break;
    }

    R.b = replybuf; R.cap = off; R.pos = 0; R.err = 0;
    if (gx32(&R) != xid) return -1;
    if (gx32(&R) != 1) return -1;       /* REPLY */
    uint32_t reply_stat = gx32(&R);
    if (reply_stat != 0) {             /* MSG_DENIED */
        uint32_t rej = gx32(&R);
        uint32_t why = gx32(&R);
        klogf(LOG_WARNING, "nfs: RPC denied (reject=%u code=%u) - server "
              "likely rejects this client's address or auth\n", rej, why);
        return -1;
    }
    gx32(&R);                           /* verf flavor */
    gopaque(&R, 0, 0);                  /* verf body */
    uint32_t astat = gx32(&R);
    if (astat != 0) {                   /* accept_stat != SUCCESS */
        klogf(LOG_WARNING, "nfs: RPC accept_stat=%u (1=PROG_UNAVAIL 2=PROG_MISMATCH "
              "3=PROC_UNAVAIL 4=GARBAGE_ARGS)\n", astat);
        return -1;
    }
    uint32_t cstat = gx32(&R);          /* COMPOUND status */
    gopaque(&R, 0, 0);                  /* COMPOUND tag */
    gx32(&R);                           /* resarray count */
    if (R.err) return -1;
    return (int)cstat;
}

/** @brief Read one nfs_resop4 header: opnum + per-op nfsstat4. */
static int res_op(uint32_t *opn, uint32_t *st) {
    uint32_t o = gx32(&R), s = gx32(&R);
    if (R.err) return -1;
    if (opn) *opn = o;
    if (st) *st = s;
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Attribute helpers                                                  *
 * ------------------------------------------------------------------ */
/**
 * @brief Append the GETATTR bitmap this client asks for.
 *
 * Two words: type, size and fileid live in word 0, mode is attribute 33 and
 * so lives in word 1. @ref parse_attrs decodes exactly these.
 */
static void put_attr_request(xdr_t *x) {
    px32(x, 2);
    px32(x, (1u << FATTR4_TYPE) | (1u << FATTR4_SIZE) | (1u << FATTR4_FILEID));
    px32(x, (1u << (FATTR4_MODE - 32)));
}

/**
 * @brief Append the attributes an OPEN with CREATE sets: mode 0644 only.
 *
 * Word 0 of the bitmap is empty; only the word-1 mode bit is set. Size is
 * left to the server's default of 0.
 */
static void put_create_attrs(xdr_t *x) {
    px32(x, 2);
    px32(x, 0);
    px32(x, (1u << (FATTR4_MODE - 32)));
    px32(x, 4);                         /* attrlist length: one u32 */
    px32(x, 0644);
}

/**
 * @brief Decode a fattr4 into @p a.
 * @return 0 on success, -1 on a decode error.
 *
 * Reads only the four attributes @ref put_attr_request asks for, in the
 * bitmap order XDR mandates, then jumps to the end of the declared attribute
 * list — so a server that returns more than was asked for does not desync the
 * reply stream. Anything not returned is left zero.
 */
static int parse_attrs(nfs_attr_t *a) {
    memset(a, 0, sizeof *a);
    uint32_t bmlen = gx32(&R), bm0 = 0, bm1 = 0;
    for (uint32_t i = 0; i < bmlen; i++) {
        uint32_t w = gx32(&R);
        if (i == 0) bm0 = w; else if (i == 1) bm1 = w;
    }
    uint32_t alen = gx32(&R);
    if (R.err) return -1;
    int end = R.pos + (int)((alen + 3) & ~3u);
    if (bm0 & (1u << FATTR4_TYPE))   a->type   = gx32(&R);
    if (bm0 & (1u << FATTR4_SIZE))   a->size   = gx64(&R);
    if (bm0 & (1u << FATTR4_FILEID)) a->fileid = gx64(&R);
    if (bm1 & (1u << (FATTR4_MODE - 32))) a->mode = gx32(&R);
    if (R.err || end > R.cap) return -1;
    R.pos = end;                        /* skip anything not decoded */
    return 0;
}

/** @brief Consume one nfsace4 (type, flag, access mask, who) without decoding it. */
static void skip_ace(void) { gx32(&R); gx32(&R); gx32(&R); gopaque(&R, 0, 0); }

/* Skip an open_delegation4 in the reply stream.
 * NB: Linux knfsd encodes OPEN_DELEGATE_NONE_EXT as 3, not the RFC 8881 value 4;
 * accept both. */
static void skip_delegation(void) {
    uint32_t dt = gx32(&R);
    if (dt == 1) {                      /* OPEN_DELEGATE_READ */
        graw(&R, 0, 16); gx32(&R); skip_ace();
    } else if (dt == 2) {               /* OPEN_DELEGATE_WRITE */
        graw(&R, 0, 16); gx32(&R);
        uint32_t limitby = gx32(&R);
        if (limitby == 1) gx64(&R); else { gx32(&R); gx32(&R); }
        skip_ace();
    } else if (dt == 3 || dt == 4) {    /* OPEN_DELEGATE_NONE_EXT */
        uint32_t why = gx32(&R);
        if (why == 1 || why == 2) gx32(&R);   /* CONTENTION/RESOURCE: + bool */
    }
    /* dt == 0: OPEN_DELEGATE_NONE — nothing follows */
}

/* ------------------------------------------------------------------ *
 *  Session bring-up                                                   *
 * ------------------------------------------------------------------ */
/**
 * @brief Append the channel attributes for CREATE_SESSION.
 *
 * Requests a single slot (maxrequests 1), which is why the whole client needs
 * only the one @ref slot_seq counter and one in-flight COMPOUND. The 8 KiB
 * request/response limits are well inside @ref callbuf and @ref replybuf.
 */
static void put_chan_attrs(xdr_t *x) {
    px32(x, 0);        /* headerpad */
    px32(x, 8192);     /* maxrequestsize */
    px32(x, 8192);     /* maxresponsesize */
    px32(x, 4096);     /* maxresponsesize_cached */
    px32(x, 8);        /* maxoperations */
    px32(x, 1);        /* maxrequests: single slot */
    px32(x, 0);        /* rdma_ird<>: none */
}
/** @brief Consume a channel_attrs4 from the reply: six words plus the rdma list. */
static void skip_chan_attrs(void) {
    for (int i = 0; i < 6; i++) gx32(&R);
    uint32_t n = gx32(&R);
    for (uint32_t i = 0; i < n; i++) gx32(&R);
}

/**
 * @brief EXCHANGE_ID: introduce this client and learn its clientid.
 * @return 0 on success, -1 on any transport or protocol failure.
 *
 * Sent outside a SEQUENCE — there is no session yet. The verifier is the TSC,
 * so a reboot presents a new one and the server discards the old client
 * record instead of refusing the owner id as already in use. SP4_NONE: no
 * state protection.
 */
static int do_exchange_id(void) {
    uint32_t xid;
    co_begin(&xid);
    op(OP_EXCHANGE_ID);
    px64(&X, rdtsc());                  /* co_verifier */
    pstr(&X, "koppi-os-nfs-client");    /* co_ownerid */
    px32(&X, 0);                        /* eia_flags */
    px32(&X, 0);                        /* state_protect: SP4_NONE */
    px32(&X, 0);                        /* client_impl_id<>: none */
    if (co_send(xid) < 0) return -1;

    uint32_t o, s;
    if (res_op(&o, &s) || o != OP_EXCHANGE_ID || s != NFS4_OK) return -1;
    nfs_clientid    = gx64(&R);
    nfs_exch_seqid  = gx32(&R);
    gx32(&R);                           /* eir_flags */
    gx32(&R);                           /* spr_how (SP4_NONE -> void) */
    gx64(&R);                           /* so_minor_id */
    gopaque(&R, 0, 0);                  /* so_major_id */
    gopaque(&R, 0, 0);                  /* server_scope */
    uint32_t nimpl = gx32(&R);
    for (uint32_t i = 0; i < nimpl; i++) {
        gopaque(&R, 0, 0); gopaque(&R, 0, 0); gx64(&R); gx32(&R);
    }
    return R.err ? -1 : 0;
}

/**
 * @brief CREATE_SESSION: turn the clientid into a session id.
 * @return 0 on success, -1 otherwise.
 *
 * Also sent outside a SEQUENCE. The back channel is described but no callback
 * program is registered (cb_program 0), so the server cannot recall a
 * delegation — which is why OPEN always asks for none.
 */
static int do_create_session(void) {
    uint32_t xid;
    co_begin(&xid);
    op(OP_CREATE_SESSION);
    px64(&X, nfs_clientid);
    px32(&X, nfs_exch_seqid);
    px32(&X, 0);                        /* csa_flags */
    put_chan_attrs(&X);                 /* fore channel */
    put_chan_attrs(&X);                 /* back channel */
    px32(&X, 0);                        /* cb_program */
    px32(&X, 1); px32(&X, 0);           /* sec_parms<1>: AUTH_NONE */
    if (co_send(xid) < 0) return -1;

    uint32_t o, s;
    if (res_op(&o, &s) || o != OP_CREATE_SESSION || s != NFS4_OK) return -1;
    graw(&R, nfs_sessionid, 16);
    gx32(&R);                           /* csr_sequence */
    gx32(&R);                           /* csr_flags */
    skip_chan_attrs();
    skip_chan_attrs();
    return R.err ? -1 : 0;
}

/**
 * @brief Append the SEQUENCE that must lead every post-bring-up COMPOUND.
 *
 * Always slot 0 with @ref slot_seq as the sequence id, and cachethis false:
 * with one slot there is no reply cache to populate. @ref co_exchange
 * advances @ref slot_seq only after the server accepts the SEQUENCE.
 */
static void put_sequence(void) {
    op(OP_SEQUENCE);
    pbytes(&X, nfs_sessionid, 16);
    px32(&X, slot_seq);
    px32(&X, 0);                        /* slotid */
    px32(&X, 0);                        /* highest slotid */
    px32(&X, 0);                        /* cachethis = false */
}

/** @brief Parse "a.b.c.d" into a host-order address. @return 1 on success. */
static int parse_dotted_quad(const char *s, uint32_t *out) {
    uint32_t v = 0;
    int parts = 0, cur = 0, digits = 0;
    for (const char *p = s;; p++) {
        if (*p >= '0' && *p <= '9') {
            cur = cur * 10 + (*p - '0');
            if (cur > 255) return 0;
            digits = 1;
        } else if (*p == '.' || *p == 0) {
            if (!digits) return 0;
            v = (v << 8) | (uint32_t)cur;
            parts++;
            cur = digits = 0;
            if (*p == 0) break;
        } else {
            return 0;
        }
    }
    if (parts != 4) return 0;
    *out = v;
    return 1;
}

/**
 * @brief Is @p s an nfsstat4 that means "the session is gone, re-establish"?
 * @return Non-zero for BADSESSION, DEADSESSION, BADSLOT, SEQ_MISORDERED and
 *         STALE_CLIENTID — the codes a server returns after it forgot this
 *         client, typically because it restarted.
 */
static int is_sess_err(uint32_t s) {
    return s == 10052 || s == 10048 || s == 10051 || s == 10022 || s == 10011;
}

/**
 * @brief Send the current COMPOUND and consume its leading SEQUENCE result.
 * @return 0 on success (slot advanced), -1 on a hard error, -2 if the caller
 *         should re-establish the session and retry.
 */
static int co_exchange(uint32_t xid) {
    if (co_send(xid) < 0) return -2;
    uint32_t o, s;
    if (res_op(&o, &s) || o != OP_SEQUENCE) return -1;
    if (s != NFS4_OK) return is_sess_err(s) ? -2 : -1;
    graw(&R, 0, 16);                    /* sessionid */
    gx32(&R); gx32(&R); gx32(&R); gx32(&R); gx32(&R);
    if (R.err) return -1;
    slot_seq++;
    return 0;
}

static int nfs_lookup_root(nfs_fh_t *fh, nfs_attr_t *a);   /* fwd */
static void nfs_probe_export(void);                        /* fwd */

static int nfs_establish(void) {
    nfs_mounted = 0;
    if (nfs_conn >= 0) { tcp_close(nfs_conn); nfs_conn = -1; }

    uint32_t addrs[4];
    int n_addrs = 0;
    uint32_t dotted;
    if (parse_dotted_quad(NFS_HOST, &dotted)) {
        addrs[n_addrs++] = dotted;
    } else {
        /* A stale/duplicate DNS answer (e.g. an old DHCP lease the name
         * server hasn't expired yet) shouldn't wedge the mount - try every
         * address returned, not just the first. */
        n_addrs = dns_resolve(NFS_HOST, addrs, 4);
    }
    if (n_addrs < 1) {
        klogf(LOG_WARNING, "nfs: cannot resolve %s\n", NFS_HOST);
        return -1;
    }

    /* Reserved (<1024, for a "secure" export) but varies per attempt: a fixed
     * port would collide with a lingering server-side socket from a prior
     * connection that never got a clean FIN (e.g. the client rebooted or the
     * link dropped mid-session) - the server answers a fresh SYN on that
     * 4-tuple with a bare challenge ACK instead of SYN-ACK, and the mount
     * just hangs retransmitting forever. */
    uint16_t lport = (uint16_t)(512 + (rdtsc() % 512));

    int h = -1;
    for (int i = 0; i < n_addrs && h < 0; i++)
        h = tcp_connect_lport(addrs[i], NFS_PORT, lport);
    if (h < 0) { klogf(LOG_WARNING, "nfs: connect to %s:%d failed (%d address(es) tried)\n", NFS_HOST, NFS_PORT, n_addrs); return -1; }
    nfs_conn = h;

    establishing = 1;
    slot_seq = 1;
    int rc = -1;
    if (do_exchange_id() != 0) {
        klogf(LOG_WARNING, "nfs: EXCHANGE_ID failed\n");
    } else if (do_create_session() != 0) {
        klogf(LOG_WARNING, "nfs: CREATE_SESSION failed\n");
    } else {
        /* RECLAIM_COMPLETE (best effort but knfsd wants it before use). */
        uint32_t xid;
        co_begin(&xid);
        put_sequence();
        op(OP_RECLAIM_COMPLETE);
        px32(&X, 0);                    /* rca_one_fs = FALSE */
        if (co_exchange(xid) != 0) {
            klogf(LOG_WARNING, "nfs: RECLAIM_COMPLETE failed\n");
        } else {
            uint32_t o, s;
            res_op(&o, &s);             /* ignore RECLAIM_COMPLETE status */
            nfs_attr_t a;
            nfs_probe_export();
            rc = nfs_lookup_root(&nfs_root_fh, &a);
            if (rc != 0)
                klogf(LOG_WARNING, "nfs: LOOKUP %s failed (exp_n=%d)\n",
                      NFS_EXPORT, exp_n);
        }
    }
    establishing = 0;

    if (rc == 0) nfs_mounted = 1;
    else if (nfs_conn >= 0) { tcp_close(nfs_conn); nfs_conn = -1; }
    return rc;
}

/**
 * @brief Establish the session if it is not up.
 * @return 0 if a session is usable, -1 if bring-up failed.
 *
 * Returns success while @ref establishing is set: the bring-up path itself
 * issues COMPOUNDs, and without this guard those calls would recurse back
 * into @ref nfs_establish.
 */
static int ensure_session(void) {
    if (nfs_mounted || establishing) return 0;
    return nfs_establish();
}

/* ------------------------------------------------------------------ *
 *  Path resolution                                                    *
 * ------------------------------------------------------------------ */
#define NFS_MAXCOMP 24

/** @brief Split a VFS path ("/nfs/a/b", "nfs/a/b", "/nfs") into components,
 *         dropping the leading '/' and the "nfs" mount component. */
static int split_path(const char *path, char comp[][64]) {
    const char *p = path;
    while (*p == '/') p++;
    if (strncmp((char *)p, NFS_MOUNTPOINT, 3) == 0) p += 3;
    int n = 0;
    while (*p && n < NFS_MAXCOMP) {
        while (*p == '/') p++;
        if (!*p || *p == ' ') break;
        int k = 0;
        while (*p && *p != '/' && *p != ' ' && k < 63) comp[n][k++] = *p++;
        comp[n][k] = 0;
        n++;
    }
    return n;
}

/**
 * @brief One COMPOUND: SEQUENCE, PUTROOTFH, LOOKUP exp_comp[i].., LOOKUP
 *        comp[i].., GETFH, GETATTR. Fills @p fh / @p a for the last component.
 */
static int nfs_resolve(char comp[][64], int n, nfs_fh_t *fh, nfs_attr_t *a) {
    for (int att = 0; att < 2; att++) {
        if (ensure_session() != 0) return -1;

        uint32_t xid;
        co_begin(&xid);
        put_sequence();
        op(OP_PUTROOTFH);
        for (int i = 0; i < exp_n; i++) { op(OP_LOOKUP); pstr(&X, exp_comp[i]); }
        for (int i = 0; i < n; i++)     { op(OP_LOOKUP); pstr(&X, comp[i]); }
        op(OP_GETFH);
        op(OP_GETATTR); put_attr_request(&X);

        int r = co_exchange(xid);
        if (r == -2) { nfs_mounted = 0; continue; }
        if (r != 0) return -1;

        uint32_t o, s;
        if (res_op(&o, &s) || s != NFS4_OK) {
            if (establishing)
                klogf(LOG_WARNING, "nfs: PUTROOTFH nfsstat=%u (13=ACCESS "
                      "10062=WRONGSEC 10001=PERM) - client not authorised for "
                      "the export\n", s);
            return -1;
        }
        for (int i = 0; i < exp_n; i++)
            if (res_op(&o, &s) || s != NFS4_OK) return -1;      /* LOOKUP export */
        for (int i = 0; i < n; i++)
            if (res_op(&o, &s) || s != NFS4_OK) return -1;      /* LOOKUP comp */
        if (res_op(&o, &s) || s != NFS4_OK) {
            if (establishing) klogf(LOG_WARNING, "nfs: GETFH nfsstat=%u\n", s);
            return -1;
        }
        int fhl = gopaque(&R, fh->data, sizeof fh->data);
        if (fhl < 0 || fhl > (int)sizeof fh->data) return -1;
        fh->len = (uint32_t)fhl;
        if (res_op(&o, &s) || s != NFS4_OK) {
            if (establishing) klogf(LOG_WARNING, "nfs: GETATTR nfsstat=%u\n", s);
            return -1;
        }
        return parse_attrs(a);
    }
    return -1;
}

/**
 * @brief Fetch the export root's file handle and attributes.
 * @param fh Receives the handle.
 * @param a  Receives the attributes.
 * @return 0 on success.
 *
 * @ref nfs_resolve with zero path components, so it stops at whatever
 * @ref nfs_probe_export decided the export root is.
 */
static int nfs_lookup_root(nfs_fh_t *fh, nfs_attr_t *a) {
    char comp[NFS_MAXCOMP][64];
    return nfs_resolve(comp, 0, fh, a);
}

/**
 * @brief Work out how many components sit between PUTROOTFH and the export.
 *
 * With the pseudo-root already at the export (fsid=0) that is zero; with the
 * export mount name ("nfs") as a real subdirectory of the pseudo-root it is one.
 */
static void nfs_probe_export(void) {
    char comp[NFS_MAXCOMP][64];
    nfs_fh_t fh;
    nfs_attr_t a;

    exp_n = 0;
    strncpy(comp[0], NFS_MOUNTPOINT, sizeof comp[0] - 1);
    comp[0][sizeof comp[0] - 1] = 0;
    if (nfs_resolve(comp, 1, &fh, &a) == 0 && a.type == NF4DIR) {
        strncpy(exp_comp[0], NFS_MOUNTPOINT, sizeof exp_comp[0] - 1);
        exp_comp[0][sizeof exp_comp[0] - 1] = 0;
        exp_n = 1;
    }
}

/* ------------------------------------------------------------------ *
 *  Data operations (each a single SEQUENCE-wrapped COMPOUND)          *
 * ------------------------------------------------------------------ */
static int nfs_do_open(char comp[][64], int n, int create, int wr,
                       nfs_fh_t *fh_out, nfs_attr_t *a_out, uint8_t sid_out[16]) {
    nfs_fh_t parent;
    nfs_attr_t pa;
    if (n < 1 || nfs_resolve(comp, n - 1, &parent, &pa) != 0) return -1;

    for (int att = 0; att < 2; att++) {
        if (ensure_session() != 0) return -1;

        uint32_t xid;
        co_begin(&xid);
        put_sequence();
        op(OP_PUTFH); popaque(&X, parent.data, parent.len);
        op(OP_OPEN);
        px32(&X, 0);                                     /* open seqid (4.1: 0) */
        px32(&X, (wr ? OPEN4_SHARE_ACCESS_BOTH : OPEN4_SHARE_ACCESS_READ)
                     | OPEN4_SHARE_ACCESS_WANT_NO_DELEG);
        px32(&X, 0);                                     /* share_deny NONE */
        px64(&X, nfs_clientid);                          /* open_owner: clientid */
        pstr(&X, "koppi-os-open");                       /* open_owner: owner */
        if (create) {
            px32(&X, OPEN4_CREATE);
            px32(&X, UNCHECKED4);
            put_create_attrs(&X);
        } else {
            px32(&X, OPEN4_NOCREATE);
        }
        px32(&X, CLAIM_NULL);
        pstr(&X, comp[n - 1]);
        op(OP_GETFH);
        op(OP_GETATTR); put_attr_request(&X);

        int r = co_exchange(xid);
        if (r == -2) { nfs_mounted = 0; continue; }
        if (r != 0) return -1;

        uint32_t o, s;
        if (res_op(&o, &s) || s != NFS4_OK) return -1;   /* PUTFH */
        if (res_op(&o, &s) || s != NFS4_OK) return -1;   /* OPEN */
        graw(&R, sid_out, 16);                           /* open stateid */
        gx32(&R); gx64(&R); gx64(&R);                    /* change_info4 */
        gx32(&R);                                        /* rflags */
        { uint32_t bl = gx32(&R); for (uint32_t i = 0; i < bl; i++) gx32(&R); }
        skip_delegation();
        if (R.err) return -1;
        if (res_op(&o, &s) || s != NFS4_OK) return -1;   /* GETFH */
        int fhl = gopaque(&R, fh_out->data, sizeof fh_out->data);
        if (fhl < 0 || fhl > (int)sizeof fh_out->data) return -1;
        fh_out->len = (uint32_t)fhl;
        if (res_op(&o, &s) || s != NFS4_OK) return -1;   /* GETATTR */
        return parse_attrs(a_out);
    }
    return -1;
}

/**
 * @brief CLOSE an open stateid.
 * @param fh  The file's handle.
 * @param sid The stateid OPEN returned.
 * @return 0 if the COMPOUND reached the server, -1 otherwise.
 *
 * CLOSE's own status is read but ignored: the local slot is dropped either
 * way, and a server that has already forgotten the state is not a failure the
 * caller can do anything about. Unlike the other operations this does not
 * retry on a dead session — a re-established session has no stateid to close.
 */
static int nfs_do_close(nfs_fh_t *fh, uint8_t sid[16]) {
    if (ensure_session() != 0) return -1;
    uint32_t xid;
    co_begin(&xid);
    put_sequence();
    op(OP_PUTFH); popaque(&X, fh->data, fh->len);
    op(OP_CLOSE);
    px32(&X, 0);                        /* seqid */
    pbytes(&X, sid, 16);
    if (co_exchange(xid) != 0) return -1;
    uint32_t o, s;
    if (res_op(&o, &s) || s != NFS4_OK) return -1;   /* PUTFH */
    if (res_op(&o, &s)) return -1;                   /* CLOSE (status ignored) */
    return 0;
}

static int nfs_do_read(nfs_fh_t *fh, uint8_t sid[16], uint64_t off,
                       uint32_t cnt, uint8_t *buf, int *eof) {
    for (int att = 0; att < 2; att++) {
        if (ensure_session() != 0) return -1;
        uint32_t xid;
        co_begin(&xid);
        put_sequence();
        op(OP_PUTFH); popaque(&X, fh->data, fh->len);
        op(OP_READ);
        pbytes(&X, sid, 16);
        px64(&X, off);
        px32(&X, cnt);

        int r = co_exchange(xid);
        if (r == -2) { nfs_mounted = 0; continue; }
        if (r != 0) return -1;

        uint32_t o, s;
        if (res_op(&o, &s) || s != NFS4_OK) return -1;   /* PUTFH */
        if (res_op(&o, &s) || s != NFS4_OK) return -1;   /* READ */
        *eof = (int)gx32(&R);
        int dl = gopaque(&R, buf, (int)cnt);
        /* Same trap as in nfs_do_readdir(): more data than was asked for means
         * gopaque() copied nothing, so clamping the count here would have
         * reported a full buffer of uninitialised memory as file content.
         * A server returning more than the requested count is out of spec;
         * fail the read. */
        if (dl < 0 || dl > (int)cnt || R.err) return -1;
        return dl;
    }
    return -1;
}

static int nfs_do_write(nfs_fh_t *fh, uint8_t sid[16], uint64_t off,
                        const uint8_t *data, int len) {
    if (ensure_session() != 0) return -1;
    uint32_t xid;
    co_begin(&xid);
    put_sequence();
    op(OP_PUTFH); popaque(&X, fh->data, fh->len);
    op(OP_WRITE);
    pbytes(&X, sid, 16);
    px64(&X, off);
    px32(&X, WRITE_FILE_SYNC4);
    popaque(&X, data, len);
    if (co_exchange(xid) != 0) return -1;
    uint32_t o, s;
    if (res_op(&o, &s) || s != NFS4_OK) return -1;       /* PUTFH */
    if (res_op(&o, &s) || s != NFS4_OK) return -1;       /* WRITE */
    uint32_t wrote = gx32(&R);
    gx32(&R);                                            /* committed */
    graw(&R, 0, 8);                                      /* writeverf */
    return R.err ? -1 : (int)wrote;
}

/**
 * @brief REMOVE @p name from the directory @p parent.
 * @return 0 on success, -1 on any failure, including "no such file".
 */
static int nfs_do_remove(nfs_fh_t *parent, const char *name) {
    if (ensure_session() != 0) return -1;
    uint32_t xid;
    co_begin(&xid);
    put_sequence();
    op(OP_PUTFH); popaque(&X, parent->data, parent->len);
    op(OP_REMOVE); pstr(&X, name);
    if (co_exchange(xid) != 0) return -1;
    uint32_t o, s;
    if (res_op(&o, &s) || s != NFS4_OK) return -1;       /* PUTFH */
    if (res_op(&o, &s) || s != NFS4_OK) return -1;       /* REMOVE */
    return 0;
}

/**
 * @brief READDIR a directory and print its entries to the console.
 *
 * Pages through the directory with the cookie and cookie verifier the server
 * returns, asking for the type attribute so directories can be marked with a
 * trailing '/'. Capped at 64 pages so a pathological or looping server cannot
 * hold the net thread forever; prints "(empty)" if nothing was listed. There
 * is no machine-readable form of this — @ref vfs_listdir deliberately returns
 * 0 for NFS paths.
 */
static void nfs_do_readdir(nfs_fh_t *dir) {
    uint8_t verf[8];
    uint64_t cookie = 0;
    int have_verf = 0, printed = 0;

    for (int page = 0; page < 64; page++) {
        if (ensure_session() != 0) { printf("nfs: readdir failed\n"); return; }
        uint32_t xid;
        co_begin(&xid);
        put_sequence();
        op(OP_PUTFH); popaque(&X, dir->data, dir->len);
        op(OP_READDIR);
        px64(&X, cookie);
        if (have_verf) pbytes(&X, verf, 8); else { px32(&X, 0); px32(&X, 0); }
        px32(&X, 2048);                 /* dircount */
        px32(&X, 4096);                 /* maxcount */
        px32(&X, 1); px32(&X, (1u << FATTR4_TYPE));

        int r = co_exchange(xid);
        if (r == -2) { nfs_mounted = 0; continue; }
        if (r != 0) { printf("nfs: readdir error\n"); return; }

        uint32_t o, s;
        if (res_op(&o, &s) || s != NFS4_OK) return;      /* PUTFH */
        if (res_op(&o, &s) || s != NFS4_OK) return;      /* READDIR */
        graw(&R, verf, 8); have_verf = 1;

        int any = 0;
        for (;;) {
            uint32_t present = gx32(&R);
            if (R.err) return;
            if (!present) break;
            uint64_t ck = gx64(&R);
            char nm[256];
            int nl = gopaque(&R, (uint8_t *)nm, (int)sizeof nm - 1);
            if (nl < 0) return;
            nfs_attr_t a;
            if (parse_attrs(&a)) return;
            cookie = ck;
            any = 1;
            printed = 1;
            /* gopaque() consumes the field either way but copies it only if
             * it fits, so an over-long name left nm untouched - printing it
             * would show uninitialised stack. The entry is still consumed, so
             * the reply stream stays in sync and only the name is lost. A
             * server should never send one: NFS caps a component at 255. */
            if (nl > (int)sizeof nm - 1) {
                printf("(name too long)%s  ", a.type == NF4DIR ? "/" : "");
                continue;
            }
            nm[nl] = 0;
            printf("%s%s  ", nm, a.type == NF4DIR ? "/" : "");
        }
        uint32_t eof = gx32(&R);
        if (R.err) return;
        if (eof || !any) break;
    }
    if (!printed) printf("(empty)");
}

/* ------------------------------------------------------------------ *
 *  net-thread task dispatch                                           *
 * ------------------------------------------------------------------ */
enum { NREQ_LS, NREQ_CD, NREQ_OPEN, NREQ_READ, NREQ_WRITE, NREQ_CLOSE,
       NREQ_TOUCH, NREQ_DELETE };

static struct {
    int   op;
    char  path[256];
    char  mode[4];
    file  f;
    char *buf;
    int   rc;
} NR;

static volatile int nfs_busy;
/** @brief Take the client lock guarding the single @c NR request block,
 *         sleeping 20 ms between attempts rather than spinning. */
static void nfs_acquire(void) { while (__sync_lock_test_and_set(&nfs_busy, 1)) sleep(20); }
/** @brief Release the client lock taken by @ref nfs_acquire. */
static void nfs_release(void) { __sync_lock_release(&nfs_busy); }

/**
 * @brief NREQ_OPEN on the net thread: resolve, then OPEN if it is a file.
 *
 * A bare mount point or an existing directory comes back as @c FS_DIR without
 * consuming an open slot. A real file takes a slot in @ref noft, and the
 * slot index is stored in @c file::current_cluster — the field FAT uses for
 * its start cluster, reused here because @c file has nowhere else to put it.
 * @ref NFS_MAX_OPEN in that field means "no slot", so a file that failed to
 * open, or opened when all eight slots were busy, reads back as @c FS_NULL.
 */
static void task_open(void) {
    char comp[NFS_MAXCOMP][64];
    int n = split_path(NR.path, comp);
    int create = (strcmp(NR.mode, "w") == 0);

    file f;
    memset(&f, 0, sizeof f);
    f.dev = NFS_DEV_ID;
    f.current_cluster = NFS_MAX_OPEN;   /* "no slot" until an OPEN succeeds */
    strncpy(f.name, n ? comp[n - 1] : (char *)NFS_MOUNTPOINT, sizeof f.name - 1);

    nfs_fh_t fh;
    nfs_attr_t a;
    uint8_t sid[16];

    if (n == 0) {
        f.type = (nfs_resolve(comp, 0, &fh, &a) == 0) ? FS_DIR : FS_NULL;
        NR.f = f;
        return;
    }
    if (!create && nfs_resolve(comp, n, &fh, &a) == 0 && a.type == NF4DIR) {
        f.type = FS_DIR;
        f.len = (uint32_t)a.size;
        NR.f = f;
        return;
    }
    if (nfs_do_open(comp, n, create, create, &fh, &a, sid) != 0) {
        f.type = FS_NULL;
        NR.f = f;
        return;
    }
    int sl = -1;
    for (int i = 0; i < NFS_MAX_OPEN; i++)
        if (!noft[i].used) { sl = i; break; }
    if (sl < 0) { f.type = FS_NULL; NR.f = f; return; }

    noft[sl].used = 1;
    noft[sl].fh   = fh;
    noft[sl].off  = 0;
    noft[sl].size = create ? 0 : a.size;
    noft[sl].wrote = 0;
    memcpy(noft[sl].stateid, sid, 16);

    f.type = FS_FILE;
    f.len  = (uint32_t)noft[sl].size;
    f.current_cluster = (uint32_t)sl;
    NR.f = f;
}

/**
 * @brief NREQ_READ on the net thread: fill one 512-byte block from the file.
 *
 * The VFS read interface has no length, so the buffer is always treated as
 * 512 bytes and zero-filled first. Sets @c f->eof at the server's EOF, on a
 * short read, or once the offset reaches the size recorded at open — so a
 * file another client grows while it is open is not followed.
 */
static void task_read(void) {
    file *f = &NR.f;
    memset(NR.buf, 0, 512);
    uint32_t sl = f->current_cluster;
    if (sl >= NFS_MAX_OPEN || !noft[sl].used) { f->eof = 1; return; }
    struct nofile *of = &noft[sl];
    if (of->off >= of->size) { f->eof = 1; return; }

    uint8_t tmp[512];
    int eof = 0;
    int dl = nfs_do_read(&of->fh, of->stateid, of->off, 512, tmp, &eof);
    if (dl < 0) { f->eof = 1; return; }
    memcpy(NR.buf, tmp, dl);
    of->off += (uint64_t)dl;
    if (eof || dl == 0 || of->off >= of->size) f->eof = 1;
}

/**
 * @brief NREQ_WRITE on the net thread: append a string at the file offset.
 *
 * The length comes from @c strlen(), so this writes text only: an embedded
 * NUL truncates the write. WRITE_FILE_SYNC4 means the server has committed
 * the data before it replies, so there is no COMMIT to follow.
 */
static void task_write(void) {
    file *f = &NR.f;
    uint32_t sl = f->current_cluster;
    if (sl >= NFS_MAX_OPEN || !noft[sl].used) return;
    struct nofile *of = &noft[sl];
    int len = strlen(NR.buf);
    int w = nfs_do_write(&of->fh, of->stateid, of->off, (const uint8_t *)NR.buf, len);
    if (w > 0) {
        of->off += (uint64_t)w;
        of->wrote = 1;
        if (of->off > of->size) of->size = of->off;
        f->len = (uint32_t)of->size;
    }
}

/**
 * @brief NREQ_CLOSE on the net thread: CLOSE the stateid and free the slot.
 *
 * Tolerates a handle with no slot (a directory, or a failed open), and always
 * marks the handle @c FS_NULL so a double close is harmless.
 */
static void task_close(void) {
    file *f = &NR.f;
    uint32_t sl = f->current_cluster;
    if (sl < NFS_MAX_OPEN && noft[sl].used) {
        nfs_do_close(&noft[sl].fh, noft[sl].stateid);
        noft[sl].used = 0;
    }
    f->type = FS_NULL;
}

/**
 * @brief NREQ_LS on the net thread: list a directory to the console.
 *
 * Reports "not found" or "not a directory" itself; the console has no other
 * channel for the error, since the VFS ls hook returns void.
 */
static void task_ls(void) {
    char comp[NFS_MAXCOMP][64];
    int n = split_path(NR.path, comp);
    nfs_fh_t fh;
    nfs_attr_t a;
    if (nfs_resolve(comp, n, &fh, &a) != 0) { printf("nfs: %s: not found\n", NR.path); NR.rc = -1; return; }
    if (a.type != NF4DIR) { printf("nfs: %s: not a directory\n", NR.path); NR.rc = -1; return; }
    nfs_do_readdir(&fh);
    printf("\n");
}

/**
 * @brief NREQ_CD on the net thread: test whether a path is a directory.
 *
 * Returns a handle of type @c FS_DIR or @c FS_NULL; nothing is opened, so no
 * slot is consumed and no close is needed.
 */
static void task_cd(void) {
    char comp[NFS_MAXCOMP][64];
    int n = split_path(NR.path, comp);
    file f;
    memset(&f, 0, sizeof f);
    f.dev = NFS_DEV_ID;
    nfs_fh_t fh;
    nfs_attr_t a;
    if (nfs_resolve(comp, n, &fh, &a) == 0 && a.type == NF4DIR) f.type = FS_DIR;
    else f.type = FS_NULL;
    NR.f = f;
}

/**
 * @brief NREQ_TOUCH on the net thread: create a file, then close it again.
 *
 * OPEN with CREATE/UNCHECKED4, so an existing file is left as it is and still
 * reports success, matching touch(1). Sets @c NR.rc to 1 on success, 0 on
 * failure — the VFS convention, the opposite of the 0-is-success used inside
 * this file.
 */
static void task_touch(void) {
    char comp[NFS_MAXCOMP][64];
    int n = split_path(NR.path, comp);
    if (n < 1) { NR.rc = 0; return; }
    nfs_fh_t fh;
    nfs_attr_t a;
    uint8_t sid[16];
    if (nfs_do_open(comp, n, 1, 1, &fh, &a, sid) != 0) { NR.rc = 0; return; }
    nfs_do_close(&fh, sid);
    NR.rc = 1;
}

/**
 * @brief NREQ_DELETE on the net thread: resolve the parent, then REMOVE.
 *
 * Sets @c NR.rc to 1 on success, 0 on failure.
 */
static void task_delete(void) {
    char comp[NFS_MAXCOMP][64];
    int n = split_path(NR.path, comp);
    if (n < 1) { NR.rc = 0; return; }
    nfs_fh_t parent;
    nfs_attr_t pa;
    if (nfs_resolve(comp, n - 1, &parent, &pa) != 0) { NR.rc = 0; return; }
    NR.rc = (nfs_do_remove(&parent, comp[n - 1]) == 0) ? 1 : 0;
}

/**
 * @brief The single entry point @c net_exec() runs on the net thread.
 * @return @c NR.rc, which only the touch and delete paths set.
 *
 * Every nfs_vfs_* wrapper fills in @c NR and routes through here, so all
 * socket traffic stays on the one thread that owns the TCP stack.
 */
static int nfs_task(void) {
    NR.rc = 0;
    switch (NR.op) {
    case NREQ_LS:     task_ls();     break;
    case NREQ_CD:     task_cd();     break;
    case NREQ_OPEN:   task_open();   break;
    case NREQ_READ:   task_read();   break;
    case NREQ_WRITE:  task_write();  break;
    case NREQ_CLOSE:  task_close();  break;
    case NREQ_TOUCH:  task_touch();  break;
    case NREQ_DELETE: task_delete(); break;
    }
    return NR.rc;
}

/* ------------------------------------------------------------------ *
 *  Public API                                                         *
 * ------------------------------------------------------------------ */
/** @brief Is the export mounted? @return Non-zero once bring-up succeeded. */
int nfs_is_mounted(void) { return nfs_mounted; }

/**
 * @brief Does @p name fall under the NFS mount point?
 * @param name A VFS path, with or without a leading slash.
 * @return Non-zero for "/nfs", "nfs/x" and the like, 0 otherwise.
 *
 * The component must end at a '/', a space or the end of the string, so
 * "nfsroot" does not match. vfs.c consults this before the device table, so
 * a real device called "nfs" would be shadowed.
 */
int nfs_owns_path(const char *name) {
    const char *p = name;
    if (!p) return 0;
    while (*p == '/') p++;
    if (strncmp((char *)p, NFS_MOUNTPOINT, 3) != 0) return 0;
    char c = p[3];
    return c == 0 || c == '/' || c == ' ';
}

/**
 * @brief One line of mount status for the console.
 * @return A static string; not a copy, so it must not be modified.
 */
const char *nfs_status_str(void) {
    return nfs_mounted
        ? "nfs: " NFS_HOST ":" NFS_EXPORT " mounted at /" NFS_MOUNTPOINT
        : "nfs: not mounted (auto-retry every 10s once the link is up)";
}

/**
 * @brief Try to mount the export; called repeatedly from @ref net_thread.
 *
 * Does nothing once mounted or while the link is down, and otherwise retries
 * at most every 10 seconds, so a server that is not there costs one connect
 * attempt per interval rather than a spin. Runs on the net thread already, so
 * unlike the nfs_vfs_* wrappers it calls @ref nfs_establish directly.
 */
void nfs_boot_tick(void) {
    static uint32_t next_ms;
    if (nfs_mounted || !net_is_up()) return;
    uint32_t now = pit_ms();
    if (next_ms && now < next_ms) return;
    next_ms = now + 10000;

    klogf(LOG_INFO, "nfs: mounting %s:%s ...\n", NFS_HOST, NFS_EXPORT);
    if (nfs_establish() == 0)
        klogf(LOG_INFO, "nfs: mounted %s:%s at /%s\n", NFS_HOST, NFS_EXPORT, NFS_MOUNTPOINT);
    else
        klogf(LOG_WARNING, "nfs: mount of %s:%s failed, will retry\n", NFS_HOST, NFS_EXPORT);
}

/**
 * @brief VFS hook: list an NFS directory to the console.
 * @param dir Device-qualified path under the mount point.
 */
void nfs_vfs_ls(char *dir) {
    nfs_acquire();
    NR.op = NREQ_LS;
    strncpy(NR.path, dir, sizeof NR.path - 1); NR.path[sizeof NR.path - 1] = 0;
    net_exec(nfs_task);
    nfs_release();
}

/**
 * @brief VFS hook: test whether an NFS path is a directory.
 * @param dir Device-qualified path.
 * @return A handle of type @c FS_DIR, or @c FS_NULL if it is not one.
 */
file nfs_vfs_cd(char *dir) {
    nfs_acquire();
    NR.op = NREQ_CD;
    strncpy(NR.path, dir, sizeof NR.path - 1); NR.path[sizeof NR.path - 1] = 0;
    net_exec(nfs_task);
    file f = NR.f;
    nfs_release();
    return f;
}

/**
 * @brief VFS hook: open an NFS file.
 * @param name Device-qualified path.
 * @param mode "w" creates and truncates; anything else opens for reading.
 * @return A handle, of type @c FS_NULL if the open failed.
 */
file nfs_vfs_open(char *name, const char *mode) {
    nfs_acquire();
    NR.op = NREQ_OPEN;
    strncpy(NR.path, name, sizeof NR.path - 1); NR.path[sizeof NR.path - 1] = 0;
    strncpy(NR.mode, (char *)mode, sizeof NR.mode - 1); NR.mode[sizeof NR.mode - 1] = 0;
    net_exec(nfs_task);
    file f = NR.f;
    nfs_release();
    return f;
}

/**
 * @brief VFS hook: read the next 512 bytes of an open NFS file.
 * @param f   Handle; its offset and @c eof flag are updated in place.
 * @param buf Destination, which must have room for 512 bytes.
 */
void nfs_vfs_read(file *f, char *buf) {
    nfs_acquire();
    NR.op = NREQ_READ; NR.f = *f; NR.buf = buf;
    net_exec(nfs_task);
    *f = NR.f;
    nfs_release();
}

/**
 * @brief VFS hook: append a NUL-terminated string to an open NFS file.
 * @param f   Handle; its length is updated in place.
 * @param str Text to write. Binary data with embedded NULs is truncated.
 */
void nfs_vfs_write(file *f, char *str) {
    nfs_acquire();
    NR.op = NREQ_WRITE; NR.f = *f; NR.buf = str;
    net_exec(nfs_task);
    *f = NR.f;
    nfs_release();
}

/**
 * @brief VFS hook: close an open NFS file and release its slot.
 * @param f Handle; marked @c FS_NULL on return.
 */
void nfs_vfs_close(file *f) {
    nfs_acquire();
    NR.op = NREQ_CLOSE; NR.f = *f;
    net_exec(nfs_task);
    *f = NR.f;
    nfs_release();
}

/**
 * @brief VFS hook: create an empty NFS file.
 * @param name Device-qualified path.
 * @return 1 on success, 0 on failure. An existing file counts as success.
 */
int nfs_vfs_touch(char *name) {
    nfs_acquire();
    NR.op = NREQ_TOUCH;
    strncpy(NR.path, name, sizeof NR.path - 1); NR.path[sizeof NR.path - 1] = 0;
    net_exec(nfs_task);
    int rc = NR.rc;
    nfs_release();
    return rc;
}

/**
 * @brief VFS hook: delete an NFS file.
 * @param name Device-qualified path.
 * @return 1 on success, 0 on failure.
 */
int nfs_vfs_delete(char *name) {
    nfs_acquire();
    NR.op = NREQ_DELETE;
    strncpy(NR.path, name, sizeof NR.path - 1); NR.path[sizeof NR.path - 1] = 0;
    net_exec(nfs_task);
    int rc = NR.rc;
    nfs_release();
    return rc;
}
