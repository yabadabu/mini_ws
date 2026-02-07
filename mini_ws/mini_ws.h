#pragma once

#include <stdbool.h>
//#include <stddef.h>
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

	bool ws_conn_send_binary(WsConn* conn, const uint8_t* data, size_t len);
	bool ws_conn_send_text(WsConn* conn, const char* data, size_t len);
	void ws_conn_destroy(WsConn* conn);		// best-effort CLOSE; does not wait, conn is not usable after this call

	typedef enum { WS_IO_OK, WS_IO_TIMEOUT, WS_IO_CLOSED, WS_IO_ERROR } WsIoResult;
	WsIoResult ws_conn_read(WsConn* conn, int max_usecs);

	typedef enum {
		WS_NO_FRAME = 0,
		WS_TEXT = 1,
		WS_BINARY = 2,
		WS_CLOSE = 8,
		WS_PING = 9,
		WS_PONG = 10,
		WS_ERROR = -1
	} WsOpcode;
	// payload_data and payload_len are only valid if the return value is WS_TEXT or WS_BINARY, and only valid
	// until the next call to ws_conn_read() or ws_conn_parse_frame() on the same connection.
	WsOpcode ws_conn_parse_frame(WsConn* conn, const uint8_t** payload_data, size_t* payload_len);

#ifdef __cplusplus
}
#endif
