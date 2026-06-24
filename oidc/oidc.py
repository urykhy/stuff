#!/usr/bin/env python

# docker run --rm authelia/authelia:4.39 authelia crypto hash generate pbkdf2 --variant sha512 --random --random.length 16 --random.charset rfc3986
# Digest: $pbkdf2-sha512$310000$00HJaLnNRerqPXMhFo9gow$fTjjHUdfqcFIZj38.N1ojI7ZVI.qQecCbMoN6vpGEayMqPmkTzmHs8YHex5fcRcMuZYm/CW33g5Bz5WWcxQ8gw
SECRET = "2Dy9e6LReCIrUMq1"
CLIENT_ID = "py-oidc"
REDIRECT_PATH = "/auth/callback"
BASE_URL = "https://photo.elf.dark:8098"
AUTH_URL = "https://authelia.elf.dark"
SSL_DIR = "/etc/ssl/elf/"

import random
import string

import cherrypy
import requests
from jinja2 import BaseLoader, Environment
from requests.auth import HTTPBasicAuth


def random_string(length=16):
    return "".join(random.choices(string.ascii_letters + string.digits, k=length))


index_page = """
<!doctype html>
<html>
  <head>
    <title>Login Demo</title>
  </head>
  <body>
    <h1>Sign in with OIDC</h1>

    {% if user %}
      <p>Signed in as <strong>{{ user }}</strong></p>
    {% else %}
      <p>You are not signed in.</p>
      <a class="btn" href="/login">Sign in</a>
    {% endif %}
  </body>
</html>
"""


class Auth(object):
    @cherrypy.expose()
    def callback(self, **params):
        state = params["state"]
        code = params["code"]
        cherrypy.log(f"callback with state {state=} and {code=}")

        if state == cherrypy.session.get("state") and code is not None:
            r = requests.post(
                AUTH_URL + "/api/oidc/token",
                auth=HTTPBasicAuth(CLIENT_ID, SECRET),
                headers={
                    "Content-Type": "application/x-www-form-urlencoded",
                },
                data=f"grant_type=authorization_code&code={code}&redirect_uri={BASE_URL + REDIRECT_PATH}",
            )
            r.raise_for_status()
            cherrypy.log(f"token response: {r.text=}")
            r = requests.get(
                AUTH_URL + "/api/oidc/userinfo",
                headers={
                    "Authorization": f"Bearer {r.json()['access_token']}",
                },
            )
            r.raise_for_status()
            cherrypy.log(f"user info response: {r.text=}")
            cherrypy.session["user"] = r.json()["email"]
            raise cherrypy.HTTPRedirect(BASE_URL)
        else:
            raise cherrypy.HTTPError(501)


class App(object):
    @cherrypy.expose("Home")
    def index(self):
        user = cherrypy.session.get("user")
        cherrypy.log(f"index for user {user=}")
        t = Environment(loader=BaseLoader).from_string(index_page)
        return t.render({"user": user})

    @cherrypy.expose()
    def login(self, **params):
        state = random_string()
        cherrypy.session["state"] = state
        raise cherrypy.HTTPRedirect(
            AUTH_URL
            + f"/api/oidc/authorization?client_id={CLIENT_ID}&redirect_uri={BASE_URL + REDIRECT_PATH}&response_type=code&scope=openid%20profile%20email&state={state}"
        )


if __name__ == "__main__":
    cfg = {
        "server.socket_host": "0.0.0.0",
        "server.socket_port": 8098,
        "tools.sessions.on": True,
        "server.ssl_module": "builtin",
        "server.ssl_certificate": SSL_DIR + "server-photo.elf.dark.crt",
        "server.ssl_private_key": SSL_DIR + "server-photo.elf.dark.key",
    }
    cherrypy.tree.mount(Auth(), "/auth")
    cherrypy.config.update(cfg)
    cherrypy.quickstart(App())
