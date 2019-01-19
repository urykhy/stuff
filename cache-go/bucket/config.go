package bucket

import (
    "flag"
)

type Config struct {
    Root   string
    Port   int
    Master string
}

func CreateConfig() *Config {
    rootPtr := flag.String("root", "/tmp", "data root")
    portPtr := flag.Int("port", 2081, "listen port")
    masterPtr := flag.String("master", "", "master host:port")
    flag.Parse()

    return &Config {
        Root: *rootPtr,
        Port: *portPtr,
        Master: *masterPtr,
    }
}
