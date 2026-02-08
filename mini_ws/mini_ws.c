#include "mini_ws.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>      // tolower()

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef WS_MAX_SEND_FRAME
#define WS_MAX_SEND_FRAME (64u * 1024u * 1024u) // 64MB
#endif

// ===================== SHA1 (small) =====================

typedef struct {
    uint32_t h[5];
    uint64_t len_bits;
    uint8_t  buf[64];
    size_t   buf_len;
} sha1_ctx;

static uint32_t rol32(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

static void sha1_init(sha1_ctx* c) {
    c->h[0] = 0x67452301;
    c->h[1] = 0xEFCDAB89;
    c->h[2] = 0x98BADCFE;
    c->h[3] = 0x10325476;
    c->h[4] = 0xC3D2E1F0;
    c->len_bits = 0;
    c->buf_len = 0;
}

static void sha1_block(sha1_ctx* c, const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4 + 0] << 24) |
            ((uint32_t)block[i * 4 + 1] << 16) |
            ((uint32_t)block[i * 4 + 2] << 8) |
            ((uint32_t)block[i * 4 + 3] << 0);
    }
    for (int i = 16; i < 80; i++) w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & cc) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ cc ^ d;           k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDC; }
        else { f = b ^ cc ^ d;           k = 0xCA62C1D6; }
        uint32_t temp = rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = cc;
        cc = rol32(b, 30);
        b = a;
        a = temp;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}

static void sha1_update(sha1_ctx* c, const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    c->len_bits += (uint64_t)n * 8;

    while (n) {
        size_t take = 64 - c->buf_len;
        if (take > n) take = n;
        memcpy(c->buf + c->buf_len, p, take);
        c->buf_len += take;
        p += take;
        n -= take;
        if (c->buf_len == 64) {
            sha1_block(c, c->buf);
            c->buf_len = 0;
        }
    }
}

static void sha1_final(sha1_ctx* c, uint8_t out20[20]) {
    c->buf[c->buf_len++] = 0x80;
    if (c->buf_len > 56) {
        while (c->buf_len < 64) c->buf[c->buf_len++] = 0;
        sha1_block(c, c->buf);
        c->buf_len = 0;
    }
    while (c->buf_len < 56) c->buf[c->buf_len++] = 0;

    uint64_t bitlen = c->len_bits;
    for (int i = 7; i >= 0; i--) c->buf[c->buf_len++] = (uint8_t)((bitlen >> (i * 8)) & 0xFF);
    sha1_block(c, c->buf);

    for (int i = 0; i < 5; i++) {
        out20[i * 4 + 0] = (uint8_t)((c->h[i] >> 24) & 0xFF);
        out20[i * 4 + 1] = (uint8_t)((c->h[i] >> 16) & 0xFF);
        out20[i * 4 + 2] = (uint8_t)((c->h[i] >> 8) & 0xFF);
        out20[i * 4 + 3] = (uint8_t)((c->h[i] >> 0) & 0xFF);
    }
}


// ===================== Base64 =====================

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t base64_encode(const uint8_t* in, size_t in_len, char* out, size_t out_cap) {
    size_t needed = ((in_len + 2) / 3) * 4;
    if (out_cap < needed + 1) return 0;

    size_t o = 0;
    for (size_t i = 0; i < in_len; i += 3) {
        uint32_t v = 0;
        int rem = (int)(in_len - i);
        v |= (uint32_t)in[i] << 16;
        if (rem > 1) v |= (uint32_t)in[i + 1] << 8;
        if (rem > 2) v |= (uint32_t)in[i + 2];

        out[o++] = b64_table[(v >> 18) & 63];
        out[o++] = b64_table[(v >> 12) & 63];
        out[o++] = (rem > 1) ? b64_table[(v >> 6) & 63] : '=';
        out[o++] = (rem > 2) ? b64_table[v & 63] : '=';
    }
    out[o] = '\0';
    return o;
}

// ===================== Helpers =====================

static int set_reuseaddr(int fd) {
    int yes = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*) &yes, sizeof(yes));
}

static int wait_fd(int fd, int for_read, int max_usecs) {
    fd_set rfds, wfds;
    FD_ZERO(&rfds); FD_ZERO(&wfds);
    if (for_read) FD_SET(fd, &rfds); else FD_SET(fd, &wfds);

    struct timeval tv;
    struct timeval* ptv = NULL;
    if (max_usecs >= 0) {
        tv.tv_sec = max_usecs / 1000000;
        tv.tv_usec = max_usecs % 1000000;
        ptv = &tv;
    }

    int rc = select(fd + 1, for_read ? &rfds : NULL, for_read ? NULL : &wfds, NULL, ptv);
    return rc; // 0 timeout, >0 ready, <0 error
}

