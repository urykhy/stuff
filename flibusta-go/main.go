package main

import (
	"archive/zip"
	"bufio"
	"fmt"
	"io/ioutil"
	"log"
	"regexp"
	"strconv"
	"strings"

	"github.com/hanwen/go-fuse/fuse"
	"github.com/hanwen/go-fuse/fuse/nodefs"
	"github.com/hanwen/go-fuse/fuse/pathfs"
)

const (
	INPX  = "/home/ury/tmp/flibusta_fb2_local.inpx"
	STORE = "/u03/mirror/fb2.Flibusta.Net"
)

type Book struct {
	id     uint64
	size   uint64
	author string
	title  string
	seq    string
}
type BookList map[string]Book

func (b Book) String() string {
	return fmt.Sprintf("%v:%v %v - %v (%v)", b.id, b.size, b.author, b.title, b.seq)
}

var byAuthor = make(map[string]*BookList)
var replacer = strings.NewReplacer(",", ", ", "/", " ", "\"", "", "$", "", "(", "", ")", "")

func clearString(s string) string {
	s = replacer.Replace(s)
	s = strings.Trim(s, ",-: ")
	return s
}

func parseLine(s string) (result Book) {
	split := strings.Split(s, "\x04")
	v, err := strconv.ParseUint(split[5], 10, 64)
	if err != nil {
		log.Fatal("Fail to parse ID in line <" + s + ">: " + err.Error())
	}
	result.id = v
	v, err = strconv.ParseUint(split[6], 10, 64)
	if err != nil {
		log.Fatal("Fail to parse Size in line <" + s + ">: " + err.Error())
	}
	result.size = v
	result.author = split[0]
	result.title = clearString(split[2])
	result.title = result.title + ".fb2"
	result.seq = split[3]
	return
}

func find_filename(id uint64) []string {
	re := regexp.MustCompile(".*-(\\d+)-(\\d+)\\.zip")

	files, err := ioutil.ReadDir(STORE)
	if err != nil {
		log.Fatal(err)
	}

	var list []string

	for _, file := range files {
		r := re.FindAllStringSubmatch(file.Name(), -1)
		if len(r) > 0 {
			i1, _ := strconv.ParseUint(r[0][1], 10, 64)
			i2, _ := strconv.ParseUint(r[0][2], 10, 64)
			if id >= i1 && id <= i2 {
				list = append(list, file.Name())
			}
		}
	}
	return list
}

func get_book(id uint64) (nodefs.File, fuse.Status) {
	xname := fmt.Sprintf("%v.fb2", id)
	filename := find_filename(id)

	for _, x := range filename {
		log.Printf("search %v in %s", xname, x)
		reader, err := zip.OpenReader(STORE + "/" + x)
		if err != nil {
			log.Printf("fail to open zip archive: %v", err)
			return nil, fuse.ENOENT
		}
		defer reader.Close()
		for _, f := range reader.File {
			if f.Name == xname {
				log.Printf("found file %v in %v", xname, f.Name)
				file, err := f.Open()
				if err != nil {
					log.Printf("fail to open file in archive: %v", err)
					return nil, fuse.ENOENT
				}
				defer file.Close()
				buf, err := ioutil.ReadAll(file)
				if err != nil {
					log.Printf("fail to read file in archive: %v", err)
					return nil, fuse.ENOENT
				}
				nf := nodefs.NewDataFile(buf)
				return nf, fuse.OK
			}
		}
	}
	return nil, fuse.ENOENT
}

func collectBook(b Book) {
	for _, author := range strings.Split(b.author, ":") {
		b.author = clearString(author)
		e, ok := byAuthor[b.author]
		if !ok {
			tmp_e := make(BookList)
			e = &tmp_e
			byAuthor[b.author] = e
		}
		(*e)[b.title] = b
	}
}

type BookFs struct {
	pathfs.FileSystem
}

func (me *BookFs) GetAttr(name string, context *fuse.Context) (*fuse.Attr, fuse.Status) {
	//log.Printf("get attr [" + name + "]")
	if name == "" {
		return &fuse.Attr{Mode: fuse.S_IFDIR | 0755}, fuse.OK
	} else {
		elements := strings.Split(name, "/")
		books, ok := byAuthor[elements[0]]
		if ok {
			if len(elements) == 1 {
				return &fuse.Attr{Mode: fuse.S_IFDIR | 0755}, fuse.OK
			}
			b, book_ok := (*books)[elements[1]]
			if book_ok {
				return &fuse.Attr{Mode: fuse.S_IFREG | 0644, Size: b.size}, fuse.OK
			} else {
				log.Printf("no ent [" + name + "]")
				return nil, fuse.ENOENT
			}
		} else {
			log.Printf("no ent [" + name + "]")
			return nil, fuse.ENOENT
		}
	}
}

func (me *BookFs) OpenDir(name string, context *fuse.Context) (c []fuse.DirEntry, code fuse.Status) {
	//log.Printf("opendir [" + name + "]")
	if name == "" {
		for key, _ := range byAuthor {
			c = append(c, fuse.DirEntry{Name: key, Mode: fuse.S_IFDIR})
		}
		return c, fuse.OK
	}
	e, ok := byAuthor[name]
	if ok {
		for title, _ := range *e {
			c = append(c, fuse.DirEntry{Name: title, Mode: fuse.S_IFREG})
		}
		return c, fuse.OK
	}
	return nil, fuse.ENOENT
}

func (me *BookFs) Open(name string, flags uint32, context *fuse.Context) (file nodefs.File, code fuse.Status) {
	log.Printf("open [" + name + "]")

	if name == "" {
		return nil, fuse.EPERM
	} else {
		elements := strings.Split(name, "/")
		books, ok := byAuthor[elements[0]]
		if ok {
			if len(elements) == 1 {
				return nil, fuse.EPERM
			}
			b, book_ok := (*books)[elements[1]]
			if book_ok {
				log.Printf("read file %v", b.id)
				return get_book(b.id)
			} else {
				log.Printf("no ent [" + name + "]")
				return nil, fuse.ENOENT
			}
		} else {
			log.Printf("no ent [" + name + "]")
			return nil, fuse.ENOENT
		}
	}

}

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)
	log.Print("Starting")
	log.Print("Reading: " + INPX)

	reader, err := zip.OpenReader(INPX)
	if err != nil {
		log.Fatal(err)
	}
	defer reader.Close()

	for _, file := range reader.File {
		match, _ := regexp.MatchString(".*.inp", file.Name)
		if match == false {
			continue
		}
		log.Print("read: " + file.Name)

		count := 0
		fileReader, err := file.Open()
		if err != nil {
			log.Fatal(err)
		}
		defer fileReader.Close()
		scanner := bufio.NewScanner(fileReader)
		for scanner.Scan() {
			b := parseLine(scanner.Text())
			collectBook(b)
			count += 1
		}
		if err := scanner.Err(); err != nil {
			log.Fatal(err)
		}
		log.Print(fmt.Sprintf("found %v entries", count))
	}
	log.Print("ok, starting fuse mount")

	nfs := pathfs.NewPathNodeFs(&BookFs{FileSystem: pathfs.NewDefaultFileSystem()}, nil)
	server, _, err := nodefs.MountRoot("/mnt/books", nfs.Root(), nil)
	if err != nil {
		log.Fatalf("Mount fail: %v\n", err)
	}
	server.Serve()

	log.Print("done.")
}
