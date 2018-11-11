package bucket

import (
    "errors"
    "sync"
)

type OneStorage struct {
    data map[string]*string
    mutex sync.Mutex
}

func CreateOne() *OneStorage {
    return &OneStorage {
        data  :make(map[string]*string),
        mutex :sync.Mutex{},
    }
}

func (me *OneStorage) Get(key string) (*string, error) {
    me.mutex.Lock()
    defer me.mutex.Unlock()

    data, ok := me.data[key]
    if ok {
        return data, nil
    } else {
        return nil,errors.New("not found")
    }
}

func (me *OneStorage) Set(key string, data string) (*string, error) {
    me.mutex.Lock()
    defer me.mutex.Unlock()

    me.data[key] = &data
    s := "OK"
    return &s, nil
}

func (me *OneStorage) Del(key string) (*string, error) {
    me.mutex.Lock()
    defer me.mutex.Unlock()

    _, ok := me.data[key]
    if ok {
        delete(me.data, key)
        s := "OK"
        return &s, nil
    } else {
        return nil,errors.New("not found")
    }
}
