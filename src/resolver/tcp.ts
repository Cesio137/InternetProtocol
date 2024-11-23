import * as net from "net";
import * as tls from "tls";
import { credentials } from "#settings";

const server = net.createServer(socket);
server.on("error", error);
const ssl_server = tls.createServer(credentials, socket);
ssl_server.on("error", error);

const clients: (net.Socket | tls.TLSSocket)[] = [];

function socket(socket: net.Socket | tls.TLSSocket) {
    if (clients.indexOf(socket) === -1) {
        clients.push(socket);
        console.log(
            `(${socket.remotePort} -> login, ${clients.length} client(s))`
        );
    }

    socket.on("data", (data: Buffer) => {
        if (data.length === 0) return;
        const response = `${socket.remotePort} -> ${data.toString()}`;
        for (const client of clients) {
            if (client.remotePort === socket.remotePort) continue;
            client.write(response, function (error) {
                if (error) {
                    console.error(`Erro trying send message: ${error.message}`);
                }
            });
        }
    });

    socket.on("end", function () {
        const index = clients.indexOf(socket);
        if (index < 0) return;
        clients.splice(index);
        console.log(
            `(${socket.remotePort} -> logout, ${clients.length} client(s))`
        );
    });

    socket.on("close", function (hadError: boolean) {
        const index = clients.indexOf(socket);
        if (index < 0) return;
        clients.splice(index);
        console.log(
            `(${socket.remotePort} -> logout, ${clients.length} client(s))`
        );
    });

    socket.on("error", (err: Error) => {
        console.error("Socket error:", err);
    });
}

function error(err: Error) {
    console.error("Server error:", err);
}

export function resolve(port: number) {
    server.listen(port, () => {
        console.log(`TCP server listening on locahost:${port}`);
    });
}

export function ssl_resolve(port: number) {
    ssl_server.listen(port, () => {
        console.log(`TCP SSL server listening on localhost:${port}`);
    });
}
