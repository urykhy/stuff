#!/usr/bin/env python

from forecastiopy import *
import codecs

api_key = "d78952060b4571ee9cdccda4f3e04dfc"
lat = 55.7522
lng = 37.6156

def print_forecast(dfile, tom):
    print >> dfile, tom["summary"]
    print >> dfile, tom["temperatureMin"]
    print >> dfile, tom["temperatureMax"]
    print >> dfile, tom["windSpeed"]
    #print >> dfile, tom.precipAccumulation
    print >> dfile, tom["icon"]


f = ForecastIO.ForecastIO(api_key, latitude=lat, longitude=lng, units="si")
cur = FIOCurrently.FIOCurrently(f)

dfile = codecs.open("/tmp/.forecast.txt", "w", "utf-8")
print >> dfile, cur.summary
print >> dfile, cur.temperature
#print >> dfile, cur.icon

# cast for today
print_forecast(dfile, f.daily["data"][0])
# tomorrow
print_forecast(dfile, f.daily["data"][1])

