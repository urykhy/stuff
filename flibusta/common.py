import logging
import os


def init_logger():
    LOG_FMT = "%(asctime)s.%(msecs)03d %(name)s (%(funcName)s:%(lineno)d) [%(levelname)s] %(message)s"
    LOG_DATEFMT = "%Y-%m-%d %H:%M:%S"
    logging.basicConfig(
        format=LOG_FMT,
        datefmt=LOG_DATEFMT,
        level=os.environ.get("LOG_LEVEL", "INFO").upper(),
    )
    logger = logging.getLogger(__name__)
    logging.getLogger("sqlalchemy").setLevel(logging.ERROR)


from sqlalchemy import create_engine, String, Integer, Date, Column, select
from sqlalchemy.engine import URL
from sqlalchemy.orm import declarative_base, Session
from sqlalchemy import Index

Base = declarative_base()


class Book(Base):
    __tablename__ = "flibusta_book"
    id = Column(Integer, nullable=False, primary_key=True)
    author = Column(String(128), nullable=False)
    title = Column(String(256), nullable=False)
    size = Column(Integer, nullable=False)
    date = Column(Date, nullable=False)

    def __repr__(self) -> str:
        return f"Book(id={self.id!r}, author={self.author!r}, title={self.title!r}, size={self.size!r}, date={self.date!r}"

    @staticmethod
    def already(session, ids):
        a = session.execute(select(Book.id).where(Book.id.in_(ids))).all()
        return set(str(x[0]) for x in a)


fti_author = Index("author", Book.author, mysql_prefix="FULLTEXT")
fti_title = Index("title", Book.title, mysql_prefix="FULLTEXT")


def engine():
    url = URL.create(
        "mysql",
        username=os.environ["MYSQL_USER"],
        password=os.environ["MYSQL_PASS"],
        host=os.environ["MYSQL_HOST"],
        port=os.environ["MYSQL_PORT"],
        database="test",
    )
    engine = create_engine(url)
    Base.metadata.create_all(engine)
    return engine


def batched(iterable, n=1):
    l = len(iterable)
    for ndx in range(0, l, n):
        yield iterable[ndx : min(ndx + n, l)]
