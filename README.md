# Description

This is a minimal websocket in c, with no dependencies, ideal for small projects where you need bidirectional communications oriented to msg, or communicate c with js.

# Features

* Super simple API
* Poll specifying the maximum timeout in micro secs. Use 0 is perform a single test for incomming messages.
* Support Text and binary frames
* Read buffer is owned by the WsConn.

# Install

Copy and compile the mini_ws folder into your project.

# Usage

The WsServer object is used to accept new connections.

```c
	WsServer* ws_server = ws_server_create(7450);
	if (!ws_server)		// Can't start the ws server
		return;

	// Check if a new ws connections is knocking the door. Wait 1s max.
	WsConn* conn = ws_server_accept(ws_server, 1000000);
	if (conn) {

		// Check if new data has arrived..
		WsIoResult r = ws_conn_read(conn, 1000000);


    }



```



