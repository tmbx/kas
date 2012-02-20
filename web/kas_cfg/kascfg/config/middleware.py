"""Pylons middleware initialization"""
from beaker.middleware import CacheMiddleware, SessionMiddleware
from paste.cascade import Cascade
from paste.registry import RegistryManager
from paste.urlparser import StaticURLParser
from paste.deploy.converters import asbool
from pylons import config
from pylons.middleware import ErrorHandler, StatusCodeRedirect
from pylons.wsgiapp import PylonsApp
from pylons.templating import render_mako
from routes.middleware import RoutesMiddleware
from traceback import format_exc

from kascfg.config.environment import load_environment

# Middleware used for debugging.
class TeamboxDebugMiddleware:
    def __init__(self, application):
        self.application = application

    def __call__(self, environ, start_response):
        try: return self.application(environ, start_response)
        except:
            start_response('200 OK', [('Content-Type','text/html')])
            return [str(render_mako("trace.mako", { "trace" : format_exc(50) }))]

# Middleware used to set the url scheme.
class UrlSchemeMiddleware:
    def __init__(self, application, url_scheme):
        self.application = application
        self.url_scheme = url_scheme

    def __call__(self, environ, start_response):
        environ['wsgi.url_scheme'] = self.url_scheme
        return self.application(environ, start_response)

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

    # Routing/Session/Cache Middleware
    app = RoutesMiddleware(app, config['routes.map'])
    app = SessionMiddleware(app, config)
    app = CacheMiddleware(app, config)

    # Teambox debugging.
    if config['debug']: app = TeamboxDebugMiddleware(app)
        
    # Production setup.
    else: app = ErrorHandler(app, global_conf, **config['pylons.errorware'])
    
    # Handle special status codes.
    app = StatusCodeRedirect(app, [400, 401, 403, 404, 500])

    # Establish the Registry for this application
    app = RegistryManager(app)

    if asbool(static_files):
        # Serve static files
        static_app = StaticURLParser(config['pylons.paths']['static_files'])
        app = Cascade([static_app, app])

    # Change the url scheme when needed.
    if config.has_key('url_scheme'):
        app = UrlSchemeMiddleware(app, config['url_scheme'])

    return app
