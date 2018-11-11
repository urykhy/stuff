package bucket

import (
    "log"
    "fmt"
    "errors"
)

const (
    INSERT = 0
    DELETE = 1
    REDO_LEN = 128
)

type Operation int

type Act struct {
    operation Operation
    key       *string
    value     *string
    serial    int
}

type Redo struct {
    data    []*Act
    serial  int
    root    string
}

func CreateRedo(root string) *Redo {
    return &Redo {
        data   :make([]*Act, REDO_LEN),
        serial :0,
        root   :root,
    }
}

func (me *Redo) Append(op Operation, key *string, value *string) (error) {
    if op == INSERT {
        log.Print(fmt.Sprintf("redo: INSERT %v for [%v:%v]", me.serial, *key, *value))
    } else {
        log.Print(fmt.Sprintf("redo: DELETE %v for [%v]", me.serial, *key))
    }
    me.data = append(me.data, &Act{op, key, value, me.serial})
    me.serial++
    return nil
}

func (me *Redo) Restore() (error) {
    return errors.New("Not implemented")
}

func (me *Redo) Dump() (error) {
    return errors.New("Not implemented")
}
