import { RawData, WebSocket, WebSocketServer } from "ws";
import https from "https";
import { IncomingMessage } from "http";
import { credentials } from "#settings";
import { rl } from "#io";

const httpServer = https.createServer(credentials);
let server: WebSocketServer;
const remotes: [WebSocket, IncomingMessage][] = [];

function connection() {
    server.on("connection", function connection(ws, req) {
        if (typeof req.socket.remotePort === "number") {
            remotes.push([ws, req]);
            console.log(
                `(${req.socket.remotePort} -> login, ${remotes.length} client(s))`
            );
        }

        ws.on("message", function message(data: RawData) {
            const response = `${req.socket.remotePort} -> ${data.toString()}`;
            for (const remote of remotes) {
                if (remote[0] === ws) continue;
                remote[0].send(response, function (error) {
                    if (error) {
                        console.error(
                            `Erro trying send message: ${error.message}`
                        );
                    }
                });
            }
        });

        ws.on("close", function () {
            const index = remotes.indexOf([ws, req]);
            remotes.splice(index);
            console.log(
                `(${req.socket.remotePort} -> logout, ${remotes.length} client(s))`
            );
        });

        ws.on("error", function () {
            console.error("Socket error!");
        });
    });

    rl.on("SIGINT", function () {
        server.close();
        console.log("bye!");
        process.exit();
    });
    rl.on("line", function (input: string) {
        if (input === "") return;
        if (input === "quit") {
            server.close();
            rl.close();
            console.log("\nbye!");
            process.exit();
            return;
        }
        const message = Buffer.from("Server -> " + input);
        for (const client of remotes) {
            client[0].send(message, function (error) {
                if (error) {
                    console.error("Error trying to send message!\n", error);
                    return;
                }
            });
        }
    });
}

export function resolve(port: number) {
    server = new WebSocketServer({ port: port });
    console.log(`WS listening at adress localhost:${port}`);
    connection();
}

export function ssl_resolve(port: number) {
    server = new WebSocketServer({ server: httpServer });
    httpServer.listen(port, () => {
        console.log(`WS SSL listening at adress localhost:${port}`);
    });
    connection();
}

let client: WebSocket;
const httpAgent = new https.Agent(credentials);

function listening() {
    client.on("open", function() {
        console.log(`WS connected at adress: ${client.url}`);
        client.on("message", function message(data: RawData) {
            const message = data.toString();
            console.log(message);
        });

        client.on("close", function () {
            console.log("closed!");
        });

        client.on("error", function (error: Error) {
            console.error(error);
        });
    });

    rl.on("SIGINT", function () {
        client.close();
        console.log("bye!");
        process.exit();
    });
    rl.on("line", function (input: string) {
        if (input === "") return;
        if (input === "quit") {
            client.close();
            rl.close();
            console.log("\nbye!");
            process.exit();
            return;
        }
        const message = Buffer.from(input);
        client.send(message, function (error) {
            if (error) {
                console.error("Error trying to send message!\n", error);
                return;
            }
        });
    });
}

export function connect(port: number) {
    client = new WebSocket(`ws:localhost:${port}`);
    listening();
}

export function ssl_connect(port: number) {
    client = new WebSocket(`wss:localhost:${port}`, { agent:  httpAgent});
    listening();
}
