package main

import (
	"context"
	"flag"
	"fmt"
	"net"
	"os"
	"strings"
	"sync"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/client"
	"github.com/miekg/dns"
	log "github.com/sirupsen/logrus"
)

const (
	DOCKER      = ".docker."
	LEN_DOCKER  = len(DOCKER)
	IN_ADDR     = ".in-addr.arpa."
	LEN_IN_ADDR = len(IN_ADDR)
)

func getLogger() *log.Logger {
	logger := &log.Logger{
		Out:       os.Stderr,
		Formatter: new(log.JSONFormatter),
		Level:     log.DebugLevel,
	}
	return logger
}

var logger = getLogger()

type entry struct {
	name string
	ip   string
}

type storage struct {
	mu  sync.Mutex
	dns map[string]entry
}

var dnsMap = storage{dns: make(map[string]entry)}

func (s *storage) insert(ID string, dnsName string, ipAddr string) {
	l := logger.WithField("module", "docker")
	l.Info("add service ", dnsName, " at ", ipAddr)
	s.mu.Lock()
	defer s.mu.Unlock()
	s.dns[ID] = entry{name: dnsName, ip: ipAddr}
}

func (s *storage) remove(ID string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	e, ok := s.dns[ID]
	if ok {
		l := logger.WithField("module", "docker")
		l.Info("drop service ", e.name, " at ", e.ip)
		delete(s.dns, ID)
	}
}

func (s *storage) resolve(dnsName string) string {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, e := range s.dns {
		if dnsName == e.name {
			return e.ip
		}
	}
	return ""
}

func (s *storage) ptr(ipAddr string) string {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, e := range s.dns {
		if ipAddr == e.ip {
			return e.name
		}
	}
	return ""
}

func (s *storage) dump() {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, e := range s.dns {
		fmt.Println(e.name, e.ip)
	}
}

func inspect(cli *client.Client, ID string) {
	ctx := context.Background()
	info, err := cli.ContainerInspect(ctx, ID)
	if err != nil {
		panic(err)
	}

	dnsName := func() string {
		service := info.Config.Labels["com.docker.compose.service"]
		project := info.Config.Labels["com.docker.compose.project"]
		if project != "" && service != "" {
			if project == service {
				return service
			}
			return service + "." + project
		}
		if len(info.Name) > 1 && info.Name[0] == '/' {
			return info.Name[1:]
		}
		return info.Config.Hostname
	}()

	ipAddr := func() string {
		for _, v := range info.NetworkSettings.Networks {
			if v.IPAddress != "" {
				return v.IPAddress
			}
		}
		return ""
	}()
	if dnsName != "" && ipAddr != "" {
		dnsMap.insert(ID, dnsName, ipAddr)
	}
}

func makeAResponse(name string, addr string, m *dns.Msg) {
	m.Authoritative = true
	rr := new(dns.A)
	rr.Hdr = dns.RR_Header{Name: name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: uint32(60)}
	rr.A = net.ParseIP(addr)
	m.Answer = []dns.RR{rr}
}

func makePtrResponse(name string, addr string, m *dns.Msg) {
	m.Authoritative = true
	rr := new(dns.PTR)
	rr.Hdr = dns.RR_Header{Name: name, Rrtype: dns.TypePTR, Class: dns.ClassINET, Ttl: uint32(60)}
	rr.Ptr = addr
	m.Answer = []dns.RR{rr}
}

func handleDnsRequest(w dns.ResponseWriter, r *dns.Msg) {
	l := logger.WithField("module", "dns")

	name := r.Question[0].Name
	l.Info("request ", name)
	m := new(dns.Msg)

	if r.Question[0].Qtype == dns.TypeA && strings.HasSuffix(name, DOCKER) {
		addr := dnsMap.resolve(name[:len(name)-LEN_DOCKER])
		if addr != "" {
			l.Info("resolved ", name, " to ", addr)
			m.SetRcode(r, dns.RcodeSuccess)
			makeAResponse(r.Question[0].Name, addr, m)
		} else {
			l.Warn("not found ", name)
			m.SetRcode(r, dns.RcodeNameError)
		}
	} else {
		if r.Question[0].Qtype == dns.TypePTR && strings.HasSuffix(name, IN_ADDR) {
			name, err := dns.ReverseAddr(name[:len(name)-LEN_IN_ADDR])
			if err != nil {
				l.Error("fail to reverse addr ", name)
				m.SetRcode(r, dns.RcodeFormatError)
			} else {
				name = name[:len(name)-LEN_IN_ADDR]
				addr := dnsMap.ptr(name)
				if addr != "" {
					l.Warn("resolved ", name, " to ", addr)
					addr += DOCKER
					m.SetRcode(r, dns.RcodeSuccess)
					makePtrResponse(r.Question[0].Name, addr, m)
				} else {
					l.Warn("not found ", name)
					m.SetRcode(r, dns.RcodeNameError)
				}
			}
		} else {
			m.SetRcode(r, dns.RcodeRefused)
		}
	}

	err := w.WriteMsg(m)
	if err != nil {
		l.Error("fail to write dns response: ", err)
	}
}

func main() {
	listen := flag.String("a", ":2053", "address to listen for DNS requests (udp)")
	dump := flag.Bool("d", false, "dump information on running containers and exit")
	flag.Parse()

	logger.Info("started")
	ctx := context.Background()

	cli, err := client.NewClientWithOpts(client.FromEnv, client.WithAPIVersionNegotiation())
	if err != nil {
		panic(err)
	}
	info, err := cli.Info(ctx)
	if err != nil {
		panic(err)
	}
	logger.Info("connected to docker ", info.ServerVersion)

	logger.Info("list running containers")
	cont, err := cli.ContainerList(ctx, types.ContainerListOptions{})
	if err != nil {
		panic(err)
	}
	for _, c := range cont {
		inspect(cli, c.ID)
	}

	if *dump {
		dnsMap.dump()
		return
	}

	logger.Info("start dns server at ", *listen)
	dns.HandleFunc(".", handleDnsRequest)
	go func() {
		srv := &dns.Server{Addr: *listen, Net: "udp"} // configure port
		err := srv.ListenAndServe()
		if err != nil {
			panic(err)
		}
	}()

	logger.Info("wait to events ...")
	msgs, errs := cli.Events(ctx, types.EventsOptions{})
	for {
		select {
		case err := <-errs:
			panic(err)
		case msg := <-msgs:
			if msg.Type == "container" && msg.Action == "start" {
				inspect(cli, msg.ID)
			} else if msg.Type == "container" && (msg.Action == "die" || msg.Action == "stop") {
				dnsMap.remove(msg.ID)
			}
		}
	}
}
