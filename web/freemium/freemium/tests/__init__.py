"""Pylons application test package

This package assumes the Pylons environment is already loaded, such as
when this script is imported from the `nosetests --with-pylons=test.ini`
command.

This module initializes the application via ``websetup`` (`paster
setup-app`) and provides the base testing objects.
"""
from unittest import TestCase

from paste.deploy import loadapp
from paste.script.appinstall import SetupCommand
from pylons import config, url
from routes.util import URLGenerator
from webtest import TestApp

import pylons.test
from elixir import *
from freemium.model import *
from freemium.model import meta
from freemium import model as model
from sqlalchemy import engine_from_config

__all__ = ['environ', 'url', 'TestController', 'TestModel']


# Invoke websetup with the current config file
# SetupCommand('setup-app').run([config['__file__']])

# additional imports ...
import os
from paste.deploy import appconfig
from freemium.config.environment import load_environment

here_dir = os.path.dirname(__file__)
conf_dir = os.path.dirname(os.path.dirname(here_dir))

test_file = os.path.join(conf_dir, 'test.ini')
conf = appconfig('config:' + test_file)
load_environment(conf.global_conf, conf.local_conf)
environ = {}

engine = engine_from_config(config, 'sqlalchemy.')
model.init_model(engine)
metadata = elixir.metadata
Session = elixir.session = meta.Session

class Individual(Entity):
    """Table 'Individual'.

    >>> me = Individual('Groucho')

    # 'name' field is converted to lowercase
    >>> me.name
    'groucho'
    """
    name = Field(String(20), unique=True)
    favorite_color = Field(String(20))

    def __init__(self, name, favorite_color=None):
        self.name = str(name).lower()
        self.favorite_color = favorite_color

setup_all()

def setup():
    pass

def teardown():
    pass

class TestModel(TestCase):
    def setUp(self):
        setup_all(True)
    
    def tearDown(self):
        drop_all(engine)


class TestController(TestCase):

    def __init__(self, *args, **kwargs):
        if pylons.test.pylonsapp:
            wsgiapp = pylons.test.pylonsapp
        else:
            wsgiapp = loadapp('config:%s' % config['__file__'])
        self.app = TestApp(wsgiapp)
        url._push_object(URLGenerator(config['routes.map'], environ))
        TestCase.__init__(self, *args, **kwargs)
