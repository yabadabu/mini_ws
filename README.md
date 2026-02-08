# Description

This is a minimal websocket in c, with no dependencies, ideal for small projects where you need bidirectional communications oriented to msg, or communicate c with a web browser js.

# Features

* Super simple API
* Poll specifying the maximum timeout in micro secs. Use 0 is perform a single test for incomming messages.
* Supports text and binary frames
* Read buffer is owned by the WsConn.
* Tested on windows/linux/osx

# Install

Copy and compile the mini_ws folder into your project.

# Usage

The WsServer object is used to accept new connections. The server does not keep track of the active connections.

```c
	WsServer* ws_server = ws_server_create(7450);
	if (!ws_server)		// Can't start the ws server
		return;
	
	// ...
	if( WsConn* conn = ws_server_accept(ws_server, 1000000) ) 
		add_new_connection( conn );

	// ...
	ws_server_destroy(ws_server);
```

The WsConn represents a single connection. You can check for messages, write binary/text messages or close the connection.

```c


```

In windows, remember to init the winsock library before using the ws_server_create function:

```c

#include <winsock2.h>

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
```

# Compile in Linux

	cc demo.cpp ../mini_ws/mini_ws.c -I.. -lstdc++ -o server

# Run the demo

	./server

Launch a web server to serve the static page, then use a browser to navigate to http://127.0.0.1:7450

	python -m http.server


