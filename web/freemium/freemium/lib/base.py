"""The base Controller API

Provides the BaseController class for subclassing.
"""
import logging
from pylons.controllers import WSGIController
from pylons.templating import render_mako as render
from pylons.controllers.util import redirect_to
from pylons import config, url, session, tmpl_context as c
import freemium.model as model
from freemium.lib.config import detect_cached_config_change, get_cached_master_config

log = logging.getLogger(__name__)

class BaseController(WSGIController):
    def __before__(self, action, controller):

        log.debug("Request to %s.%s" % \
            ( controller, action ) )

        # Detect changes in configuration.
        def config_has_changed():
            model.new_engine()
        detect_cached_config_change(config_has_changed, config['master_file_path'])

        # Get cached master configuration.
        c.mc = get_cached_master_config()

        # Initialize model in local thread.
        model.init_local()

    def __call__(self, environ, start_response):
        """Invoke the Controller"""
        # WSGIController.__call__ dispatches to the Controller method
        # the request is routed to. This routing information is
        # available in environ['pylons.routes_dict']

        # Insert any code to be run per request here.

        try:
            return WSGIController.__call__(self, environ, start_response)
        finally:
            model.clean_local()
