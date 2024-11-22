import fs from "fs";

export const credentials = {
    key: fs.readFileSync("server-key.pem"),
    cert: fs.readFileSync("server-cert.pem"),
    ca: fs.readFileSync("ca-cert.pem")
}