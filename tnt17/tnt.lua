box.cfg {listen = 2090}
box.schema.user.grant('guest', 'read,write,execute', 'universe')

box.schema.user.create('user1', {password = 'password1'})
box.schema.user.grant('user1', 'read', 'universe')

s = box.schema.space.create('test')
s:create_index('primary', {type = 'hash', parts = {1, 'unsigned'}})
s:insert({1, 'Roxette'})
s:insert({2, 'Scorpions', 2015})
s:insert({3, 'Ace of Base', 1993})

box.space.test:create_index('secondary', {type = 'tree', parts = {2, 'string'}, unique = false})
box.space.test:insert{10, 'NightWish'}
box.space.test:insert{11, 'NightWish'}
box.space.test:insert{12, 'NightWish'}

function get_by_ids(args)
    local res = {}
    for _,v in ipairs(args) do
        local t = box.space.test:select{v}
        if #t > 0 then
            table.insert(res, box.tuple.new{t[1][1], t[1][2]})
        end
    end
    return unpack(res)
end

s = box.schema.space.create('cache')
s:create_index('primary', {type = 'hash', parts = {1, 'string'}})

-- box.space.test.index.primary:select(0, {iterator = "ALL", limit=10})
-- get_by_ids({1,2,4,5,6,7,8,9,10})
