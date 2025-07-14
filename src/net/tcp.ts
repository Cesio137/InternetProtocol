import * as net from "net";
import type { AddressInfo } from "net";
import * as tls from "tls";
import { credentials } from "#settings";
import { rl } from "#io";

export class tcpserver {
    constructor(server: net.Server | tls.Server) {
        this.server = server;
        this.clients = [];
        this.server.on("listening", () => this.onlistening());
        this.server.on("connection", (socket) => this.onconnection(socket));
        this.server.on("close", () => this.onclose());
        this.server.on("error", (err) => this.onerror(err));
    }

    public static create() {
        const server = net.createServer();
        return new tcpserver(server);
    }

    server: net.Server;
    clients: net.Socket[];

    write(message: string) {
        for (const sock of this.clients) {
            sock.write(message, function (error) {
                if (error) {
                    console.error("Error trying to send message!\n", error);
                }
            });
        }
    }

    listen(port: number) {
        this.server.listen(port);
    }

    close() {
        for (const sock of this.clients) {
            sock.end();
        }
        this.server.close((err) => {
            if (err) this.onerror(err);
        });
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
        const addr = this.server.address();
        if (addr && typeof addr === "object" && "address" in addr)
            console.log(`TCP listenings on localhost:${(addr as AddressInfo).port}`);
        

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    onconnection(socket: net.Socket) {
        if (this.clients.indexOf(socket) === -1) {
            this.clients.push(socket);
            console.log(
                `(${socket.remoteAddress}:${socket.remotePort} -> login, ${this.clients.length} client(s))`
            );
        }

        socket.on("data", (buf: Buffer) => { 
            this.onclientdata(buf, socket); 
        });
        socket.on("end", () => { 
            this.onclientend(socket); 
        });
        socket.on("error", (err: Error) => {
            this.onclientrror(err, socket);
        });
    }

    onclose() {
        console.log("bye!");
    }

    onerror(err: Error) {
        console.error(err.message);
    }

    // Remote
    onclientdata(buf: Buffer, socket: net.Socket) {
        const message = `${socket.remotePort} -> ${buf.toString()}`;
        console.log(message);
    }

    onclientend(socket: net.Socket): void {
        const index = this.clients.indexOf(socket);
        if (index < 0) return;
        this.clients.splice(index);
        console.log(
            `(${socket.remoteAddress}:${socket.remotePort} -> logout, ${this.clients.length} client(s))`
        );
    }

    onclientrror(err: Error, socket: net.Socket): void {
        console.error(`Error ${socket.localAddress}:${socket.localPort} -> ${err}`);
    }
}

export class tcpserver_ssl extends tcpserver {
    static override create() {
        const server = tls.createServer(credentials);
        return new tcpserver(server);
    }
    declare server: tls.Server;

    override listen(port: number) {
        this.server.listen(port);
    }

    override onlistening() {
        const addr = this.server.address();
        console.log("teste");
        if (addr && typeof addr === "object" && "address" in addr)
            console.log(`TCP ssl listenings on localhost:${(addr as AddressInfo).port}`);        

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    override onconnection(socket: tls.TLSSocket) {
        if (this.clients.indexOf(socket) === -1) {
            this.clients.push(socket);
            console.log(
                `(${socket.remoteAddress}:${socket.remotePort} -> login, ${this.clients.length} client(s))`
            );
        }

        socket.setEncoding('utf8');
        socket.pipe(socket);

        socket.on("data", (buf: Buffer) => { 
            this.onclientdata(buf, socket); 
        });
        socket.on("end", () => { 
            this.onclientend(socket); 
        });
        socket.on("error", (err: Error) => {
            this.onclientrror(err, socket);
        });
    }
}

export class tcpclient {
    constructor(socket: net.Socket | tls.TLSSocket) {
        this.socket = socket;
        this.socket.on("connect", () => this.onconnect());
        this.socket.on("data", (data) => this.ondata(data));
        this.socket.on("end", () => this.onclose());
        this.socket.on("error", (err) => this.onerror(err));
    }

    public static create() {
        const socket = new net.Socket();
        return new tcpclient(socket);
    }

    socket: net.Socket;

    write(message: string) {
        this.socket.write(message, (err) => {
            if (err)
                console.error(err);
        })
    }

    connect(port: number) {
        this.socket.connect({ host: "localhost", port });
    }

    close() {
        this.socket.end();
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
        console.log(`TCP connected at locahost:${this.socket?.localPort}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    ondata(data: Buffer) {
        if (data.length === 0) return;
        const message = data.toString();
        console.log(message);
    }

    onclose() {
        console.log("bye!");
    }

    onerror(err: Error) {
        console.error(err);
    }
}

export class tcpclient_ssl extends tcpclient {
    static override  create() {
        const rawSock = new net.Socket();
        const socket = new tls.TLSSocket(rawSock, { ...credentials, rejectUnauthorized: false });
        return new tcpclient(socket);
    }

    declare socket: tls.TLSSocket;

    override connect(port: number) {
        this.socket.connect({ host: "localhost", port });
    }

    override onconnect() {
        console.log(`TCP ssl connected at locahost:${this.socket?.localPort}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => { this.online(input); });
    }
}