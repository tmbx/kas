import logging
import sys
import warnings
import traceback
import simplejson
from decorator import decorator
from pylons import config, tmpl_context as c

from pylons.decorators.util import get_pylons

__all__ = ['KJsonifyException', 'kjsonify']

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

# Jsonify exception
class KJsonifyException(Exception):
    def __init__(self, type, exception, trace=[]):
        self.type = type
        self.exception = exception
        self.trace = trace

    def __repr__(self):
        return "<KJsonifyException exception='%s'>" % ( self.exception )

# Jsonify reload exception
class KJsonifyReload(Exception):
    def __init__(self, message=None):
        self.message = message

    def __repr__(self):
        return "<KJsonifyReload message='%s'>" % ( str(self.message) )

# Jsonify redirect exception
class KJsonifyRedirect(Exception):
    def __init__(self, url, message=None):
        self.url = url
        self.message = message

    def __repr__(self):
        return "<KJsonifyRedirect url='%s' message='%s'>" % ( self.url, str(self.message) )

# New version that returns an http error code on exception.
def kjsonify(func, *args, **kwargs):
    """Action decorator that formats output for JSON

    Given a function that will return content, this decorator will turn
    the result into JSON, with a content-type of 'application/json' and
    output it.

    CUSTOM: On exception, raise a KJsonifyException with keys:
        - in debug mode: type, exception, trace
        - in production: exception
    """
    pylons = get_pylons(args)
    pylons.response.headers['Content-Type'] = 'application/json'
    try:
        data = func(*args, **kwargs)

        # Accept only dict objects for returned data.
        if not isinstance(data, (dict)):
            raise Exception("kjsonify: bad data: only dictionaries are accepted.")
            warnings.warn(msg, Warning, 2)
            log.warning(msg)

        # Append global messages if any.
        if len(c.glob_messages):
            data['glob_msgs'] = map(lambda x: x.to_dict(), c.glob_messages)

        return simplejson.dumps(data)

    except Exception, e:
        exceptionType, exceptionValue, exceptionTraceback = sys.exc_info()
        if config['debug']:

            # Log exception and debugging informations.
            log.debug("JSON exception:")
            for t in traceback.format_tb(exceptionTraceback):
                log.debug("%-9s: '%s'" % ( "trace", str(t) ) )
            log.debug("%-9s: '%s'" % ( "exception", str(e) ) )

            # Return ajax dictionary with exception details.
            raise KJsonifyException(str(exceptionType), str(exceptionValue), traceback.format_tb(exceptionTraceback))

        else:
            # Log exception.
            log.debug("JSON exception: '%s'" % ( str(e) ) )

            # Return generic error.
            raise KJsonifyException(str(exceptionType), 'Internal error')
kjsonify = decorator(kjsonify)

