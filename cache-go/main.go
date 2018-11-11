package main

import (
    "log"

    "./api"
    "./bucket"
)

var storage *bucket.OneStorage

func handle_set(args []string) (*string, error) {
    return (*storage).Set(args[1], args[2])
    //return nil, errors.New("Not implemented")
}
func handle_get(args []string) (*string, error) {
    return (*storage).Get(args[1])
}
func handle_del(args []string) (*string, error) {
    return (*storage).Del(args[1])
}

var commands = api.HandlerMap {
	"set":    handle_set,
	"get":    handle_get,
	"del":    handle_del,
}

func main() {
    log.SetFlags(log.LstdFlags | log.Lmicroseconds)

    config := bucket.CreateConfig()
    log.Printf("Starting with root: %v", config.Root)

    storage = bucket.CreateOne(config.Root, config.Master)
    api.Start(&commands, config.Port);
}
