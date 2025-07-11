import * as net from "net";
import type { AddressInfo } from "net";
import * as tls from "tls";
import { credentials } from "#settings";
import { rl } from "#io";

export class tcpserver {
    constructor() {
        this.clients = [];
    }

    server?: net.Server;
    clients?: net.Socket[];

    write(message: string) {
        if (typeof this.clients === "undefined") return;
        for (const sock of this.clients) {
            sock.write(message, function (error) {
                if (error) {
                    console.error("Error trying to send message!\n", error);
                }
            });
        }
    }

    listen(port: number) {
        this.server = net.createServer();
        this.server.on("listening", () => this.onlistening());
        this.server.on("connection", (socket) => this.onconnection(socket));
        this.server.on("close", () => this.onclose());
        this.server.on("error", (err) => this.onerror(err));

        this.server.listen(port);
    }

    close() {
        if (this.clients) {
            for (const sock of this.clients) {
                sock.end();
            }
        }
        this.server?.close((err) => {
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
        const addr = this.server?.address();
        if (addr && typeof addr === "object" && "address" in addr)
            console.log(`TCP listenings on localhost:${(addr as AddressInfo).port}`);
        

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    onconnection(socket: net.Socket) {
        if (this.clients && this.clients.indexOf(socket) === -1) {
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
        if (buf.length === 0) return;
        const message = `${socket.remotePort} -> ${buf.toString()}`;
        console.log(message);
    }

    onclientend(socket: net.Socket): void {
        if (typeof this.clients === "undefined") return;
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
    declare server?: tls.Server;
    declare clients?: net.Socket[];

    override listen(port: number) {
        this.server = tls.createServer(credentials, (socket) => this.onconnection(socket));
        this.server.on("listening", () => this.onlistening());
        this.server.on("close", () => this.onclose());
        this.server.on("error", (err) => this.onerror(err));

        this.server.listen(port);
    }

    override onlistening() {
        if (typeof this.server === "undefined") return;
        const addr = this.server.address();
        console.log("teste");
        if (addr && typeof addr === "object" && "address" in addr)
            console.log(`TCP ssl listenings on localhost:${(addr as AddressInfo).port}`);        

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    override onconnection(socket: tls.TLSSocket) {
        if (this.clients && this.clients.indexOf(socket) === -1) {
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
    constructor() {}

    socket?: net.Socket;

    write(message: string) {
        this.socket?.write(message, (err) => {
            if (err)
                console.error(err);
        })
    }

    connect(port: number) {
        this.socket = new net.Socket()
        this.socket.on("connect", () => this.onconnect());
        this.socket.on("data", (data) => this.ondata(data));
        this.socket.on("end", () => this.onclose());
        this.socket.on("error", (err) => this.onerror(err));
        this.socket.connect({ host: "localhost", port });
    }

    close() {
        this.socket?.end();
        process.exit();
    }

    online(input: string) {
        if (input === "") return;
        if (input === "quit") {
            rl.close();
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
    declare socket?: tls.TLSSocket;

    override connect(port: number) {
        this.socket = tls.connect(
            {
                host: "localhost",
                port,
                rejectUnauthorized: false,
                ...credentials
            },
            () => this.onconnect()
        );
        this.socket.on("data", (data) => this.ondata(data));
        this.socket.on("end", () => this.onclose());
        this.socket.on("error", (err) => this.onerror(err));
    }

    override onconnect() {
        console.log(`TCP ssl connected at locahost:${this.socket?.localPort}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => { this.online(input); });
    }
}