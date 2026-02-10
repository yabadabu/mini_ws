# Description

This is a minimal websocket in c, with no dependencies, ideal for small projects where you need bidirectional communications oriented to msg, or communicate c with a web browser js.

# Features

* Super simple API in C
* Poll specifying the maximum timeout in micro secs. Use 0 is perform a single test for incomming messages.
* Supports text and binary frames
* Read buffer is owned by the WsConn.
* Tested on windows/linux/osx

# What it's not

* A library to manage hunders of simultaneous ws connections
* An http server

# Install

Copy and compile the mini_ws folder into your project.

# Usage

The WsServer object is used to accept new connections. The server does not keep track of the active connections.

```c

#include "mini_ws/mini_ws.h"
..

	WsServer* ws_server = ws_server_create(7450);
	if (!ws_server)		// Can't start the ws server
		return;
	
	// Check for new connections
	if( WsConn* conn = ws_server_accept(ws_server, 1000000) ) 
		add_new_connection( conn );

	// When we are done with the server
	ws_server_destroy(ws_server);
```

The WsConn represents a single connection. You can poll for messages, write binary/text messages or close the connection.

```c

	// The timeout will not be used if there are frames already in the buffer
	WsEvent evt;
	while(ws_conn_poll_event(&conn, &evt, 1000)) {
		if (evt.type == WS_EVT_CLOSED) {
			// app->conn has already been cleared and conn = NULL now
			// ..
		} 
		else if (evt.type == WS_EVT_TEXT) {
			// payload is NOT null-terminated
			printf("Ws.Text: %.*s\n", (int)evt.payload_len, evt.payload);
		}
		else if (evt.type == WS_EVT_BINARY) {
			printf("Ws.Bin %d bytes\n", (int)evt.payload_len);
		}
	}

```

In Windows, remember to init the winsock library before using the ws_server_create function:

```c

#include <winsock2.h>

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
```

# Compile in Linux/OSX

	cc demo.cpp ../mini_ws/mini_ws.c -I.. -lstdc++ -o server

# Run the demo

	./server

Launch a web server to serve the static page, then use a browser to navigate to http://127.0.0.1:7450

	python -m http.server


