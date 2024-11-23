import { RawData, WebSocket, WebSocketServer } from "ws";
import https from "https";
import { credentials } from "#settings";

const httpsServer = https.createServer(credentials);
let wss: WebSocketServer;
//const clients: WebSocket[] = [];
const clients = new Map<number, WebSocket>();

function connection() {
    wss.on("connection", function connection(ws, req) {
        if (typeof req.socket.remotePort === "number") {
            clients.set(req.socket.remotePort, ws);
            console.log(
                `(${req.socket.remotePort} -> login, ${clients.size} client(s))`
            );
        }

        ws.on("message", function message(data: RawData) {
            const response = `${
                req.socket.remotePort
            } -> ${data.toString()}`;
            for (const [port, client] of clients) {
                if (port === req.socket.remotePort) continue;
                client.send(response, function (error) {
                    if (error) {
                        console.error(
                            `Erro trying send message: ${error.message}`
                        );
                    }
                });
            }
        });

        ws.on("close", function () {
            if (typeof req.socket.remotePort === "number") {
                clients.delete(req.socket.remotePort);
                console.log(
                    `(${req.socket.remotePort} -> logout, ${clients.size} client(s))`
                );
            }
        });

        ws.on("error", function () {
            console.error("Socket error!");
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
