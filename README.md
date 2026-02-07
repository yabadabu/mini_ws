# Description

This is a minimal ws in c, with no dependencies for small projects where you need bidirectional communications oriented to msg.

# Features

* Super simple API
* Polling with maximum timeout in usecs
* Text and binary frames

# Install

Copy and compile the mini_ws folder into your project.

# Usage

The WsServer object is used to accept new connections.

```c
	WsServer* ws_server = ws_server_create(7450);
	if (!ws_server)		// Can't start the ws server
		return;

	...

	// Check if a new ws connections is knocking the door. Wait 1s max.
	WsConn* conn = ws_server_accept(ws_server, 1000000);
	if (conn) {



```



