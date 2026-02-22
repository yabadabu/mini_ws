
#define _CRT_SECURE_NO_WARNINGS
#include "mini_ws/mini_ws.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <WinSock2.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms)*1000)
#endif

#include <vector>

class Png : std::vector< uint8_t > {
public:
	bool readFromFile(const char* filename) {
		FILE* f = fopen(filename, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			size_t size = ftell(f);
			fseek(f, 0, SEEK_SET);
			resize(size);
			fread(data(), 1, size, f);
			fclose(f);
			return true;
		}
		return false;
	}
	void send(WsConn* conn) {
		ws_conn_send_binary(conn, data(), size());
	}
};

Png pngs[2];

int main()
{
#ifdef _WIN32
	WSADATA wsa_data;
	if (!WSAStartup(MAKEWORD(2, 2), &wsa_data))
		return -1;
#endif

	pngs[0].readFromFile("img00.png");
	pngs[1].readFromFile("img01.png");

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

			WsEvent evt;
			while( true ) {

				if (ws_conn_poll_event(&conn, &evt, 1000000)) {
					if (evt.type == WS_EVT_TEXT) {
						printf("Event: text frame: %.*s\n", (int)evt.payload_len, evt.payload);
						ws_conn_send_text(conn, "Hello, WebSocket!", 18);

						if (strncmp((char*)evt.payload, "png0", 4) == 0)
							pngs[0].send(conn);
						else if (strncmp((char*)evt.payload, "png1", 4) == 0)
							pngs[1].send(conn);
						else if (strncmp((char*)evt.payload, "pngs", 4) == 0) {
							for (int i = 0; i < 10; ++i) {
								for (int j = 0; j < 2; ++j) {
									pngs[j].send(conn);
									sleep_ms(16);
								}
							}
						}

					}
					else if (evt.type == WS_EVT_BINARY) {
						printf("Event: binary frame: %d bytes\n", (int)evt.payload_len);
						uint8_t data[4] = { 'A', 'B', 'C', 'D' };
						ws_conn_send_binary(conn, data, sizeof(data));
					}
					else if (evt.type == WS_EVT_CLOSED) {
						printf("Event: connection closed\n");
						break;
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
