# -*- coding: utf-8 -*-
"""Setup the kwmo application"""
import logging

from kwmo.config.environment import load_environment

log = logging.getLogger(__name__)

from pylons import config
import elixir
from elixir import *
from kwmo import model as model

def setup_app(command, conf, vars):
    """Place any commands to setup kwmo here"""
    load_environment(conf.global_conf, conf.local_conf)

    model.metadata.create_all()
    
    # Initialisation here ... this sort of stuff:

    # some_entity = model.Session.query(model.<modelfile>.<Some_Entity>).get(1)
    # e.g. foo = model.Session.query(model.identity.User).get(1)
    # from datetime import datetime
    # some_entity.poked_on = datetime.now()
    # model.Session.add(some_entity)
    model.Session.commit()
