#!/usr/bin/env python

import sys

# FIXME Check the thread-safetyness of that code.

# Imports
import logging
from sqlalchemy import create_engine
from sqlalchemy.exc import OperationalError
import elixir
from elixir import setup_all
elixir.metadata = None
elixir.session = None

# Get logger.
log = logging.getLogger(__name__)

##### Config

__all__ = ['kcd']

from kwmo.model.meta import metadata, Session

# Models
from kwmo.model.user_workspace_settings import UserWorkspaceSettings

elixir_model_imports = (('kwmo.model.chat_message', 'ChatMessage'),
                        ('kwmo.model.chat_request', 'ChatRequest'),
                        ('kwmo.model.kfs_node', 'KfsNode'),
                        ('kwmo.model.user', 'User'),
                        ('kwmo.model.vnc_session', 'VncSession'),
                        ('kwmo.model.workspace', 'Workspace'),
                        ('kwmo.model.ws_request', 'WSRequest'),
                        ('kwmo.model.kfs_upload_status', 'KfsUploadStatus'))

# Get url function.
from kwmo.lib.config import get_local_db_url
get_url = get_local_db_url

# Set elixir default options.
elixir.options_defaults.update({ 'shortnames' : True, 'autoload' : False })

##### /Config

# Initialize the global engine variable. The engine can change at any time,
# so don't use that directly.
_engine = None

# Get current engine. 
# WARNING: The engine can change at any time, so using it directly is not reliable.
def get_engine():
    return _engine

# Import models and set them as module globals.
def import_models():
    # Define or import your model(s)  here.
    # MAGIC: elixir entities use a metaclass that fill an entities list (elixir.entities) at class definition,
    # so models need to be imported between the metadata definition and the setup_all() call.
    # ie: from x.model.user import User
    for module_name, model_name in elixir_model_imports:
        # Import model locally.
        i = __import__(module_name, fromlist=[model_name])

        # Set model in module.
        model = getattr(i, model_name)
        setattr(sys.modules[__name__], model_name, model)

# Create a new sqlalchemy engine, and close old engine pool if needed.
def new_engine(): 
    global _engine

    # Keep a reference to the old engine (if any).
    _old_engine = _engine

    # Get the database url.
    url = get_url()
    log.debug("SQLAlchemy URL: '" + str(url) + "'")

    # Create new engine.
    log.debug("Creating new sqlalchemy engine.")
    _engine = create_engine(url, convert_unicode=True, encoding='latin1')

    # Bind metadata to new engine.
    metadata.bind = _engine

    if _old_engine:
        # Close engine pool.
        log.debug("Disposing old engine pool.")
        _old_engine.pool.dispose()

# Reset the thread-local SQLAlchemy session. It is still usable after that.
def reset_thread_local_session():
    Session.remove()

# Initialize sqlalchemy in the current thread (idempotent).
# In pylons, use this in the __before__ method of the lib/base.py controller.
def init_local():
    # Change session engine bind, if needed.
    if Session.get_bind(None) != _engine:
        log.debug("init_local(): [Re-]configuring session bind.")
        Session.configure(bind=_engine)

    # Detect stale pool database connections.
    # In case of failure, the pool will reconnect automatically.
    # This is not needed when using only models, but it is needed when using either the engine
    # or the session directly to do raw queries.
    try:
        Session.execute("SELECT 1")

    except OperationalError, e:
        if getattr(e, 'connection_invalidated') and e.connection_invalidated:
            log.warn("init_local(): database connection was invalidated.")
        else:
            log.warn("init_local(): database exception: '%s'." % ( str(e) ) )

    # Reset thread-local session, in case it was left in an unclean state.
    log.debug("init_local(): resetting thread-local session.")
    reset_thread_local_session()

# Clean sqlalchemy in the current thread (idempotent).
# In pylons, use this in a finally clause in lib/base.py.
def clean_local():
    # Reset thread-local session.
    log.debug("clean_local(): resetting thread-local session.")
    reset_thread_local_session()

# Initialize globally (use at the application start).
def init_model(junk=None):
    # Connect.
    new_engine()

    # Import models.
    import_models()

    # Setup elixir entities.
    setup_all()

    # Bind session to the engine.
    Session.configure(bind=_engine)

 
