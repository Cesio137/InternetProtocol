export const credentials = {
    key: Deno.readFileSync("server-key.pem"),
    cert: Deno.readFileSync("server-cert.pem"),
    ca: Deno.readFileSync("ca-cert.pem")
}