static size_t send_all(int fd, const uint8_t* buf, size_t len, int max_usecs) {
    size_t off = 0;
    while (off < len) {
        int w = wait_fd(fd, 0, max_usecs);
        if (w == 0) return -2; // timeout
        if (w < 0) return -1;

        size_t n = send(fd, buf + off, (int)(len - off), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
        if (n == 0) return -1;
        off += (size_t)n;
    }
    return off;
}

static void maybe_compact(WsConn* c) {
    if (!c || c->read_offset == 0) return;
    size_t avail = (c->read_buffer_size > c->read_offset) ? (c->read_buffer_size - c->read_offset) : 0;
    if (avail == 0) {
        c->read_buffer_size = 0;
        c->read_offset = 0;
        return;
    }
    // compact when offset grows (simple heuristic)
    if (c->read_offset >= (c->read_buffer_capacity / 2)) {
        memmove(c->read_buffer, c->read_buffer + c->read_offset, avail);
        c->read_buffer_size = avail;
        c->read_offset = 0;
    }
}

static int ensure_capacity(WsConn* c, size_t extra) {
    if (!c) return 0;
    size_t need = c->read_buffer_size + extra;
    if (need <= c->read_buffer_capacity) return 1;

    size_t newcap = c->read_buffer_capacity ? c->read_buffer_capacity : 4096;
    while (newcap < need) newcap *= 2;
    uint8_t* nb = (uint8_t*)realloc(c->read_buffer, newcap);
    if (!nb) return 0;
    c->read_buffer = nb;
    c->read_buffer_capacity = newcap;
    return 1;
}

static uint16_t read_be16(const uint8_t* p) { return (uint16_t)(p[0] << 8) | p[1]; }

// ===================== Handshake =====================

static const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// ASCII case-insensitive compare of n chars
static int ascii_ncasecmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        int da = tolower(ca);
        int db = tolower(cb);
        if (da != db) return da - db;
        if (ca == '\0') return 0;
    }
    return 0;
}

static int header_get_value(const char* headers, const char* name, char* out, size_t out_cap) {
    size_t name_len = strlen(name);
    const char* p = headers;

    while (*p) {
        const char* eol = strstr(p, "\r\n");
        if (!eol) break;

        // Empty line ends headers
        if (eol == p) break;

        // Find ':'
        const char* colon = memchr(p, ':', (size_t)(eol - p));
        if (colon) {
            // Header name is [p, colon)
            // Trim trailing spaces in name (rare, but)
            const char* name_end = colon;
            while (name_end > p && (name_end[-1] == ' ' || name_end[-1] == '\t'))
                name_end--;

            size_t hdr_name_len = (size_t)(name_end - p);
            if (hdr_name_len == name_len && ascii_ncasecmp(p, name, name_len) == 0) {
                // Value starts after colon, skip whitespace
                const char* v = colon + 1;
                while (v < eol && (*v == ' ' || *v == '\t')) v++;

                size_t vlen = (size_t)(eol - v);
                if (out_cap == 0) return 0;
                if (vlen >= out_cap) vlen = out_cap - 1;
                memcpy(out, v, vlen);
                out[vlen] = '\0';
                return 1;
            }
        }

        p = eol + 2;
    }
    return 0;
}


static int ws_make_accept(const char* client_key, char* out_b64, size_t out_cap) {
    char concat[256];
    size_t klen = strlen(client_key);
    size_t glen = strlen(WS_GUID);
    if (klen + glen >= sizeof(concat)) return 0;
    memcpy(concat, client_key, klen);
    memcpy(concat + klen, WS_GUID, glen);

    sha1_ctx c;
    uint8_t digest[20];
    sha1_init(&c);
    sha1_update(&c, concat, klen + glen);
    sha1_final(&c, digest);

    return base64_encode(digest, 20, out_b64, out_cap) != 0;
}


