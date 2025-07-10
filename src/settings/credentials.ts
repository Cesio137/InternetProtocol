import fs from "fs";

export const credentials = {
    key: fs.readFileSync("./ssl/key.pem"),
    cert: fs.readFileSync("./ssl/cert.pem"),
    ca: fs.readFileSync("./ssl/ca-cert.pem")
}