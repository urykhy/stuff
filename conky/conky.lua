
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


