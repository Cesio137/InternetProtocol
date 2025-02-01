import * as dgram from "dgram";
import { AddressInfo } from "net";
import { rl } from "#io";

// Server

const server: dgram.Socket = dgram.createSocket("udp4");
const remotes: dgram.RemoteInfo[] = [];

server.on("message", function (msg: Buffer, rinfo: dgram.RemoteInfo) {
    if (msg.length === 0) return;
    const data = msg.toString();
    switch (data) {
        case "login":
            if (remotes.indexOf(rinfo) !== -1) return;
            remotes.push(rinfo);
            console.log(
                `(${rinfo.port} -> ${msg}, ${remotes.length} client(s))`
            );
            return;
        case "logout":
            const index = remotes.indexOf(rinfo);
            remotes.splice(index);
            console.log(
                `(${rinfo.port} -> ${msg}, ${remotes.length} client(s))`
            );
            return;
    }
    const response = `${rinfo.port} -> ${data}`;
    for (const remote of remotes) {
        if (remote.port === rinfo.port) continue;
        server.send(response, remote.port, remote.address, function (error) {
            if (error) {
                console.error(`Erro trying send message: ${error.message}`);
            }
        });
    }
});

server.on("listening", function () {
    const address: AddressInfo = server.address();
    console.log(`UDP listening at adress: ${address.address}:${address.port}`);
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
        const response = Buffer.from(input);
        for (const client of remotes) {
            server.send(response, client.port, client.address, function (error) {
                if (error) {
                    console.error(`Erro trying send message: ${error.message}`);
                }
            });
        }
    });
});

server.on("error", function (error: Error) {
    console.error(`Server error: ${error.message}`);
});

export function open(port: number) {
    server.bind(port);
}

// Client

const client = dgram.createSocket("udp4");

client.on("message", function (msg: Buffer, rinfo: dgram.RemoteInfo) {
    if (msg.length === 0) return;
    const data = msg.toString();
    console.log(data);
});

client.on("connect", function () {
    const address: AddressInfo = client.remoteAddress();
    console.log(`UDP connected at adress: ${address.address}:${address.port}`);
    console.log(
        "Type and press 'Enter' to send a message or 'quit' to close socket"
    );
    client.send("login", function (error, bytes: number) {
        if (error) {
            console.error("Error trying to send message!\n", error);
            return;
        }
    });
    rl.on("SIGINT", function () {
        client.send("logout", function (error, bytes: number) {
            if (error) {
                console.error("Error trying to send message!\n", error);
                return;
            }
        });
        server.close();
        console.log("bye!");
        process.exit();
    });
    rl.on("line", function (input: string) {
        if (input === "") return;
        if (input === "quit") {
            client.send("logout", function (error, bytes: number) {
                if (error) {
                    console.error("Error trying to send message!\n", error);
                    return;
                }
            });
            client.close();
            rl.close();
            console.log("\nbye!");
            return;
        }
        const message = Buffer.from(input);
        client.send(message, function (error, bytes: number) {
            if (error) {
                console.error("Error trying to send message!\n", error);
                return;
            }
        });
    });
});

client.on("error", function (error: Error) {
    console.error(`Client error: ${error.message}`);
    rl.close();
    client.close();
});

export function connect(port: number) {
    client.connect(port, "localhost");
}
