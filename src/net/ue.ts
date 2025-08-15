import { udpSocket } from "bun";

interface LoginPayload {
    event: "login";
    id: number;
    players: number[];
}

interface RegisterPayload {
    event: "register";
    player: number;
}

interface LogoutPayload {
    event: "logout";
    player: number;
}

interface PlayerStatePayload {
    event: "playerState";
    condition: number; // 0-none, 1-owner, 2-skip+owner
    id: number;
    positionX: number;
    positionY: number;
    positionZ: number;
    rotationZ: number;
}

type ClientPayload = LoginPayload | LogoutPayload | PlayerStatePayload;

let clients: Map<number, string> = new Map();

export class udpUnreal {
    public static async create() {
        const socket = await Bun.udpSocket({
            port: 8080,
            socket: {
                data(socket, data, rinfo, addr) {
                    const json = JSON.parse(data.toString());
                    const payload = json as ClientPayload;

                    switch (payload.event) {
                        case "login":
                            loginHandler(payload, socket, rinfo, addr);
                            break;
                        case "logout":
                            logoutHandler(socket, rinfo, addr);
                            break;
                        case "playerState":
                            playerHandler(payload, socket, rinfo, addr);
                            break;
                    }
                },
            }
        });
    }
}

async function loginHandler(payload: LoginPayload, socket: Bun.udp.Socket<"buffer">, rinfo: number, addr: string) {
    if (clients.has(rinfo)) return;
    clients.set(rinfo, addr);
    console.log(`${rinfo} -> login | total players: ${clients.size}`);

    if (clients.size < 2) {
        const loginPayload: LoginPayload = {
            event: "login",
            id: rinfo,
            players: [0]
        }
        const json = JSON.stringify(loginPayload);
        socket.send(json, rinfo, addr);
    }

    const registerPayload: RegisterPayload = {
        event: "register",
        player: rinfo
    };
    const registerjson = JSON.stringify(registerPayload);

    const loginPayload: LoginPayload = {
        event: "login",
        id: rinfo,
        players: clients.keys().toArray()
    };
    const loginjson = JSON.stringify(loginPayload);

    socket.send(loginjson, rinfo, addr);
    for (const [port, address] of clients) {
        if (rinfo == port) continue;
        socket.send(registerjson, port, address);
    }
}

async function logoutHandler(socket: Bun.udp.Socket<"buffer">, rinfo: number, addr: string) {
    if (!clients.has(rinfo)) return;
    console.log(`${rinfo} -> logout | total players: ${clients.size}`);

    if (clients.size < 1) return;

    const logoutPayload: LogoutPayload = {
        event: "logout",
        player: rinfo
    };

    const logoutJson = JSON.stringify(logoutPayload);
    for (const [port, address] of clients) {
        socket.send(logoutJson, port, address);
    }
}

async function playerHandler(payload: PlayerStatePayload, socket: Bun.udp.Socket<"buffer">, rinfo: number, addr: string) {
    let playerPayload = payload;
    playerPayload.id = rinfo;
    const playerJson = JSON.stringify(playerPayload);

    switch (playerPayload.condition) {
        case 0:
            for (const [port, address] of clients) {
                socket.send(playerJson, port, address);
            }
            break;
        case 1:
            socket.send(playerJson, rinfo, addr);
            break;
        case 2:
            for (const [port, address] of clients) {
                if (port === rinfo) continue;
                socket.send(playerJson, port, address);
            }
            break;
    }
}