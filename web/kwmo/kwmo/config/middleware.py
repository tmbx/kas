"""Pylons middleware initialization"""
from beaker.middleware import CacheMiddleware
from paste.cascade import Cascade
from paste.registry import RegistryManager
from paste.urlparser import StaticURLParser
from paste.deploy.converters import asbool
from pylons import config
from pylons.middleware import ErrorHandler, StatusCodeRedirect
from pylons.wsgiapp import PylonsApp
from pylons.templating import render_mako
from routes.middleware import RoutesMiddleware
from kwmo.config.environment import load_environment
import kwmo.lib.middlewares
from kwmo.lib.kwmo_session import KwmoSessionMiddleware

def make_app(global_conf, full_stack=True, static_files=True, **app_conf):
    """Create a Pylons WSGI application and return it

    ``global_conf``
        The inherited configuration for this application. Normally from
        the [DEFAULT] section of the Paste ini file.

    ``full_stack``
        Whether this application provides a full WSGI stack (by default,
        meaning it handles its own exceptions and errors). Disable
        full_stack when this application is "managed" by another WSGI
        middleware.

    ``static_files``
        Whether this application serves its own static files; disable
        when another web server is responsible for serving them.

    ``app_conf``
        The application's local configuration. Normally specified in
        the [app:<name>] section of the Paste ini file (where <name>
        defaults to main).

    """
    # Configure the Pylons environment
    load_environment(global_conf, app_conf)

    # The Pylons WSGI app
    app = PylonsApp()

    # Undefer POST parsing by undoing DeferPOSTParsing middleware work.
    app = kwmo.lib.middlewares.UnDeferPOSTParsing(app)

    # KWMO session middleware
    app = KwmoSessionMiddleware(app, config)

    # KWMO middleware - wrap kwmo application.
    app = kwmo.lib.middlewares.KWMOMiddleware(app)

    # Routing/Session/Cache Middleware
    app = RoutesMiddleware(app, config['routes.map'])

    app = CacheMiddleware(app, config)

    # Catch KJsonifyException exceptions and send a json exception in the body.
    app = kwmo.lib.middlewares.JSONErrorMiddleware(app)
        
    # Production setup.
    app = ErrorHandler(app, global_conf, **config['pylons.errorware'])
    
    # Handle special status codes.
    app = StatusCodeRedirect(app, [400, 401, 403, 404, 500])

    # Establish the Registry for this application
    app = RegistryManager(app)

    if asbool(static_files):
        # Serve static files
        static_app = StaticURLParser(config['pylons.paths']['static_files'])
        app = Cascade([static_app, app])

    app = kwmo.lib.middlewares.ContentLengthMiddleware(app)

    # Defer POST parsing by hiding the real input body file object and providing an empty StringIO() instead.
    app = kwmo.lib.middlewares.DeferPOSTParsing(app)

    # Change the url scheme when needed.
    if config.has_key('url_scheme'):
        app = kwmo.lib.middlewares.UrlSchemeMiddleware(app, config['url_scheme'])

    return app
