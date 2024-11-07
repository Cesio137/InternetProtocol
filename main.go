package main

import (
	"encoding/json"
	"fmt"
	"net"

	"github.com/google/go-cmp/cmp"
)

type LoginOrder struct {
	Event   string `json:"event"`
	ID      int    `json:"id"`
	Players []int  `json:"players"`
}

type RegisterOrder struct {
	Event  string `json:"event"`
	Player int    `json:"player"`
}

type LogoutOrder struct {
	Event  string `json:"event"`
	Player int    `json:"player"`
}

type MessageOrder struct {
	Event     string  `json:"event"`
	Condition int     `json:"condition"` // 0-none, 1-owner, 2-skip+owner
	ID        int     `json:"id"`
	PositionX float32 `json:"positionX"`
	PositionY float32 `json:"positionY"`
	PositionZ float32 `json:"positionZ"`
	RotationZ float32 `json:"rotationZ"`
}

const IP string = "localhost"
const PORT int = 3000

func main() {
	host, err := Bootstrap(IP, PORT)
	if err != nil {
		fmt.Println("Erro trying to create UDP server:", err)
		return
	}
	host.AddLoginHandler(OnLogin)
	host.AddLogoutHandler(OnLogout)
	host.AddMessageHandler(OnMessage)
	defer host.Conn.Close()
	fmt.Println("UDP server initialized at port", PORT)
	host.Listen()
}

func OnLogin(host *Host, client *net.UDPAddr) {
	fmt.Printf("%v -> login\n", client.Port)
	l := len(host.Clients)
	fmt.Printf("total clients -> %v\n", l)
	if l < 2 {
		login_order := LoginOrder{
			Event:   "login_order",
			ID:      client.Port,
			Players: make([]int, 0),
		}
		loginData, err := json.Marshal(login_order)
		if err != nil {
			fmt.Println("Error trying to encode login order:", err)
		}
		_, err = host.Conn.WriteToUDP(loginData, client)
		if err != nil {
			fmt.Println("Error trying to replicate login data:", err)
		}
		return
	}

	id_players := make([]int, 0)
	for _, c := range host.Clients {
		if cmp.Equal(*c, *client) {
			continue
		}
		id_players = append(id_players, c.Port)
	}
	login_order := LoginOrder{
		Event:   "login_order",
		ID:      client.Port,
		Players: id_players,
	}
	loginData, err := json.Marshal(login_order)
	if err != nil {
		fmt.Println("Error trying to encode login data:", err)
		return
	}

	register_order := RegisterOrder{
		Event:  "register_order",
		Player: client.Port,
	}
	registerData, err := json.Marshal(register_order)
	if err != nil {
		fmt.Println("Error trying to encode login data:", err)
		return
	}

	for _, c := range host.Clients {
		if cmp.Equal(*c, *client) {
			_, err = host.Conn.WriteToUDP(loginData, c)
			if err != nil {
				fmt.Println("Error trying to replicate login data:", err)
			}
		} else {
			_, err = host.Conn.WriteToUDP(registerData, c)
			if err != nil {
				fmt.Println("Error trying to replicate register data:", err)
			}
		}
	}
}

func OnLogout(host *Host, client *net.UDPAddr) {
	fmt.Printf("%v -> logout\n", client.Port)
	l := len(host.Clients)
	fmt.Printf("total clients -> %v\n", l)
	if l < 1 {
		return
	}
	logout_order := LogoutOrder{
		Event:  "logout_order",
		Player: client.Port,
	}
	logoutData, err := json.Marshal(logout_order)
	if err != nil {
		fmt.Println("Error trying to encode logout order:", err)
	}
	for _, c := range host.Clients {
		_, err = host.Conn.WriteToUDP(logoutData, c)
		if err != nil {
			fmt.Println("Error trying to replicate register data:", err)
		}
	}
}

func OnMessage(host *Host, client *net.UDPAddr, data []byte) {
	var message MessageOrder
	err := json.Unmarshal(data, &message)
	if err != nil {
		fmt.Println("Error trying to parse JSON:", err)
		return
	}
	message.ID = client.Port
	messageData, err := json.Marshal(message)
	if err != nil {
		fmt.Println("Error trying to encode JSON:", err)
		return
	}
	if message.Condition == 1 {
		_, err = host.Conn.WriteToUDP(messageData, client)
		if err != nil {
			fmt.Println("Error trying to send message:", err)
		}
		return
	}
	for _, c := range host.Clients {
		switch message.Condition {
		case 0:
			_, err = host.Conn.WriteToUDP(messageData, c)
			if err != nil {
				fmt.Println("Error trying to send message:", err)
			}
		case 2:
			if !cmp.Equal(*c, *client) {
				_, err = host.Conn.WriteToUDP(messageData, c)
				if err != nil {
					fmt.Println("Error trying to send message:", err)
				}
			}
		}
	}
}
