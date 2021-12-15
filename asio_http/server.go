package main

import (
	"fmt"
	"net/http"

	"golang.org/x/net/http2"
	"golang.org/x/net/http2/h2c"
)

// based on https://github.com/thrawn01/h2c-golang-example/blob/master/cmd/server/main.go
func main() {
	h2s := &http2.Server{}

	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("hello from go"))
	})

	server := &http.Server{
		Addr:    "0.0.0.0:2081",
		Handler: h2c.NewHandler(handler, h2s),
	}

	fmt.Printf("Listening ...\n")
	server.ListenAndServe()
}
