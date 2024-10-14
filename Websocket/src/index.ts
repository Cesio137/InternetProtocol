import internal from "stream";
import { RawData, WebSocket, WebSocketServer } from "ws";

//URL -> ws://localhost:3000
const WS_PORT: number = 3000;
const wss: WebSocketServer = new WebSocketServer({ port: WS_PORT });
console.log(`listening at adress localhost:${WS_PORT}`);
const clients: WebSocket[] = [];

wss.on("connection", function connection(ws: WebSocket) {
    if (clients.indexOf(ws) < 0) clients.push(ws);
    console.log("connected!");

    ws.on("error", function error() {
        if (clients.indexOf(ws) > -1) clients.splice(clients.indexOf(ws), 1);
        console.log("closed!");
    });

    ws.on("message", function message(data: RawData) {
        console.log("Client message: " + data);
        ws.send(data);
        clients.forEach((websocket: WebSocket) => {
            if (ws !== websocket) websocket.send(data);
        });
    });

    ws.on("close", function close() {
        if (clients.indexOf(ws) > -1) clients.splice(clients.indexOf(ws), 1);
        console.log("closed!");
    });
});
