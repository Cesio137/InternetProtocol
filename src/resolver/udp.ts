let listener: Deno.DatagramConn;
const clients = new Set<Deno.NetAddr>();

export async function resolve(options: ResolverOptions) {
    listener = Deno.listenDatagram({
        hostname: options.hostname || "127.0.0.1",
        port: options.port || 3000,
        transport: "udp",
    });
    const { hostname, port } = listener.addr as Deno.NetAddr;
    console.log(`UDP listening at adress: ${hostname}:${port}`);

    for await (const [msg, addr] of listener) {
        try {
            const client_addr = addr as Deno.NetAddr;
            const data = new TextDecoder().decode(msg);
            console.log(`${client_addr.port} -> ${data}`);
            switch (data) {
                case "login":
                    clients.add(client_addr)
                    continue
                case "logout":
                    clients.delete(client_addr)
                    continue
            }
            for (const client of clients) {
                if (client.port === client_addr.port) continue
                await listener.send(msg, client)
            }
        } catch (err) {
            if (err instanceof Error) {
                console.error(`Error receiving message: ${err.message}`);
            } else {
                console.error(`Unknown error: ${err}`);
            }
        }
    }
}

export function close() {
    if (listener)
        listener.close()
}