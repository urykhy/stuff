
function conky_audacious_title_1()
    local utf8 = require 'lua-utf8'
    local a = conky_parse('${audacious_title 160}')
    a = string.gsub(a,"%s+", " ")
    return utf8.sub(a, 0, 38)
end
function conky_audacious_title_2()
    local utf8 = require 'lua-utf8'
    local a = conky_parse('${audacious_title 160}')
    a = string.gsub(a,"%s+", " ")
    return utf8.sub(a, 39, 76)
end
function conky_battery_state()
    local a = conky_parse('${battery}')
    word, _ = string.match(a,"(%w+)(.+)")
    return word
end
function conky_uptime()
    local a = conky_parse('$uptime_short')
    return string.format("%11s",a)
end
function conky_kernel_version()
    local a = conky_parse('$kernel')
    return string.format("%11s",a)
end
function conky_temp()
    local file = io.open("/sys/class/hwmon/hwmon2/temp2_input", "r")
    if file then
        res = file:read("*number")
        file:close()
        return res / 1000
    end
    return "NX"
end
function conky_mb_temp()
    local file = io.open("/sys/class/hwmon/hwmon2/temp3_input", "r")
    if file then
        res = file:read("*number")
        file:close()
        return res / 1000
    end
    return "NX"
end

function conky_disk_dirty()
    local res = ""
    local resn = 0
    for line in io.lines("/proc/meminfo") do
        local key, value = string.match(line, "(.+):%s*(%d+)")
        if key == "Dirty" then
            resn = resn + value
        end
        if key == "Writeback" then
            resn = resn + value
        end
    end
    if resn < 9999 then
        res = string.format("%iKb", resn)
    else
        res = string.format("%iMb", round(resn/1024))
    end
    return res
end

global_disk_util = {}
function get_disk_util(name)
    for line in io.lines("/proc/diskstats") do
        local value = string.match(line, ".+ "..name.." %d+ %d+ %d+ %d+ %d+ %d+ %d+ %d+ %d+ (%d+)")
        if value then
            return value
        end
    end
    return 0
end

-- expected call rate - once in 5 seconds
function conky_disk_util(name)
    if global_disk_util[name] then
        cdu = get_disk_util(name)
        s = (cdu - global_disk_util[name]) / 5 / 10
        global_disk_util[name] = cdu
        return s
    else
        global_disk_util[name] = get_disk_util(name)
        return 0
    end
end

function round(num, idp)
    local mult = 10^(idp or 0)
    return math.floor(num * mult + 0.5) / mult
end
function conky_freq()
    local sum = 0
    for var=0,5,1 do
        local file = io.open("/sys/devices/system/cpu/cpu"..tostring(var).."/cpufreq/scaling_cur_freq", "r")
        sum = sum + file:read("*number")
        file:close()
    end
    return round(sum / 1000 / 1000.0 / 6.0, 1)
end
function conky_date()
    local utf8 = require 'lua-utf8'
    local a = conky_parse('${time %A, %e %B}')
    return utf8.sub(a, 0, 20)
end

function conky_fan1()
    local file = io.open("/sys/class/hwmon/hwmon2/fan1_input", "r")
    local sum = file:read("*number")
    file:close()
    return sum
end
function conky_fan2()
    local file = io.open("/sys/class/hwmon/hwmon2/fan2_input", "r")
    local sum = file:read("*number")
    file:close()
    return sum
end
function conky_fan3()
    local file = io.open("/sys/class/hwmon/hwmon2/fan5_input", "r")
    local sum = file:read("*number")
    file:close()
    return sum
end

function conky_weather1()
    local file = io.open("/tmp/.forecast.txt")
    if file then
        txt1 = file:read()
        temp1 = tonumber(file:read())
        skip1 = file:read()
        file:close()
        return string.format("%-.14s, %s°C",txt1,round(temp1))
    end
    return ""
end
function conky_weather2()
    local file = io.open("/tmp/.forecast.txt")
    if file then
        file:read()
        file:read()
        file:read()
        tmin = tonumber(file:read("*number"))
        tmax = tonumber(file:read("*number"))
        file:close()
        return string.format("Forecast: %d..%d°C",round(tmax),round(tmin))
    end
    return ""
end
function conky_weather_tom()
    local file = io.open("/tmp/.forecast.txt")
    if file then
        for var=0,6,1 do
          file:read()
        end
        desc = file:read()
        tmin = tonumber(file:read("*number"))
        tmax = tonumber(file:read("*number"))
        file:close()
        return string.format("Tomorrow: %d..%d°C",round(tmax),round(tmin))
    end
    return ""
end

g_lan_www = 0
g_lan_bulk = 0
function lan_diff(www, bulk)
    local d_www = www
    local d_bulk = bulk
    if g_lan_www > 0 then
        d_www = www - g_lan_www
    end
    g_lan_www = www
    if g_lan_bulk > 0 then
        d_bulk = bulk - g_lan_bulk
    end
    g_lan_bulk = bulk
    local sum = d_www + d_bulk
    d_www = math.floor((d_www / sum) * 100)
    d_bulk = math.floor((d_bulk / sum) * 100)
    return d_www,d_bulk
end

lan_www = 0
lan_bulk = 0
function conky_lan_update()
    local www_bytes = 0
    local next_line_www = 0
    local bulk_bytes = 0
    local next_line_bulk = 0
    local f = assert (io.popen ("tc -s qdisc show dev ifb0"))
    for line in f:lines() do
        if next_line_www == 1 then
            www_bytes = tonumber(string.match(line, ".*Sent (%d+) bytes.*"))
            next_line_www = 0
        end
        if next_line_bulk == 1 then
            bulk_bytes = tonumber(string.match(line, ".*Sent (%d+) bytes.*"))
            next_line_bulk = 0
        end
        if string.find(line, "qdisc sfq 101:") then
            next_line_www = 1
        end
        if string.find(line, "qdisc sfq 102:") then
            next_line_bulk = 1
        end
    end
    lan_www, lan_bulk = lan_diff(www_bytes, bulk_bytes)
    f:close()
    return ""
end

function conky_lan_www()
    return lan_www
end
function conky_lan_bulk()
    return lan_bulk
end


