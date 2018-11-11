package bucket

import (
    "errors"
    "sync"
)

const (
    INSERT = 0
    DELETE = 1
    REDO_LEN = 128
)

type Operation int

type Act struct {
    operation Operation
    data      string
    serial    int
}

type Redo struct {
    data    []*Act
    serial  int
}

func CreateRedo() *Redo {
    return &Redo {
        data   :make([]*Act, REDO_LEN),
        serial :0,
        mutex  :sync.Mutex{},
    }
}

func (me *Redo) Append(op Operation, data string) (error) {
    me.data = append(me.data, &Act{op, data, me.serial})
    me.serial++
    return nil
}

func (me *Redo) Restore() (error) {
    return errors.New("Not implemented")
}

func (me *Redo) Dump() (error) {
    return errors.New("Not implemented")
}
