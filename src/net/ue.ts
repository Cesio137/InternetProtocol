import { rl } from "#io";
import * as dgram from "dgram";
import type { AddressInfo } from "net";

interface LoginPayload {
    event: "login_order";
    id: number;
    players: number[];
}

interface RegisterPayload {
    event: "register_order";
    player: number;
}

interface LogoutPayload {
    event: "logout_order";
    player: number;
}

interface PlayerStatePayload {
    event: "playerState_order";
    condition: number; // 0-none, 1-owner, 2-skip+owner
    id: number;
    positionX: number;
    positionY: number;
    positionZ: number;
    rotationZ: number;
}

export class unrealserver {
    constructor() {
        this.socket.on("listening", () => this.onlistening());
        this.socket.on("message", (msg, rinfo) => this.onmessage(msg, rinfo));
        this.socket.on("close", () => this.onclose());
        this.socket.on("error", (err) => this.error(err));
    }

    socket: dgram.Socket = dgram.createSocket("udp4");
    remotes: Map<number ,dgram.RemoteInfo> = new Map();

    write(message: string) {
        for (const [port, remote] of this.remotes) {
            this.socket.send(message, remote.port, remote.address, function (error) {
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
        const data = buf.toString();
        switch (data) {
        case "login":
            this.onlogin(rinfo);
            break;
        case "logout":
            this.onlogout(rinfo);
            break;
        default:
            this.onplayerbroadcast(rinfo, data);
        }
    }

    onclose() {
        console.log("bye!");
    }

    error(error: Error) {
        console.error(`Error: ${error.message}`);
    }

    // Payload

    onlogin(rinfo: dgram.RemoteInfo) {
        if (this.remotes.has(rinfo.port)) return;
        this.remotes.set(rinfo.port, rinfo);
        console.log(`${rinfo.port} -> login | total players: ${this.remotes.size}`);

        if (this.remotes.size <= 1) {
            const loginPayload: LoginPayload = {
                event: "login_order",
                id: rinfo.port,
                players: []
            }
            const json = JSON.stringify(loginPayload);
            this.socket.send(json, rinfo.port, rinfo.address);
            return;
        }

        const registerPayload: RegisterPayload = {
            event: "register_order",
            player: rinfo.port
        };
        const registerjson = JSON.stringify(registerPayload);

        const loginPayload: LoginPayload = {
            event: "login_order",
            id: rinfo.port,
            players: this.remotes.keys().filter(val => val !== rinfo.port).toArray()
        };
        const loginjson = JSON.stringify(loginPayload);

        this.socket.send(loginjson, rinfo.port, rinfo.address);
        for (const [port, remote] of this.remotes) {
            if (port === rinfo.port) continue;
            this.socket.send(registerjson, remote.port, remote.address);
        }
    }

    onlogout(rinfo: dgram.RemoteInfo) {
        if (!this.remotes.has(rinfo.port)) return;
        this.remotes.delete(rinfo.port);
        console.log(`${rinfo.port} -> logout | total players: ${this.remotes.size}`);

        if (this.remotes.size < 1) return;

        const logoutPayload: LogoutPayload = {
            event: "logout_order",
            player: rinfo.port
        };

        const logoutJson = JSON.stringify(logoutPayload);
        for (const [port, remote] of this.remotes) {
            this.socket.send(logoutJson, remote.port, remote.address);
        }
    }

    onplayerbroadcast(rinfo: dgram.RemoteInfo, data: string) {
        const playerMessage = data;
        const json = JSON.parse(playerMessage);
        let playerPayload = json as PlayerStatePayload;
        playerPayload.id = rinfo.port;
        const playerJson = JSON.stringify(playerPayload);

        switch (playerPayload.condition) {
            case 0:
                for (const [port, remote] of this.remotes) {
                    this.socket.send(playerJson, remote.port, remote.address);
                }
                break;
            case 1:
                this.socket.send(playerJson, rinfo.port, rinfo.address);
                break;
            case 2:
                for (const [port, remote] of this.remotes) {
                    if (remote.port === rinfo.port) continue;
                    this.socket.send(playerJson, remote.port, remote.address);
                }
                break;
        }
    }
}