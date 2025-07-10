import { rl } from "#io";
import * as dgram from "dgram";
import type { AddressInfo } from "net";


export class udpserver {
    constructor() {
        this.socket.on("listening", this.onlistening);
        this.socket.on("message", this.onmessage);
        this.socket.on("close", this.onclose);
        this.socket.on("error", this.error);
    }

    public write(message: string) {
        for (const client of this.remotes) {
            this.socket.send(message, client.port, client.address, function(error) {
                if (error) {
                    console.error(`Erro trying send message: ${error.message}`);
                }
            });
        }
    }

    public open(port: number) {
        this.socket.bind(port);
    }

    public close() {
        this.socket.close();
        process.exit();
    }

    private socket: dgram.Socket = dgram.createSocket("udp4");
    private remotes: dgram.RemoteInfo[] = [];

    private online(input: string) {
        if (input === "") return;
        if (input === "quit") {
            rl.close();
            this.close();
            return;
        }
        this.write(input);
    }

    private onlistening() {
        const address: AddressInfo = this.socket.address();
        console.log(`UDP listening at adress: ${address.address}:${address.port}`);

        rl.on("SIGINT", this.close);
        rl.on("line", this.online);
    }

    private onmessage(buf: Buffer, rinfo: dgram.RemoteInfo) {
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

    private onclose() {
        console.log("bye!");
    }

    private error(error: Error) {
        console.error(`Error: ${error.message}`);
    }
}

export class udpclient {
    constructor() {
        this.socket.on("connect", this.onconnect);
        this.socket.on("message", this.onmessage);
        this.socket.on("close", this.onclose);
        this.socket.on("error", this.error);
    }

    public write(message: string) {
        this.socket.send(message, function (error, bytes: number) {
            if (error) {
                console.error("Error trying to send message!\n", error);
                return;
            }
        });
    }

    public connect(port: number) {
        this.socket.connect(port, "localhost");
    }

    public close() {
        this.socket.close();
        process.exit();
    }

    private socket: dgram.Socket = dgram.createSocket("udp4");

    private online(input: string) {
        if (input === "") return;
        if (input === "quit") {
            rl.close();
            this.close();
            return;
        }
        this.write(input);
    }

    private onconnect() {
        const address: AddressInfo = this.socket.address();
        console.log(`UDP listening at adress: ${address.address}:${address.port}`);

        rl.on("SIGINT", this.close);
        rl.on("line", this.online);
    }

    private onmessage(buf: Buffer, rinfo: dgram.RemoteInfo) {
        if (buf.length === 0) return;
        const data = buf.toString();
        console.log(`${rinfo.port} -> ${data}`);
    }

    private onclose() {
        console.log("bye!");
    }

    private error(error: Error) {
        console.error(`Error: ${error.message}`);
    }
}