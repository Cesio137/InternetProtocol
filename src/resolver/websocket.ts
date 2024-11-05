import { credentials } from "#settings";

let listener: Deno.HttpServer<Deno.NetAddr>;
let ssl_listener: Deno.HttpServer<Deno.NetAddr>;
const clients = new Set<WebSocket>;

async function handler(req: Request) {
    if (req.headers.get("upgrade") !== "websocket") {
        return new Response(null, { status: 501 });
    }
    const { socket, response } = Deno.upgradeWebSocket(req);
    clients.add(socket)
    socket.addEventListener("open", () => {
        console.log("Client connected.")
    });
    socket.addEventListener("close", () => {
        clients.delete(socket)
        console.log("Client disconnected.")
    })
    socket.addEventListener("message", (event) => {
        if (typeof event.data !== "string") {
            return
        }
        const message = event.data
        console.log(message)
        if (message === "ping") {
            socket.send("pong");
            return
        }
        
        for (const client of clients) {
            if (client === socket) continue
            client.send(message)
        }
    });
    return response;
}

export function resolve(options: ResolverOptions) {
    listener = Deno.serve(
        {
            hostname: options.hostname || "127.0.0.1",
            port: options.port || 3000,
        },
        handler
    );
}

export function ssl_resolve(options: ResolverOptions) {
    const decoder = new TextDecoder("utf-8");
    ssl_listener = Deno.serve(
        {
            hostname: options.hostname || "127.0.0.1",
            port: options.port || 3000,
            cert: decoder.decode(credentials.cert),
            key: decoder.decode(credentials.key),
        },
        handler
    );
}

export function close() {
    if (listener) listener.shutdown()
    if (ssl_listener) ssl_listener.shutdown()
}
