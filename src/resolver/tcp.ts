import * as net from "net";
import * as tls from "tls";
import { credentials } from "#settings"

const server = net.createServer(socket);

server.on('error', error);

const ssl_server = tls.createServer(credentials, socket);

ssl_server.on("error", error);

function socket(socket: net.Socket | tls.TLSSocket) {
    console.log('Client connected:', socket.remoteAddress, socket.remotePort);

    socket.on('data', (data:Buffer) => {
        console.log('Received from client:', data.toString());
        socket.write(data);
    });

    socket.on('end', () => {
        console.log('Client disconnected: ', socket.remoteAddress, socket.remotePort);
    });

    socket.on('close', function(hadError:boolean) {
        console.log('Client disconnected: ', socket.remoteAddress, socket.remotePort);
    });

    socket.on('error', (err:Error) => {
        console.error('Socket error:', err);
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
    })
}