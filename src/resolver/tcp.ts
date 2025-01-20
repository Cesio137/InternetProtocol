import * as net from "net";
import * as tls from "tls";
import { credentials } from "#settings";
import { rl } from "#io";

// Server

const server = net.createServer(socket);
server.on("error", error);
const ssl_server = tls.createServer(credentials, socket);
ssl_server.on("error", error);

const remotes: (net.Socket | tls.TLSSocket)[] = [];

function socket(socket: net.Socket | tls.TLSSocket) {
    if (remotes.indexOf(socket) === -1) {
        remotes.push(socket);
        console.log(
            `(${socket.remoteAddress}:${socket.remotePort} -> login, ${remotes.length} client(s))`
        );
    }

    socket.on("data", function (data: Buffer | String) {
        if (data.length === 0) return;
        const response = `${socket.remotePort} -> ${data.toString()}`;
        for (const remote of remotes) {
            if (remote.remotePort === socket.remotePort) continue;
            remote.write(response, function (error) {
                if (error) {
                    console.error(
                        `erroro trying send message: ${error.message}`
                    );
                }
            });
        }
    });

    socket.on("end", function () {
        const index = remotes.indexOf(socket);
        if (index < 0) return;
        remotes.splice(index);
        console.log(
            `(${socket.remoteAddress}:${socket.remotePort} -> logout, ${remotes.length} client(s))`
        );
    });

    socket.on("close", function (haderroror: boolean) {
        const index = remotes.indexOf(socket);
        if (index < 0) return;
        remotes.splice(index);
        console.log(
            `(${socket.remoteAddress}:${socket.remotePort} -> logout, ${remotes.length} client(s))`
        );
    });

    socket.on("error", (error: Error) => {
        console.error("Socket erroror:", error);
    });

    rl.on("SIGINT", function () {
        socket.end();
        console.log("bye!");
        process.exit();
    });
    rl.on("line", function (input: string) {
        if (input === "") return;
        if (input === "quit") {
            socket.end();
            rl.close();
            console.log("\nbye!");
            process.exit();
            return;
        }
        const message = Buffer.from(input);
        for (const remote of remotes) {
            remote.write(message, function (error) {
                if (error) {
                    console.error("Error trying to send message!\n", error);
                    return;
                }
            });
        }
    });
}

export function resolve(port: number) {
    server.listen(port, () => {
        console.log(`TCP server listening on locahost:${port}`);
    });
    
}

export function ssl_resolve(port: number) {
    ssl_server.listen(port, () => {
        console.log(`TCP SSL server listening on localhost:${port}`);
    });
}

// Client

const client = new net.Socket();
client.on("error", error);

function listener(socket: net.Socket | tls.TLSSocket) {
    console.log(
        `TCP connected at adress: ${socket.remoteAddress}:${socket.remotePort}`
    );
    console.log(
        "Type and press 'Enter' to send a message or 'quit' to close socket"
    );

    socket.on("data", function (data) {
        const message = `${socket.remotePort} -> ${data.toString()}`;
        console.log(message);
    });

    socket.on("close", function (hadError) {
        socket.end();
        console.log("bye!");
        process.exit();
    });

    rl.on("SIGINT", function () {
        socket.end();
        console.log("bye!");
        process.exit();
    });
    rl.on("line", function (input: string) {
        if (input === "") return;
        if (input === "quit") {
            socket.end();
            rl.close();
            console.log("\nbye!");
            process.exit();
            return;
        }
        const message = Buffer.from(input);
        socket.write(message, function (error) {
            if (error) {
                console.error("Error trying to send message!\n", error);
                return;
            }
        });
    });
}

export function connect(port: number) {
    client.connect({ host: "127.0.0.1", port }, function() {
        listener(client);
    });
}

export function ssl_connect(port: number) {
    process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
    const ssl_client = tls.connect(
        {
            host: "127.0.0.1",
            port: port,
            cert: credentials.cert,
            key: credentials.key,
            ca: credentials.ca
        },
        function () {
            ssl_client.on("error", error);
            listener(ssl_client);
        }
    );
}

function error(error: Error) {
    console.error("Error:", error);
    server.close();
    client.end();
    console.log("bye!");
    process.exit();
}
