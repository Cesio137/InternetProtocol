import * as dgram from "dgram";
import { AddressInfo } from "net";

const server: dgram.Socket = dgram.createSocket("udp4");
let clients: dgram.RemoteInfo[] = [];

server.on("message", function (msg: Buffer, rinfo: dgram.RemoteInfo) {
    const data = msg.toString();
    switch (data) {
        case "login":
            clients.push(rinfo);
            console.log(
                `(${rinfo.port} -> ${msg}, ${clients.length} client(s))`
            );
            return;
        case "logout":
            if (clients.length === 1) {
                clients = [];
                console.log(
                    `(${rinfo.port} -> ${msg}, ${clients.length} client(s))`
                );
                return;
            }
            const index = clients.indexOf(rinfo);
            clients = clients.splice(index);
            console.log(
                `(${rinfo.port} -> ${msg}, ${clients.length} client(s))`
            );
            return;
    }
    const response = `${rinfo.port} -> ${data}`;
    for (const client of clients) {
        if (client.port === rinfo.port) continue;
        server.send(response, client.port, client.address, function (error) {
            if (error) {
                console.error(`Erro trying send message: ${error.message}`);
                return;
            }
        });
    }
});

server.on("listening", function () {
    const address: AddressInfo = server.address();
    console.log(`UDP listening at adress: localhost:${address.port}`);
});

server.on("error", function (error: Error) {
    console.error(`Server error: ${error.message}`);
    server.close();
});

export function resolve(port: number) {
    server.bind(port);
}
