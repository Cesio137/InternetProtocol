import * as dgram from "dgram";
import { AddressInfo } from "net";

const server: dgram.Socket = dgram.createSocket("udp4");
const clients: dgram.RemoteInfo[] = [];

server.on("message", function (msg: Buffer, rinfo: dgram.RemoteInfo) {
    if (msg.length === 0) return;
    const data = msg.toString();
    switch (data) {
        case "login":
            if (clients.indexOf(rinfo) !== -1) return;
            clients.push(rinfo);
            console.log(
                `(${rinfo.port} -> ${msg}, ${clients.length} client(s))`
            );
            return;
        case "logout":
            const index = clients.indexOf(rinfo);
            if (index === -1) return;
            clients.splice(index);
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
