#!/usr/bin/env python3

# need python3-mysqldb python3-sqlalchemy-ext

import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.dirname(SCRIPT_DIR))

import time
import zipfile

from flibusta.common import *

mirror_path = "/u03/mirror/fb2.Flibusta.Net"
init_logger()
logger = logging.getLogger(__name__)

engine = engine()


def indexer(fname, books):
    with Session(engine) as session:
        start_ts = time.time()
        for batch in batched(books, 1000):
            ids = []
            for a in batch:
                [author, title, id, size, date] = a
                ids.append(id)
            already = Book.already(session, ids)

            for a in batch:
                [author, title, id, size, date] = a
                # FIXME: cut tail on space, ise field size from Book
                if len(author) > 127:
                    logger.debug(f"too long author: {author}")
                    author = author[:127]
                if len(title) > 256:
                    logger.debug(f"too long title: {author}")
                    title = title[255]
                if id not in already:
                    b = Book(id=id, author=author, title=title, size=size, date=date)
                    session.add(b)
        session.commit()
        logger.info(f"{fname} processed in {time.time() - start_ts:.2f} seconds")


def read_inp(z, fname):
    with z.open(fname) as f:
        books = []
        for l in f:
            l = l.strip().decode(sys.stdin.encoding)
            # http://forum.home-lib.net/index.php?showtopic=16
            # AUTHOR;GENRE;TITLE;SERIES;SERNO;LIBID;SIZE;FILE;DEL;EXT;DATE;LANG;LIBR ATE;KEYWORDS;
            (
                au,
                genre,
                name,
                seq,
                _None,
                id,
                size,
                _None,
                f_del,
                _None,
                date,
                _None,
            ) = l.split("\04", 11)
            if f_del == "0":
                au = au.rstrip(":,-")
                au = au.replace(",", ", ")
                if len(seq):
                    books.append((au, seq + "/" + name, id, size, date))
                else:
                    books.append((au, name, id, size, date))
        indexer(fname, books)


with zipfile.ZipFile(mirror_path + "/flibusta_fb2_local.inpx") as zfile:
    for info in zfile.infolist():
        if info.filename.endswith(".inp"):
            read_inp(zfile, info.filename)
