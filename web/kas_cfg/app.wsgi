from paste.deploy import loadapp
from paste.script.util.logging_config import fileConfig as logging_file_config

import os

config = os.path.join(os.path.dirname(__file__), 'production.ini')
logging_file_config(os.path.join(os.path.dirname(__file__), 'production.ini'))

application = loadapp('config:%s' % config)

