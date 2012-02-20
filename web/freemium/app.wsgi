HTTP_X_URL_SCHEME = "https"

from paste.deploy import loadapp
from paste.script.util.logging_config import fileConfig as logging_file_config

import os

config = os.path.join(os.path.dirname(__file__), 'production.ini')
logging_file_config(os.path.join(os.path.dirname(__file__), 'production.ini'))

_application = loadapp('config:%s' % config)

def application(environ, start_response):
    environ['wsgi.url_scheme'] = HTTP_X_URL_SCHEME
    return _application(environ, start_response)

