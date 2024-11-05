import { credentials } from "#settings";

let listener: Deno.TcpListener
let ssl_listener: Deno.TlsListener
const clients = new Set<Deno.Conn>

async function handleConn(conn: Deno.TcpConn | Deno.TlsConn) {
    clients.add(conn)
    try {
        const buffer = new Uint8Array(1400);
        while (true) {
            const bytesRead = await conn.read(buffer);
            if (bytesRead === null) {
                console.log(`${conn.remoteAddr.port} -> Closed connection`);
                clients.delete(conn)
                break;
            }
            const message = new TextDecoder().decode(
                buffer.subarray(0, bytesRead)
            );
            console.log(`${conn.remoteAddr.port} -> ${message}`);
            switch (message) {
                case "login":
                    clients.add(conn)
                    continue
                case "logout":
                    clients.delete(conn)
                    continue
            }
            const data = new TextEncoder().encode(message)
            for (const client of clients) {
                if (client === conn) continue
                await client.write(data);
            }
        }
    } catch (err) {
        console.error("Error reading from connection:", err);
    } finally {
        conn.close();
    }
}

export async function resolve(options: ResolverOptions) {
    listener = Deno.listen({
        hostname: options.hostname || "127.0.0.1",
        port: options.port || 3000,
    });
    console.log(
        `TCP server listening on ${listener.addr.hostname}:${listener.addr.port}`
    );
    for await (const conn of listener) {
        handleConn(conn);
    }
}

export async function ssl_resolve(options: ResolverOptions) {
    const decoder = new TextDecoder("utf-8");
    ssl_listener = Deno.listenTls({
        hostname: options.hostname || "127.0.0.1",
        port: options.port || 3000,
        cert: decoder.decode(credentials.cert),
        key: decoder.decode(credentials.key)
    });
    console.log(
        `TCP server listening on ${ssl_listener.addr.hostname}:${ssl_listener.addr.port}`
    );
    for await (const conn of ssl_listener) {
        handleConn(conn);
    }
}


export function close() {
    if (listener)
        listener.close()
    if (ssl_listener)
        ssl_listener.close()
}