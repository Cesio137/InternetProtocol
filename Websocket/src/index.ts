import { RawData, WebSocket, WebSocketServer } from "ws";

//URL -> ws://localhost:3000
const WS_PORT: number = 3000;
const wss: WebSocketServer = new WebSocketServer({ port: WS_PORT });
console.log(`listening at adress localhost:${WS_PORT}`);
const clients: WebSocket[] = [];

wss.on("connection", function connection(ws: WebSocket) {
  clients.push(ws);
  console.log("connected");

  ws.on("error", function error() {
    if (clients.indexOf(ws) > -1) clients.splice(clients.indexOf(ws), 1);
    console.log("closed!");
  });

  ws.on("message", function message(data) {
    HandlerMessage(ws, data);
  });

  ws.on("close", function close() {
    if (clients.indexOf(ws) > -1) clients.splice(clients.indexOf(ws), 1);
    console.log("closed!");
  });
});

function isJson(str: string): boolean {
  try {
    JSON.parse(str);
    return true;
  } catch (e) {
    return false;
  }
}

function HandlerMessage(ws: WebSocket, data: RawData) {
  if (isJson(data.toString())) {
    const JsonObject: JSON = JSON.parse(data.toString());
    clients.forEach((websocket) => {
      if (ws !== websocket) websocket.send(JSON.stringify(JsonObject));
    });
  }
}
