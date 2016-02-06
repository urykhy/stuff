
function conky_audacious_title_1()
    local utf8 = require 'lua-utf8'
    local a = conky_parse('${audacious_title 140}')
    a = string.gsub(a,"%s+", " ")
    return utf8.sub(a, 0, 38)
end
function conky_audacious_title_2()
    local utf8 = require 'lua-utf8'
    local a = conky_parse('${audacious_title 140}')
    a = string.gsub(a,"%s+", " ")
    return utf8.sub(a, 39, 70)
end
function conky_battery_state()
    local a = conky_parse('${battery}')
    word, _ = string.match(a,"(%w+)(.+)")
    return word
end
function conky_uptime()
    local a = conky_parse('$uptime_short')
    return string.format("%13s",a)
end
function conky_kernel_version()
    local a = conky_parse('$kernel')
    return string.format("%13s",a)
end
function conky_temp()
    local file = io.open("/sys/class/thermal/thermal_zone7/temp", "r")
    if file then
        res = file:read("*number")
        file:close()
        return res / 1000
    end
    return "NX"
end
function conky_mb_temp()
    local file = io.open("/sys/class/thermal/thermal_zone9/temp", "r")
    if file then
        res = file:read("*number")
        file:close()
        return res / 1000
    end
    return "NX"
end

function conky_disk_dirty()
    res = ""
    for line in io.lines("/proc/meminfo") do
        local key, value = string.match(line, "(.+):%s*(%d+)")
        if key == "Dirty" then
            res = string.format("Dirty %7iKb",value)
            break
        end
    end
    return res
end
function conky_disk_wb()
    res = ""
    for line in io.lines("/proc/meminfo") do
        local key, value = string.match(line, "(.+):%s*(%d+)")
        if key == "Writeback" then
            res = string.format("W-back %6iKb",value)
            break
        end
    end
    return res
end

function get_disk_util()
    for line in io.lines("/proc/diskstats") do
        local value = string.match(line, ".+ sda %d+ %d+ %d+ %d+ %d+ %d+ %d+ %d+ %d+ (%d+)")
        if value then
            return value
        end
    end
    return 0
end

-- expected call rate - once in 5 seconds
function conky_disk_util()
    if global_disk_util then
        cdu = get_disk_util()
        s = (cdu - global_disk_util) / 5 / 10
        global_disk_util = cdu
        return s
    else
        global_disk_util = get_disk_util()
        return 0
    end
end

function round(num, idp)
    local mult = 10^(idp or 0)
    return math.floor(num * mult + 0.5) / mult
end
function conky_freq()
    local sum = 0
    for var=0,3,1 do
        local file = io.open("/sys/devices/system/cpu/cpu"..tostring(var).."/cpufreq/scaling_cur_freq", "r")
        sum = sum + file:read("*number")
        file:close()
    end
    return round(sum / 1000 / 1000.0 / 4.0, 1)
end
function conky_date()
    local utf8 = require 'lua-utf8'
    local a = conky_parse('${time %A, %e %B}')
    return utf8.sub(a, 0, 20)
end

function conky_weather1()
    local file = io.open("/tmp/.forecast.txt")
    if file then
        txt1 = file:read()
        temp1 = file:read()
        skip1 = file:read()
        file:close()
        return string.format("%-.13s, %s°C",txt1,round(temp1))
    end
    return ""
end
function conky_weather2()
    local file = io.open("/tmp/.forecast.txt")
    if file then
        file:read()
        file:read()
        file:read()
        tmin = file:read("*number")
        tmax = file:read("*number")
        file:close()
        return string.format("Forecast: %d..%d°C",round(tmax),round(tmin))
    end
    return ""
end
function conky_weather_tom()
    local file = io.open("/tmp/.forecast.txt")
    if file then
        for var=0,7,1 do
          file:read()
        end
        desc = file:read()
        tmin = file:read("*number")
        tmax = file:read("*number")
        file:close()
        return string.format("Tomorrow: %d..%d°C",round(tmax),round(tmin))
    end
    return ""
end



