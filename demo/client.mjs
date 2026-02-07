// client.mjs
// Run: node client.mjs ws://127.0.0.1:8080

const url = process.argv[2] ?? "ws://127.0.0.1:7450";

if (typeof WebSocket === "undefined") {
  console.error("This script needs Node 20+ (global WebSocket).");
  process.exit(1);
}

const ws = new WebSocket(url);
console.log( "ws ready?")
ws.addEventListener("open", async () => {
  console.log("open");

  // Send a text message
  ws.send("hello from node");

  // Send a binary message
  const bin = new Uint8Array([1, 2, 3, 4, 250, 251, 252]);
  ws.send(bin);

  // Send a larger binary payload (<64KB)
  const big = new Uint8Array(1024);
  for (let i = 0; i < big.length; i++) big[i] = i & 255;
  ws.send(big);

  // Close after a moment
  setTimeout(() => {
    console.log("closing...");
    ws.close(1000, "bye");
  }, 500);
});

ws.addEventListener("message", (ev) => {
  // Node may deliver strings or ArrayBuffer depending on server payload type.
  if (typeof ev.data === "string") {
    console.log("message (text):", ev.data);
  } else if (ev.data instanceof ArrayBuffer) {
    const u8 = new Uint8Array(ev.data);
    console.log("message (binary):", u8.length, "bytes", u8.slice(0, 16));
  } else if (ArrayBuffer.isView(ev.data)) {
    // Some environments deliver Uint8Array directly
    const u8 = new Uint8Array(ev.data.buffer, ev.data.byteOffset, ev.data.byteLength);
    console.log("message (binary view):", u8.length, "bytes", u8.slice(0, 16));
  } else {
    console.log("message (unknown):", ev.data);
  }
});

ws.addEventListener("close", (ev) => {
  console.log("close:", ev.code, ev.reason);
});

ws.addEventListener("error", (ev) => {
  console.log("error:", ev.message ?? ev);
});
