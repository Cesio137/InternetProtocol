import fs from "fs";

export const credentials = {
    key: fs.readFileSync("key.pem"),
    cert: fs.readFileSync("cert.pem"),
    ca: fs.readFileSync("ca-cert.pem")
}