static int ws_do_server_handshake(int fd, int max_usecs) {
    // Read until \r\n\r\n (max 8KB)
    char req[8192];
    int used = 0;

    while( true ) {
        if (used >= (int)sizeof(req) - 1) return 0;

        int r = wait_fd(fd, 1, max_usecs);
        if (r == 0) 
            return 0; // timeout treated as failure for accept()
        if (r < 0) 
            return 0;

        int n = recv(fd, req + used, (size_t)((int)sizeof(req) - 1 - used), 0);
        if (n < 0) {
            if (errno == EINTR) 
                continue;
            return 0;
        }
        if (n == 0) 
            return 0;

        used += n;
        req[used] = '\0';
        if (strstr(req, "\r\n\r\n")) break;
    }

    char ws_key[256];
    if (!header_get_value(req, "Sec-WebSocket-Key", ws_key, sizeof(ws_key))) return 0;

    char accept[128];
    if (!ws_make_accept(ws_key, accept, sizeof(accept))) return 0;

    char resp[512];
    int resp_len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept);
    if (resp_len <= 0 || resp_len >= (int)sizeof(resp)) return 0;

    return send_all(fd, (const uint8_t*)resp, (size_t)resp_len, max_usecs) > 0;
}


// ===================== Server API =====================
static void ws_socket_close( int* fd ) {
#ifdef _WIN32
    closesocket(*fd);
#else
    close(*fd);
#endif
    *fd = -1;
}

static void ws_socket_shutdown_wr(int fd) {
#ifdef _WIN32
    shutdown((SOCKET)fd, SD_SEND);
#else
    shutdown(fd, SHUT_WR);
#endif
}

WsServer* ws_server_create(int port) {
    int fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return NULL;
    set_reuseaddr(fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ws_socket_close(&fd);
        return NULL;
    }
    if (listen(fd, 16) < 0) {
        ws_socket_close(&fd);
        return NULL;
    }

    WsServer* s = (WsServer*)calloc(1, sizeof(WsServer));
    if (!s) { ws_socket_close(&fd); return NULL; }
    s->fd = fd;
    return s;
}

// returns NULL on timeout or error
WsConn* ws_server_accept(WsServer* server, int max_usecs) {
    if (!server) return NULL;

    int w = wait_fd(server->fd, 1, max_usecs);
    if (w <= 0) return NULL;

    struct sockaddr_in cli;
    socklen_t clen = sizeof(cli);
    int cfd = (int)accept(server->fd, (struct sockaddr*)&cli, &clen);
    if (cfd < 0) return NULL;

    if (!ws_do_server_handshake(cfd, max_usecs)) {
        ws_socket_close(&cfd);
        return NULL;
    }

    WsConn* c = (WsConn*)calloc(1, sizeof(WsConn));
    if (!c) { ws_socket_close(&cfd); return NULL; }
    c->fd = cfd;
    c->is_client = false;
    c->is_connected = true;
    c->read_buffer = NULL;
    c->read_buffer_size = 0;
    c->read_buffer_capacity = 0;
    c->read_offset = 0;
    c->close_sent = false;
    c->close_received = false;
    return c;
}

void ws_server_destroy(WsServer* server) {
    if (!server) return;
    if (server->fd >= 0) ws_socket_close(&server->fd);
    free(server);
}


// ===================== Frame build/send =====================

static size_t ws_build_header(uint8_t* dst, size_t cap, uint8_t opcode, uint64_t len,
    int mask, uint8_t mask_key[4]) {
    if (cap < 2) return 0;
    size_t h = 0;

    dst[h++] = (uint8_t)(0x80 | (opcode & 0x0F)); // FIN=1

    if (len <= 125) {
        dst[h++] = (uint8_t)((mask ? 0x80 : 0x00) | (uint8_t)len);
    }
    else if (len <= 65535) {
        if (cap < h + 2 + 1) return 0;
        dst[h++] = (uint8_t)((mask ? 0x80 : 0x00) | 126);
        dst[h++] = (uint8_t)((len >> 8) & 0xFF);
        dst[h++] = (uint8_t)((len >> 0) & 0xFF);
    }
    else {
        if (cap < h + 8 + 1) return 0;
        dst[h++] = (uint8_t)((mask ? 0x80 : 0x00) | 127);
        // 64-bit big-endian length
        for (int i = 7; i >= 0; i--) dst[h++] = (uint8_t)((len >> (i * 8)) & 0xFF);
    }

    if (mask) {
        if (cap < h + 4) return 0;
        uint32_t r = (uint32_t)rand(); // replace if you care
        mask_key[0] = (uint8_t)((r >> 0) & 0xFF);
        mask_key[1] = (uint8_t)((r >> 8) & 0xFF);
        mask_key[2] = (uint8_t)((r >> 16) & 0xFF);
        mask_key[3] = (uint8_t)((r >> 24) & 0xFF);
        dst[h++] = mask_key[0];
        dst[h++] = mask_key[1];
        dst[h++] = mask_key[2];
        dst[h++] = mask_key[3];
    }

    return h;
}

