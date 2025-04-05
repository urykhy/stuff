#!/usr/bin/env python
# -*- coding: UTF-8 -*-

#
# samlib2fb2.py <url>
#

import base64
import os
import re
import requests
import sys

from xml.etree.ElementTree import *
from xml.dom.minidom import parse, parseString
from datetime import datetime, date, time


def prettify(elem):
    s = tostring(elem, "UTF-8")
    x = parseString(s)  # minidom
    return x.toprettyxml(indent="  ", encoding="UTF-8")


def strip(s):
    return re.sub("<[^<]+?>", "", s)


def cleanup(s):
    s = s.replace("&amp;nbsp;", "")
    s = s.replace("&nbsp;", "")
    s = s.replace("<b>", "")
    s = s.replace("</b>", "")
    s = s.replace("<i>", "")
    s = s.replace("<I>", "")
    s = s.replace("</i>", "")
    s = s.replace("</I>", "")
    s = s.replace("<br>", "")
    s = s.replace("<BR>", "")
    s = s.replace("<p>", "")
    s = s.replace("<P>", "")
    s = s.replace("<dd>", "")
    s = s.strip()
    return s


def push_text(parent, s):
    if len(s) < 2:
        return
    t = SubElement(parent, "p")
    t.text = s


author_line = "<li>&copy; Copyright "


def find_author(l):
    for i in l:
        if author_line == i[: len(author_line)]:
            i = i[:-4]
            return i[i.rindex(">") + 1 :]


book_start = "Собственно произведение"
cut_at_start = "<dd>&nbsp;&nbsp;"
title_anchor = "<center><h2>"


def find_title(l):
    for i in l:
        if -1 != i.find(title_anchor):
            return i[
                i.find(title_anchor) + len(title_anchor) : i.find("</h2>")
            ].replace("/", "-")


def get_html(url):
    WINDOWS_LINE_ENDING = "\r\n"
    UNIX_LINE_ENDING = "\n"
    r = requests.get(url)
    r.raise_for_status()
    s = r.content.decode("windows-1251")
    s = s.replace(WINDOWS_LINE_ENDING, UNIX_LINE_ENDING)
    return s.splitlines()


def get_image(name):
    r = requests.get(f"http://samlib.ru/{name}")
    r.raise_for_status()
    data = base64.b64encode(r.content)
    missing_padding = len(data) % 4
    if missing_padding:
        data += b"=" * (4 - missing_padding)
    return data


book_end = "<!-- ----------------------------------------------- -->"


def book():
    content = get_html(sys.argv[1])
    top = Element("FictionBook")
    top.set("xmlns", "http://www.gribuser.ru/xml/fictionbook/2.0")
    top.set("xmlns:l", "http://www.w3.org/1999/xlink")

    desc = SubElement(top, "description")
    ti = SubElement(desc, "title-info")
    SubElement(ti, "annotation").text = "Аннотация"
    author = SubElement(ti, "author")
    SubElement(author, "first-name").text = find_author(content)
    bt = find_title(content)
    print(f"title: {bt}")
    SubElement(ti, "book-title").text = bt

    docinfo = SubElement(top, "document-info")
    prog = SubElement(docinfo, "program-used")
    prog.text = "samlib2fb2.py"
    date = SubElement(docinfo, "date")
    date.text = datetime.utcnow().strftime("%a, %d %b %Y %H:%M:%S GMT")
    src = SubElement(docinfo, "src-url")
    src.text = sys.argv[1]

    body = SubElement(top, "body")
    title = SubElement(body, "title")
    title.text = bt
    section = SubElement(body, "section")
    s_title = SubElement(section, "title")
    push_text(s_title, "название первой главы")
    dump = False
    for i in content:
        if dump == True:
            if -1 != i.find(book_end):
                dump = False
                break
            if -1 != i.find(cut_at_start):
                i = i[len(cut_at_start) : -1].strip()
            i = cleanup(i)
            if (-1 != i.find("Глава ") and len(i.strip()) < 64) or (
                -1 != i.find("align='center'")
            ):
                section = SubElement(body, "section")
                s_title = SubElement(section, "title")
                push_text(s_title, strip(i))
                continue
            if i.find("<img src") > -1:
                name = re.search('<img src="([^"]+)".*', i).group(1)
                data = get_image(name)
                name = os.path.basename(name)  # filename as link name
                i64 = SubElement(top, "binary")
                i64.set("id", name)
                i64.set("content-type", "image/jpeg")  # FIXME
                i64.text = data.decode("ASCII")
                i = SubElement(section, "image")
                i.set("l:href", "#" + str(name))
                continue
            i = strip(i)
            if len(i) > 1 and i[0] == "<":
                continue
            push_text(section, i)
        if dump == False and -1 != i.find(book_start):
            dump = True
    text = prettify(top)
    out = bt + ".fb2"
    print("writing fb2 to", out)
    f = open(out, "wb")
    f.write(text)


if __name__ == "__main__":
    book()
