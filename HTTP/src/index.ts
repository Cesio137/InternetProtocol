import express, { Express, Request, Response } from 'express'
type Handle = {
    event:string,
    message:string
}

const PORT: number = 3000;
const app: Express = express();

app.get('/', function(req:Request, res:Response){
    const JsonMessage: Handle = {event: 'http_request', message: 'Received request from nodejs!'};
    res.send(JSON.stringify(JsonMessage));
});

app.listen(PORT, function(){
    console.log(`listening at adress localhost:${PORT}`);
});