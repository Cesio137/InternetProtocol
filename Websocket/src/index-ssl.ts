import https from "https";
import fs from "fs";
import WebSocket, { WebSocketServer } from "ws";

const key = fs.readFileSync("server-key.pem", "utf8");
const cert = fs.readFileSync("server-cert.pem", "utf8");

//URL -> ws://localhost:3000
const WS_PORT = 3000;
const credentials = { key: key, cert: cert };
const httpsServer = https.createServer(credentials);
const wss = new WebSocketServer({ server: httpsServer });
const clients: WebSocket[] = [];

wss.on("connection", (ws: WebSocket) => {
    if (clients.indexOf(ws) < 0) clients.push(ws);
    console.log("connected!");

    ws.on("message", function message(message: string) {
        console.log("Client message: " + message);
        ws.send(message);
        clients.forEach((websocket: WebSocket) => {
            if (ws !== websocket) websocket.send(message);
        });
    });

    ws.on("close", function close() {
        if (clients.indexOf(ws) > -1) clients.splice(clients.indexOf(ws), 1);
        console.log("closed!");
    });
});

httpsServer.listen(WS_PORT, () => {
    console.log(`listening at adress localhost:${WS_PORT}`);
});