static int ws_send_frame(WsConn* c, uint8_t opcode, const void* payload, size_t len) {
    if (!c || c->fd < 0) return 0;
    if (!c->is_connected) return 0;
    if (len > WS_MAX_SEND_FRAME) return 0;

    int mask = c->is_client ? 1 : 0;
    uint8_t header[14];
    uint8_t mask_key[4] = { 0 };

    size_t hlen = ws_build_header(header, sizeof(header), opcode, len, mask, mask_key);
    if (!hlen) return 0;

    // If masking, we must send masked payload. Avoid huge alloc: chunk it.
    if (send_all(c->fd, header, hlen, 1000000) < 0) return 0;

    if (!len) return 1;

    if (!mask) {
        return send_all(c->fd, payload, len, 1000000) == len;
    }
    else {
        // chunk-mask on the fly
        uint8_t tmp[4096];
        size_t off = 0;
        while (off < len) {
            size_t n = MIN(sizeof(tmp), len - off);
            memcpy(tmp, (uint8_t*)payload + off, n);
            for (size_t i = 0; i < n; i++) tmp[i] ^= mask_key[(off + i) & 3];
            if (send_all(c->fd, tmp, n, 1000000) < 0) return 0;
            off += n;
        }
        return 1;
    }
}

bool ws_conn_send_binary(WsConn* conn, const void* data, size_t len) {
    return ws_send_frame(conn, 0x2, data, len) != 0;
}

bool ws_conn_send_text(WsConn* conn, const char* data, size_t len) {
    if( len == 0 )
		len = strlen(data);
    return ws_send_frame(conn, 0x1, (const uint8_t*)data, len) != 0;
}

static void ws_send_close_best_effort(WsConn* c, uint16_t code) {
    if (!c || c->fd < 0) return;
    uint8_t payload[2];
    payload[0] = (uint8_t)((code >> 8) & 0xFF);
    payload[1] = (uint8_t)((code >> 0) & 0xFF);
    (void)ws_send_frame(c, 0x8, payload, 2);
}

static void ws_send_pong_best_effort(WsConn* c, const uint8_t* p, size_t n) {
    (void)ws_send_frame(c, 0xA, p, n);
}

// ===================== Conn lifecycle =====================

void ws_conn_destroy(WsConn* conn) {
    if (!conn) return;

    if (conn->fd >= 0) {
        if (conn->is_connected && !conn->close_sent) {
            ws_send_close_best_effort(conn, 1000);
            conn->close_sent = true;
        }
        ws_socket_shutdown_wr(conn->fd);
        ws_socket_close(&conn->fd);
    }

    free(conn->read_buffer);
    conn->read_buffer = NULL;
    conn->read_buffer_size = 0;
    conn->read_buffer_capacity = 0;
    conn->read_offset = 0;
    free(conn);
}

// ===================== Read / Parse =====================
// -1 -> error
//  0 -> no new data
//  1 -> new data recv
static int ws_conn_read(WsConn* conn, int max_usecs) {
    if (!conn || conn->fd < 0) 
        return -1;

    maybe_compact(conn);
    if (!ensure_capacity(conn, 4096)) 
        return -1;

    int rdy = wait_fd(conn->fd, 1, max_usecs);
    if (rdy == 0) 
        return 0;
    if (rdy < 0) 
        return -1;

    // append after read_buffer_size
    int n = recv(conn->fd,
        conn->read_buffer + conn->read_buffer_size,
        (int)( conn->read_buffer_capacity - conn->read_buffer_size ),
        0);
    if (n < 0) {
        if (errno == EINTR) 
            return 1;
        return -1;
    }
    if (n == 0) 
        return -1;

    conn->read_buffer_size += (size_t)n;
    return 1;
}

void ws_conn_handle_ping_pong(WsConn* conn, const uint8_t* payload, size_t payload_len) {
    ws_send_pong_best_effort(conn, payload, payload_len);
}

typedef enum {
	WS_NO_FRAME = 0,
	WS_TEXT = 1,
	WS_BINARY = 2,
	WS_CLOSE = 8,
	WS_PING = 9,
	WS_PONG = 10,
	WS_ERROR = -1
} WsOpcode;

