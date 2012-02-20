# -*- coding: utf-8 -*-
"""Setup the freemium application"""
import logging

from freemium.config.environment import load_environment

log = logging.getLogger(__name__)

from pylons import config
from elixir import *
from freemium import model as model

def setup_app(command, conf, vars):
    """Place any commands to setup freemium here"""
    load_environment(conf.global_conf, conf.local_conf)
    model.metadata.create_all()

    # Initialisation here ... this sort of stuff:

    # some_entity = model.Session.query(model.<modelfile>.<Some_Entity>).get(1)
    # e.g. foo = model.Session.query(model.identity.User).get(1)
    # from datetime import datetime
    # some_entity.poked_on = datetime.now()
    # model.Session.add(some_entity)
    model.Session.commit()
