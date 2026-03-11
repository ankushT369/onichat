package main

import (
	"context"
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net"
	"os"
	"strings"
	"sync"
	"time"

	tea "github.com/charmbracelet/bubbletea"

	"github.com/cretz/bine/tor"
	_ "github.com/ipsn/go-libtor"
)

const (
	CMD_GET_USERNAME = 0x01
	CMD_SEND_MESSAGE = 0x02
)

//////////////////////////////////////////////////////////////////
// SERVER
//////////////////////////////////////////////////////////////////

type Client struct {
	conn     net.Conn
	username string
}

type Server struct {
	clients map[net.Conn]*Client
	lock    sync.Mutex
}

var charset = []rune("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")

func randomUsername() string {
	length := rand.Intn(10) + 5
	name := make([]rune, length)
	for i := range name {
		name[i] = charset[rand.Intn(len(charset))]
	}
	return string(name)
}

func (s *Server) broadcast(sender net.Conn, username string, payload []byte) {

	s.lock.Lock()
	defer s.lock.Unlock()

	for conn := range s.clients {

		if conn == sender {
			continue
		}

		usrlen := uint64(len(username))
		msglen := uint64(len(payload))

		binary.Write(conn, binary.LittleEndian, usrlen)
		conn.Write([]byte(username))

		binary.Write(conn, binary.LittleEndian, msglen)
		conn.Write(payload)
	}
}

func (s *Server) handle(conn net.Conn) {

	defer conn.Close()

	client := &Client{conn: conn}

	s.lock.Lock()
	s.clients[conn] = client
	s.lock.Unlock()

	for {

		var cmd uint8
		err := binary.Read(conn, binary.LittleEndian, &cmd)
		if err != nil {
			break
		}

		switch cmd {

		case CMD_GET_USERNAME:

			username := randomUsername()
			client.username = username

			resp := make([]byte, 17)
			resp[0] = 0
			copy(resp[1:], username)

			conn.Write(resp)

			fmt.Println("Client connected:", username)

		case CMD_SEND_MESSAGE:

			var length uint64
			err := binary.Read(conn, binary.LittleEndian, &length)
			if err != nil {
				return
			}

			payload := make([]byte, length)
			conn.Read(payload)

			s.broadcast(conn, client.username, payload)

		default:
			return
		}
	}

	s.lock.Lock()
	delete(s.clients, conn)
	s.lock.Unlock()

	fmt.Println("Client disconnected:", client.username)
}

func runServer() {

	rand.Seed(time.Now().UnixNano())

	server := &Server{
		clients: make(map[net.Conn]*Client),
	}

	ctx := context.Background()

	fmt.Println("Starting embedded Tor...")

	t, err := tor.Start(ctx, nil)
	if err != nil {
		log.Fatal(err)
	}

	defer t.Close()

	onion, err := t.Listen(ctx, &tor.ListenConf{
		Version3: true,
		RemotePorts: []int{
			80,
		},
	})

	if err != nil {
		log.Fatal(err)
	}

	fmt.Println("Onion service started:")
	fmt.Println(onion.ID + ".onion:80")

	for {
		conn, err := onion.Accept()
		if err != nil {
			continue
		}

		go server.handle(conn)
	}
}

//////////////////////////////////////////////////////////////////
// CLIENT
//////////////////////////////////////////////////////////////////

type msgPacket struct {
	user string
	text string
}

type model struct {
	conn     net.Conn
	username string
	messages []string
	input    string
}

func connect(addr string) (net.Conn, string) {

	var conn net.Conn
	var err error

	if strings.Contains(addr, ".onion") {

		fmt.Println("Starting embedded Tor...")

		ctx := context.Background()

		t, err := tor.Start(ctx, nil)
		if err != nil {
			log.Fatal(err)
		}

		dialer, err := t.Dialer(ctx, nil)
		if err != nil {
			log.Fatal(err)
		}

		conn, err = dialer.Dial("tcp", addr)
		if err != nil {
			log.Fatal(err)
		}

	} else {

		conn, err = net.Dial("tcp", addr)
		if err != nil {
			log.Fatal(err)
		}
	}

	cmd := uint8(CMD_GET_USERNAME)

	conn.Write([]byte{cmd})

	resp := make([]byte, 17)

	io.ReadFull(conn, resp)

	username := strings.TrimRight(string(resp[1:]), "\x00")

	return conn, username
}

func recvLoop(conn net.Conn, ch chan msgPacket) {

	for {

		var usrlen uint64

		err := binary.Read(conn, binary.LittleEndian, &usrlen)
		if err != nil {
			return
		}

		uname := make([]byte, usrlen)

		io.ReadFull(conn, uname)

		var length uint64

		binary.Read(conn, binary.LittleEndian, &length)

		payload := make([]byte, length)

		io.ReadFull(conn, payload)

		ch <- msgPacket{
			user: string(uname),
			text: string(payload),
		}
	}
}

func sendMessage(conn net.Conn, text string) {

	payload := []byte(text)

	conn.Write([]byte{CMD_SEND_MESSAGE})

	binary.Write(conn, binary.LittleEndian, uint64(len(payload)))

	conn.Write(payload)
}

func (m model) Init() tea.Cmd {
	return nil
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {

	switch msg := msg.(type) {

	case tea.KeyMsg:

		switch msg.String() {

		case "ctrl+c":
			return m, tea.Quit

		case "enter":

			if len(m.input) > 0 {

				sendMessage(m.conn, m.input)

				now := time.Now().Format("15:04:05")

				m.messages = append(
					m.messages,
					fmt.Sprintf("[me %s] %s", now, m.input),
				)

				m.input = ""
			}

		case "backspace":

			if len(m.input) > 0 {
				m.input = m.input[:len(m.input)-1]
			}

		default:

			m.input += msg.String()
		}

	case msgPacket:

		now := time.Now().Format("15:04:05")

		m.messages = append(
			m.messages,
			fmt.Sprintf("[%s %s] %s", msg.user, now, msg.text),
		)
	}

	return m, nil
}

func (m model) View() string {

	s := "Onichat\n"
	s += "────────────────────────\n\n"

	start := 0

	if len(m.messages) > 20 {
		start = len(m.messages) - 20
	}

	for _, msg := range m.messages[start:] {
		s += msg + "\n"
	}

	s += "\n> " + m.input

	return s
}

func runClient(addr string) {

	conn, username := connect(addr)

	fmt.Println("connected as", username)

	ch := make(chan msgPacket)

	go recvLoop(conn, ch)

	m := model{
		conn:     conn,
		username: username,
	}

	p := tea.NewProgram(m)

	go func() {
		for msg := range ch {
			p.Send(msg)
		}
	}()

	if err := p.Start(); err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
}

//////////////////////////////////////////////////////////////////
// MAIN
//////////////////////////////////////////////////////////////////

func main() {

	if len(os.Args) < 2 {

		fmt.Println("Usage:")
		fmt.Println("  onichat --server")
		fmt.Println("  onichat --client host:port")
		return
	}

	switch os.Args[1] {

	case "--server":

		runServer()

	case "--client":

		if len(os.Args) < 3 {
			fmt.Println("Usage: onichat --client host:port")
			return
		}

		runClient(os.Args[2])

	default:

		fmt.Println("Unknown option:", os.Args[1])
	}
}