WsOpcode ws_conn_parse_frame(WsConn* conn, const uint8_t** payload_data, size_t* payload_len) {
    if (payload_data) *payload_data = NULL;
    if (payload_len)  *payload_len = 0;
    if (!conn) return WS_ERROR;

    size_t avail = (conn->read_buffer_size > conn->read_offset)
        ? (conn->read_buffer_size - conn->read_offset)
        : 0;
    if (avail < 2) 
        return WS_NO_FRAME;

    const uint8_t* p = conn->read_buffer + conn->read_offset;
    uint8_t b0 = p[0];
    uint8_t b1 = p[1];

    uint8_t fin = (b0 >> 7) & 1;
    uint8_t rsv = (b0 >> 4) & 0x7;
    uint8_t opcode = b0 & 0x0F;

    uint8_t masked = (b1 >> 7) & 1;
    uint8_t plen7 = (b1 & 0x7F);

    if (rsv != 0) return WS_ERROR;
    if (!fin) return WS_ERROR;              // no fragmentation
    if (opcode == 0x0) return WS_ERROR;     // no continuation
    if (opcode == 0x3 || opcode == 0x4 || opcode == 0x5 || opcode == 0x6 || opcode == 0x7) return WS_ERROR;
    if (opcode > 0xA) return WS_ERROR;

    // If we are server side, client frames MUST be masked
    if (!conn->is_client && !masked) return WS_ERROR;

    size_t hdr = 2;
    size_t payload_length = 0;

    if (plen7 <= 125) {
        payload_length = plen7;
    }
    else if (plen7 == 126) {
        if (avail < hdr + 2) return WS_NO_FRAME;
        payload_length = read_be16(p + hdr);
        hdr += 2;
        if (payload_length > 65535) return WS_ERROR;
    }
    else {
        // 127 not supported in basic mode
        return WS_ERROR;
    }

    // control frames constraints
    if (opcode >= 0x8) {
        if (payload_length > 125) 
            return WS_ERROR;
    }

    uint8_t mask_key[4] = { 0 };
    if (masked) {
        if (avail < hdr + 4) 
            return WS_NO_FRAME;
        mask_key[0] = p[hdr + 0];
        mask_key[1] = p[hdr + 1];
        mask_key[2] = p[hdr + 2];
        mask_key[3] = p[hdr + 3];
        hdr += 4;
    }

    if (avail < hdr + payload_length) return WS_NO_FRAME;

    // Unmask in-place if needed (payload lives inside read_buffer)
    uint8_t* payload = (uint8_t*)(conn->read_buffer + conn->read_offset + hdr);
    if (masked) {
        for (size_t i = 0; i < payload_length; i++) 
            payload[i] ^= mask_key[i & 3];
    }

    // consume now (advance offset)
    conn->read_offset += hdr + payload_length;

    // expose payload for ALL opcodes (makes ping/pong easy)
    if (payload_data) *payload_data = payload;
    if (payload_len)  *payload_len = payload_length;

    // handle close bookkeeping + ping auto-pong? (we do not auto-pong here; caller can)
    if (opcode == 0x8) conn->close_received = true;

    switch (opcode) {
    case 0x1: return WS_TEXT;
    case 0x2: return WS_BINARY;
    case 0x8: return WS_CLOSE;
    case 0x9: return WS_PING;
    case 0xA: return WS_PONG;
    default:  return WS_ERROR;
    }
}

bool ws_conn_poll_event(WsConn** conn_ptr, WsEvent* out_evt, int max_usecs) {
    if (!conn_ptr || !*conn_ptr || !out_evt)
        return false;
    WsConn* conn = *conn_ptr;

    int rc = ws_conn_read(conn, max_usecs);
    if (rc < 0) {
        ws_conn_destroy(conn);
        *conn_ptr = NULL;
        out_evt->type = WS_EVT_CLOSED;
        out_evt->payload = NULL;
        out_evt->payload_len = 0;
        return true;
    }

    if (rc == 0) {
        out_evt->type = WS_EVT_NONE;
        out_evt->payload = NULL;
        out_evt->payload_len = 0;
        return false;
    }

    WsOpcode code = ws_conn_parse_frame(conn, &out_evt->payload, &out_evt->payload_len);
    if (code == WS_TEXT) {
        out_evt->type = WS_EVT_TEXT;
        return true;
    }
    if (code == WS_BINARY) {
        out_evt->type = WS_EVT_BINARY;
        return true;
    }
    if (code == WS_PING) {
        // No need to bother the client
        ws_conn_handle_ping_pong(conn, NULL, 0);
        return false;
    }
    if (code == WS_CLOSE || code == WS_ERROR) {
        ws_conn_destroy(conn);
        *conn_ptr = NULL;
        out_evt->type = WS_EVT_CLOSED;
        out_evt->payload = NULL;
        out_evt->payload_len = 0;
        return true;
    }

    return true;
}