
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

	while (true) {
		printf("ws server ready\n");
		WsConn* conn = ws_server_accept(ws_server, 1000000);
		if (conn) {
			printf("ws connection accepted: fd=%d\n", conn->fd);
			ws_conn_send_text(conn, "Hello, WebSocket!", 18);
			ws_conn_destroy(conn);
		}
	}

	ws_server_destroy( ws_server );

#ifdef _WIN32
	WSACleanup();
#endif
    return 0;
}
