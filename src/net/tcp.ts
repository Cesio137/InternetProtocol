import * as net from "net";
import * as tls from "tls";
import { credentials } from "#settings";
import { rl } from "#io";

export class tcpserver {
    constructor() {
        this.server.on("listening", this.onlistening);
        this.server.on("connection", this.onconnection);
        this.server.on("close", this.onclose);
        this.server.on("error", this.onerror);
    }

    public write(message: string) {
        for (const sock of this.remotes) {
            sock.write(message, function (error) {
                if (error) {
                    console.error("Error trying to send message!\n", error);
                }
            });
        }
    }

    public listen(port: number) {
        this.server.listen(port);
    }

    public close() {
        for (const sock of this.remotes) {
            sock.end();
        }
        this.server.close((err) => {
            if (err) this.onerror(err);
        });
        process.exit();
    }

    private server: net.Server = net.createServer();
    private remotes: (net.Socket)[] = [];

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
        console.log(`TCP server listening on ${this.server.address()?.toString()}`);

        rl.on("SIGINT", this.close);
        rl.on("line", this.online);
    }

    private onconnection(socket: net.Socket) {
        if (this.remotes.indexOf(socket) === -1) {
            this.remotes.push(socket);
            console.log(
                `(${socket.remoteAddress}:${socket.remotePort} -> login, ${this.remotes.length} client(s))`
            );
        }

        socket.on("data", (buf: Buffer) => { 
            this.onremotedata(buf, socket); 
        });
        socket.on("end", () => { 
            this.onremotend(socket); 
        });
        socket.on("error", (err: Error) => {
            this.onremoterror(err, socket);
        });
    }

    private onclose() {
        console.log("bye!");
    }

    private onerror(err: Error) {
        console.error(err.message);
    }

    // Remote
    private onremotedata(buf: Buffer, socket: net.Socket) {
        if (buf.length === 0) return;
        const message = `${socket.remotePort} -> ${buf.toString()}`;
        console.log(message);
    }

    private onremotend(socket: net.Socket): void {
        const index = this.remotes.indexOf(socket);
        if (index < 0) return;
        this.remotes.splice(index);
        console.log(
            `(${socket.remoteAddress}:${socket.remotePort} -> logout, ${this.remotes.length} client(s))`
        );
    }

    private onremoterror(err: Error, socket: net.Socket): void {
        console.error(`Error ${socket.localAddress}:${socket.localPort} -> ${err}`);
    }
}

export class tcpserver_ssl {
    constructor() {
        this.server.on("listening", this.onlistening);
        this.server.on("connection", this.onconnection);
        this.server.on("close", this.onclose);
        this.server.on("error", this.onerror);
    }

    public write(message: string) {
        for (const sock of this.remotes) {
            sock.write(message, function (error) {
                if (error) {
                    console.error("Error trying to send message!\n", error);
                }
            });
        }
    }

    public listen(port: number) {
        this.server.listen(port);
    }

    public close() {
        for (const sock of this.remotes) {
            sock.end();
        }
        this.server.close((err) => {
            if (err) this.onerror(err);
        });
        process.exit();
    }

    private server: tls.Server = tls.createServer(credentials);
    private remotes: net.Socket[] = [];

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
        console.log(`TCP SSL server listening on ${this.server.address()?.toString()}`);

        rl.on("SIGINT", this.close);
        rl.on("line", this.online);
    }

    private onconnection(socket: net.Socket) {
        if (this.remotes.indexOf(socket) === -1) {
            this.remotes.push(socket);
            console.log(
                `(${socket.remoteAddress}:${socket.remotePort} -> login, ${this.remotes.length} client(s))`
            );
        }

        socket.on("data", (buf: Buffer) => { 
            this.onremotedata(buf, socket); 
        });
        socket.on("end", () => { 
            this.onremotend(socket); 
        });
        socket.on("error", (err: Error) => {
            this.onremoterror(err, socket);
        });
    }

    private onclose() {
        console.log("bye!");
    }

    private onerror(err: Error) {
        console.error(err);
    }

    // Remote
    private onremotedata(buf: Buffer, socket: net.Socket) {
        if (buf.length === 0) return;
        const message = `${socket.remotePort} -> ${buf.toString()}`;
        console.log(message);
    }

    private onremotend(socket: net.Socket): void {
        const index = this.remotes.indexOf(socket);
        if (index < 0) return;
        this.remotes.splice(index);
        console.log(
            `(${socket.remoteAddress}:${socket.remotePort} -> logout, ${this.remotes.length} client(s))`
        );
    }

    private onremoterror(err: Error, socket: net.Socket): void {
        console.error(`Error ${socket.localAddress}:${socket.localPort} -> ${err}`);
    }
}

export class tcpclient {
    constructor() {
        this.socket.on("connect", this.onconnect);
        this.socket.on("data", this.ondata);
        this.socket.on("end", this.onclose);
        this.socket.on("error", this.onerror);
    }

    public write(message: string) {
        this.socket.write(message, (err) => {
            if (err)
                console.error(err);
        })
    }

    public connnect(port: number) {
        this.socket.connect({ host: "localhost", port });
    }

    public close() {
        this.socket.end();
        process.exit();
    }

    private socket: net.Socket = new net.Socket();

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
        console.log(`TCP client connected at locahost:${this.socket.localPort}:`);

        rl.on("SIGINT", this.close);
        rl.on("line", this.online);
    }

    private ondata(buf: Buffer) {
        if (buf.length === 0) return;
        const message = buf.toString();
        console.log(message);
    }

    private onclose() {
        console.log("bye!");
    }

    private onerror(err: Error) {
        console.error(err);
    }
}

export class tcpclient_ssl {
    constructor() {
        
    }

    public write(message: string) {
        if (!this.socket) {
            console.error("Socket not connected.");
            return;
        }
        this.socket.write(message, (err) => {
            if (err) console.error(err);
        });
    }

    public connect(port: number) {
        this.socket = tls.connect(
            {
                host: "localhost",
                port,
                ...credentials
            },
            () => this.onconnect()
        );
        this.socket.on("data", (buf) => this.ondata(buf));
        this.socket.on("end", () => this.onclose());
        this.socket.on("error", (err) => this.onerror(err));
    }

    public close() {
        if (this.socket) {
            this.socket.end();
        }
        process.exit();
    }

    private socket?: tls.TLSSocket;

    private online = (input: string) => {
        if (input === "") return;
        if (input === "quit") {
            rl.close();
            this.close();
            return;
        }
        this.write(input);
    };

    private onconnect() {
        if (!this.socket) return;
        console.log(`TCP SSL client connected at ${this.socket.localAddress}:${this.socket.localPort}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", this.online);
    }

    private ondata(buf: Buffer) {
        if (buf.length === 0) return;
        const message = buf.toString();
        console.log(message);
    }

    private onclose() {
        console.log("bye!");
    }

    private onerror(err: Error) {
        console.error(err);
    }
}