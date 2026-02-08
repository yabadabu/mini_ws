#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// This library implements RFC6455 with these constraints:
// - No extensions: RSV must be 0
// - No fragmentation: FIN must be 1, opcode 0 (continuation) is rejected
// - Client->server frames must be masked; unmasked frames are protocol error
// - Payload length is limited to <= 65535 bytes (configurable in impl)
// - Control frames must have payload <= 125

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct WsConn {
		int  fd;
		bool is_client;
		bool is_connected;

		// Internal state for reading frames
		uint8_t* read_buffer;				// Owned buffer for accumulating incoming data
		size_t   read_buffer_size;			// Size of the valid data currently in the buffer
		size_t   read_buffer_capacity;		// Total allocated size of the buffer
		size_t   read_offset;				// index of next unconsumed byte within that valid region
		// So “available to parse” = read_buffer_size - read_offset

		// ... you can add more fields here if needed for your implementation
		bool close_sent;
		bool close_received;
	} WsConn;

	typedef struct WsServer {
		int fd;
	} WsServer;

	WsServer* ws_server_create(int port);
	WsConn* ws_server_accept(WsServer* server, int max_usecs);	// returns NULL on timeout or error
	void ws_server_destroy(WsServer* server);

	bool ws_conn_send_binary(WsConn* conn, const void* data, size_t len);
	bool ws_conn_send_text(WsConn* conn, const char* data, size_t len);
	void ws_conn_destroy(WsConn* conn);		// best-effort CLOSE; does not wait, conn is not usable after this call

	typedef enum {
		WS_EVT_NONE = 0,   // no complete frame available yet
		WS_EVT_TEXT,       // payload will NOT be null-terminated; payload_len is the length in bytes; payload is not guaranteed to be valid UTF-8
		WS_EVT_BINARY,
		WS_EVT_CLOSED,     // connection closed (ws close or io dead)
	} WsEventType;

	typedef struct {
		WsEventType type;
		const uint8_t* payload;
		size_t payload_len;
	} WsEvent;

	bool ws_conn_poll_event(WsConn** conn, WsEvent* out_event, int max_usecs);

#ifdef __cplusplus
}
#endif
