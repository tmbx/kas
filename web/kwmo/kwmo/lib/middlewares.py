# from system
import logging

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

from webob import Request
from StringIO import StringIO
import gp.fileupload.upload
from gp.fileupload.config import TEMP_DIR
from kwmo.lib.jsonify import KJsonifyException, KJsonifyRedirect, KJsonifyReload
import simplejson
from pylons import request, response
from pylons.controllers.util import redirect_to
from pylons.templating import render_mako
from traceback import format_exc
from pylons.util import call_wsgi_application

# FIXME TODO: This class was taken from 
#             http://www.pythonweb.org/projects/webmodules/release/0.5.3/PythonWeb.org-0.5.3-src.tar.bz2
#             without checking the license... 
#             This package is not installable using easy_install.. we might include the whole package later 
#             or we might just re-implement classes that inherit from it.
class BaseMiddleware:
    "Easy middleware building"

    def __init__(self, application):
        self.application = application

    def __call__(self, environ, start_response):
        self.setup()
        def app(environ, start_response):
            environ = self.environ(environ)
            def new_response(status, headers, exc_info=None):
                return start_response(*self.response(status, headers, exc_info))
            return self.application(environ, new_response)
        return self.result(app(environ, start_response))

    def setup(self):
        "Re-initialise variables to be reset at each execution"
        pass

    def environ(self, environ):
        "This method allows you to make changes to environ. You should return the environ dictionary"
        return environ

    def response(self, status, headers, exc_info):
        "This method defines the order in which the response parameters are returned"
        status = self.status(status)
        headers = self.headers(headers)
        exc_info = self.exc_info(exc_info)
        return status, headers, exc_info

    def status(self, status):
        "This method allows you to make changes to status. You should return status"
        return status
        
    def headers(self, headers):
        "This method allows you to make changes to headers. You should return headers"
        return headers
        
    def exc_info(self, exc_info):
        "This method allows you to make changes to exc_info. You should return exc_info"
        return exc_info
        
    def result(self, result):
        return result

# Replace X-Content-Length header with a Content-Length header (this is a workaround
# for Content-Length header being dropped somewhere by pylons).
BROWSER_MAX_FILE_SIZE = 2* 1024 * 1024 * 1024

class ContentLengthMiddleware(BaseMiddleware):
    def headers(self, headers):
        for header in headers:
            if header[0] ==  'X-Content-Length':
                #DO NOT SET CONTENT LENGTH IF FILE SIZE IS MORE THAN 2GB CAUSE MOD_WSGI COMPLAINS
                if int(header[1]) < BROWSER_MAX_FILE_SIZE:
                    log.debug('Replacing header X-Content-Length for Content-Length')
                    headers.append(('Content-Length', header[1]))
 
        return headers

WSGI_INPUT = 'wsgi.input'
CONTENT_LENGTH = 'CONTENT_LENGTH'
DEFERED_WSGI_INPUT = 'defered.' + WSGI_INPUT
DEFERED_CONTENT_LENGTH = 'defered.' + CONTENT_LENGTH

# Defer POST Parsing middleware.
# Defer post parsing to later.
# NOTE: It will "fail" if some middleware parses input before this middleware is called.
# Use environ[DEFERED_WSGI_INPUT] and environ[DEFERED_CONTENT_LENGTH] for later parsing.
# --UPDATE: It's NOT possible to access environ['beaker.session'] now, as KwmoSessionMiddleware exists only after RoutesMiddleware.
class DeferPOSTParsing(BaseMiddleware):
    def environ(self, environ):
        if environ.has_key(CONTENT_LENGTH):
            environ[DEFERED_WSGI_INPUT] = environ[WSGI_INPUT]
            environ[DEFERED_CONTENT_LENGTH] =  environ[CONTENT_LENGTH]
            environ[WSGI_INPUT] = StringIO()
            environ[CONTENT_LENGTH] = 0

        return environ

# Undefer POST Parsing middleware.
# Undo work done by DeferPOSTParsing middleware later in the middlewares chain.
class UnDeferPOSTParsing(BaseMiddleware):
    def environ(self, environ):
        if environ.has_key(DEFERED_CONTENT_LENGTH):
            environ[WSGI_INPUT] = environ[DEFERED_WSGI_INPUT]
            environ[CONTENT_LENGTH] =  environ[DEFERED_CONTENT_LENGTH]
            del environ[DEFERED_WSGI_INPUT]
            del environ[DEFERED_CONTENT_LENGTH]

        return environ

# KWMO wrapper middleware.
class KWMOMiddleware:

    def __init__(self, application):
        self.application = application

    def __call__(self, environ, start_response):
        # Run application.
        status, headers, app_iter = call_wsgi_application(self.application, environ)

        # Get location header.
        location = None
        for (name, value) in headers:
            if name == "location":
                location = value
                break
        
        if location:
            # Redirection.

            redirect_exceptions = [('file_upload', 'upload')]
            controller = environ['wsgiorg.routing_args'][1]['controller']
            action = environ['wsgiorg.routing_args'][1]['action']

            if request.headers.get('X-KAjax', None):
                log.debug("KWMOMiddleware: redirecting through a kajax request to '%s'." % ( location ) )
                raise KJsonifyRedirect(location)

            elif (controller, action) in redirect_exceptions:                              
                log.debug("KWMOMiddleware: ignoring redirect to '%s'." % ( location ) )
                status = '200 OK'
                response_headers = [('Content-type','text/plain')]
                start_response(status, response_headers)
                return 'redirection:%s' % ( location )

            else:
                log.debug("KWMOMiddleware: redirecting to '%s'." % ( location ) )
 
        start_response(status, headers)
        return app_iter

# JSON error middleware.
class JSONErrorMiddleware:

    def __init__(self, application):
        self.application = application

    def __call__(self, environ, start_response):

        try:
            return self.application(environ, start_response)

        except KJsonifyReload, e:
            status = '200 OK'
            response_headers = [('Content-type','text/plain')]
            start_response(status, response_headers)
            return simplejson.dumps({'jsonify_action' : 'reload', 'jsonify_message' : e.message})

        except KJsonifyRedirect, e:
            status = '200 OK'
            response_headers = [('Content-type','text/plain')]
            start_response(status, response_headers)
            return simplejson.dumps({'jsonify_action' : 'redirect', 'jsonify_url' : e.url,
                'jsonify_message' : e.message})

        except KJsonifyException, e:
            status = '405 OK'
            response_headers = [('Content-type','text/plain'),('X-JSON-Exception','1')]
            start_response(status, response_headers)
            return simplejson.dumps({'type' : e.type, 'exception' : e.exception, 'trace' : e.trace})


# Middleware used to set the url scheme.
class UrlSchemeMiddleware:
    def __init__(self, application, url_scheme):
        self.application = application
        self.url_scheme = url_scheme

    def __call__(self, environ, start_response):
        environ['wsgi.url_scheme'] = self.url_scheme
        return self.application(environ, start_response)

