#!/usr/bin/env python
# -*- coding: UTF-8 -*-

#
# wget <url>
# cat fname | iconv -f windows-1251 -t utf-8 | samlib2fb2.py
#

import sys
from xml.etree.ElementTree import *
from xml.dom.minidom import parse, parseString
from datetime import datetime, date, time

def prettify(elem):
	rough_string = tostring(elem, "UTF-8")
	reparsed = parseString(rough_string)
	return reparsed.toprettyxml(indent="  ", encoding="UTF-8")

def cleanup(s):
	s = s.replace("&amp;nbsp;","")
	s = s.replace("&nbsp;","")
	s = s.replace("<b>","")
	s = s.replace("</b>","")
	s = s.replace("<i>","")
	s = s.replace("<I>","")
	s = s.replace("</i>","")
	s = s.replace("</I>","")
	s = s.replace("<br>","")
	s = s.replace("<BR>","")
	s = s.replace("<p>","")
	s = s.replace("<P>","")
	s = s.replace("<dd>","")
        s = s.strip()
	return s

def push_text(parent, s):
	if len(s) < 2:
		return
	t = SubElement(parent, 'p')
	t.text = s.decode('utf-8')

author_line="<li>&copy; Copyright "
def find_author(l):
	for i in l:
		if author_line == i[:len(author_line)]:
			i = i[:-5]
			print i
			return i[i.rindex('>')+1:]

book_start="Собственно произведение"
cut_at_start="<dd>&nbsp;&nbsp;"
title_anchor="<center><h2>"
def find_title(l):
	for i in l:
		if -1 != i.find(title_anchor):
			return i[i.find(title_anchor)+len(title_anchor):i.find("</h2>")].replace("/","-")

book_end="<!--------------------------------------------------->"
def book():
	content = sys.stdin.readlines()
	top = Element('FictionBook')

	desc = SubElement(top, 'description')
	ti = SubElement(desc, 'title-info')
	SubElement(ti, 'annotation').text="Аннотация".decode('utf-8')
	author = SubElement(ti, 'author')
	SubElement(author, 'first-name').text = find_author(content).decode('utf-8')
	bt = find_title(content)
	SubElement(ti, 'book-title').text = bt.decode('utf-8')

	docinfo = SubElement(top, 'document-info')
	prog = SubElement(docinfo, 'program-used')
	prog.text = "samlib2fb2.py"
	date = SubElement(docinfo, 'date')
	date.text = datetime.utcnow().strftime("%a, %d %b %Y %H:%M:%S GMT")

	body = SubElement(top, 'body')
	title = SubElement(body, 'title')
	title.text = bt.decode('utf-8')
	section = SubElement(body, 'section')
	s_title = SubElement(section, 'title')
	push_text(s_title, "название первой главы")
	dump = False
	for i in content:
		if dump == True:
			if -1 != i.find(book_end):
				dump = False
				break
			if -1 != i.find(cut_at_start):
				i = i[len(cut_at_start):-1].strip()
			i = cleanup(i)
			if (-1 != i.find("Глава ") and len(i.strip()) < 64) or (-1 != i.find("align='center'")):
				section = SubElement(body, 'section')
				s_title = SubElement(section, 'title')
				push_text(s_title, i)
				continue
			push_text(section, i)
		if dump == False and -1 != i.find(book_start):
			dump = True
	text = prettify(top)
	out = bt+".fb2"
	print "writing fb2 to",out
	f = open(out, 'w')
	f.write(text)

if __name__ == "__main__":
	book()

