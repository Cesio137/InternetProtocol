import { RawData, WebSocket, WebSocketServer } from "ws";
import https from "https";
import { credentials } from "#settings";

const httpsServer = https.createServer(credentials);
let wss: WebSocketServer;
const clients: WebSocket[] = [];

function connection() {
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
}

export function resolve(port: number) {
    wss = new WebSocketServer({ port: port });
    console.log(`WS listening at adress localhost:${port}`);
    connection();
}

export function ssl_resolve(port: number) {
    wss = new WebSocketServer({ server: httpsServer });
    httpsServer.listen(port, () => {
        console.log(`WS SSL listening at adress localhost:${port}`);
    });
    connection();
}