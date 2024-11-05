import { credentials } from "#settings";

let listener: Deno.HttpServer<Deno.NetAddr>;
let ssl_listener: Deno.HttpServer<Deno.NetAddr>;

async function handler(req: Request) {
    if (req.method === "GET") {
        return new Response("Hello, world!", {
            status: 200,
            headers: { "content-type": "text/plain" },
        });
    } else {
        return new Response("Method Not Allowed", {
            status: 405,
            headers: { "content-type": "text/plain" },
        });
    }
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
    if (listener) listener.shutdown();
    if (ssl_listener) ssl_listener.shutdown();
}
