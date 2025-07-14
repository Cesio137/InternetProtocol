import { rl } from "#io";
import * as dgram from "dgram";
import type { AddressInfo } from "net";


export class udpserver{
    constructor() {
        this.socket.on("listening", () => this.onlistening());
        this.socket.on("message", (msg, rinfo) => this.onmessage(msg, rinfo));
        this.socket.on("close", () => this.onclose());
        this.socket.on("error", (err) => this.error(err));
    }

    socket: dgram.Socket = dgram.createSocket("udp4");
    remotes: dgram.RemoteInfo[] = [];

    write(message: string) {
        for (const client of this.remotes) {
            this.socket.send(message, client.port, client.address, function(error) {
                if (error) {
                    console.error(`Erro trying send message: ${error.message}`);
                }
            });
        }
    }

    open(port: number) {
        this.socket.bind(port);
    }

    close() {
        this.socket.close();
        process.exit();
    }

    online(input: string) {
        if (input === "") return;
        if (input === "quit") {
            this.close();
            return;
        }
        this.write(input);
    }

    onlistening() {
        const addr = this.socket.address();
        console.log(`UDP listening at adress: ${addr.address}:${addr.port}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    onmessage(buf: Buffer, rinfo: dgram.RemoteInfo) {
        if (buf.length === 0) return;
        const data = buf.toString();
        switch (data) {
            case "login":
                if (this.remotes.indexOf(rinfo) !== -1) return;
                this.remotes.push(rinfo);
                console.log(
                    `(${rinfo.port} -> ${data}, ${this.remotes.length} client(s))`
                );
                return;
            case "logout":
                const index = this.remotes.indexOf(rinfo);
                this.remotes.splice(index);
                console.log(
                    `(${rinfo.port} -> ${data}, ${this.remotes.length} client(s))`
                );
                return;
        }
        console.log(`${rinfo.port} -> ${data}`);
    }

    onclose() {
        console.log("bye!");
    }

    error(error: Error) {
        console.error(`Error: ${error.message}`);
    }
}

export class udpclient {
    constructor() {
        this.socket.on("connect", () => this.onconnect());
        this.socket.on("message", (msg, rinfo) => this.onmessage(msg, rinfo));
        this.socket.on("close", () => this.onclose());
        this.socket.on("error", (err) => this.error(err));
    }

    socket: dgram.Socket = dgram.createSocket("udp4");

    write(message: string) {
        this.socket.send(message, function (error, bytes: number) {
            if (error) {
                console.error("Error trying to send message!\n", error);
                return;
            }
        });
    }

    connect(port: number) {
        this.socket.connect(port, "localhost");
    }

    close() {
        this.socket.close();
        process.exit();
    }

    online(input: string) {
        if (input === "") return;
        if (input === "quit") {
            this.close();
            return;
        }
        this.write(input);
    }

    onconnect() {
        const addr: AddressInfo = this.socket.address();
        console.log(`UDP connected at adress: ${addr.address}:${addr.port}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    onmessage(buf: Buffer, rinfo: dgram.RemoteInfo) {
        if (buf.length === 0) return;
        const data = buf.toString();
        console.log(`${rinfo.port} -> ${data}`);
    }

    onclose() {
        console.log("bye!");
    }

    error(error: Error) {
        console.error(`Error: ${error.message}`);
    }
}