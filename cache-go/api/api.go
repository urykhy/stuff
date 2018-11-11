package api

import (
    "fmt"
    "log"
    "net"
    "bufio"
    "strings"
    "errors"
)

type Handler func(args []string) (*string, error)
type HandlerMap map[string]Handler

func handle_command(line string, handler *HandlerMap) (*string, error) {
    split := strings.Split(line, " ")
    if len(split) < 1 {
        return nil, errors.New("empty command")
    }
    cmd, cmd_ok := (*handler)[split[0]]
    if !cmd_ok {
        return nil, errors.New("not supported")
    }
    return cmd(split)
}

func request_handler(conn net.Conn, handler *HandlerMap) {
    defer conn.Close()
    reader := bufio.NewReader(conn)
    writer := bufio.NewWriter(conn)
    for {
        line, err := reader.ReadString('\n')
        if err != nil {
            log.Printf("read error: %v", err)
            return
        }
        line = strings.Trim(line, "\r\n")
        if line == "quit" {
            return
        }
        log.Print(fmt.Sprintf("got command: [%v]", line))

        var reply *string
        reply, err = handle_command(line, handler)
        if err != nil {
            reply_str := "ERR: " + err.Error()
            reply = &reply_str
        } else {
            reply_str := "OK: " + *reply
            reply = &reply_str
        }
        _, err = writer.WriteString(*reply + "\r\n")
        if err != nil {
            log.Printf("write error: %v", err)
            return
        }
        err = writer.Flush()
        if err != nil {
            log.Printf("flush error: %v", err)
            return
        }
    }
}

func Start(handler *HandlerMap, port int) {
    log.Printf("Listen port: %v", port)
    sock, err := net.Listen("tcp", fmt.Sprintf(":%v", port))

    if err != nil {
            log.Fatalf("listen error: %v", err)
    }

    for {
            conn, err := sock.Accept()
            if err != nil {
                    log.Fatalf("accept error: %v", err)
            }
            log.Print("New client")
            go request_handler(conn, handler)
    }
}
