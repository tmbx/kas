# Hey emacs, this is -*- Python -*-

import site,sys,os

sys.path.append(os.path.dirname(__file__))

import teambox_cfg

for p in teambox_cfg.SYS_PATHS:
    sys.path.append(p)

for s in teambox_cfg.SITE_DIRS:
    site.addsitedir(s);

from paste.deploy import loadapp
from paste.script.util.logging_config import fileConfig as logging_file_config

import os

config = os.path.join(os.path.dirname(__file__), 'production.ini')
logging_file_config(os.path.join(os.path.dirname(__file__), 'production.ini'))

application = loadapp('config:%s' % config)

