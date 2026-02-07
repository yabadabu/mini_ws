
#include "mini_ws/mini_ws.h"

#include <stdio.h>

#ifdef _WIN32
#include <WinSock2.h>
#endif

int main()
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

	WsServer* ws_server = ws_server_create(7450);
	if (!ws_server)
		return -1;
	printf("ws server started\n");

	while (true) {
		printf(".");
		fflush(stdout);
		WsConn* conn = ws_server_accept(ws_server, 1000000);
		if (conn) {
			printf("ws connection accepted: fd=%d\n", conn->fd);

			while( true ) {
				WsIoResult r = ws_conn_read(conn, 1000000);
				if (r == WS_IO_ERROR || r == WS_IO_CLOSED)
					break;
				else if (r == WS_IO_TIMEOUT)
					continue;
				
				const uint8_t* payload;
				size_t payload_len;

				while (WsOpcode code = ws_conn_parse_frame(conn, &payload, &payload_len)) {
					if( code == WS_TEXT ) {
						printf("Recv text frame: %.*s\n", (int)payload_len, payload);
						ws_conn_send_text(conn, "Hello, WebSocket!", 18);
					}
					else if (code == WS_BINARY) {
						uint8_t data[4] = { 'A', 'B', 'C', 'D' };
						printf("Recv %lld bytes\n", payload_len);
						ws_conn_send_binary(conn, data, sizeof(data));
					}
					else if (code == WS_CLOSE) {
						printf("Recv close frame\n");
						ws_conn_destroy(conn);
						break;
					}
					else if (code == WS_ERROR) {
						printf("Recv err\n");
					}
					else if (code == WS_NO_FRAME) {
						printf("Recv WS_NO_FRAME\n");
					}
				}
			}
		}
	}

	ws_server_destroy( ws_server );

#ifdef _WIN32
	WSACleanup();
#endif
    return 0;
}
