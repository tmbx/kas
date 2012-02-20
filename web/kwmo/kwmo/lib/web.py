# This is code inspired by the pythonweb module, which is not used because of it's beta state.

import urllib

# Return the server url scheme.
def current_url_scheme(environ):
    return environ['wsgi.url_scheme']

# Return the server host.
def current_host(environ):
    if environ.get('HTTP_HOST'): 
        return environ['HTTP_HOST']
    return environ['SERVER_NAME']

# Return None, or server port string if it does not match the default port for that scheme.
def current_port_string(environ):
    s = None
    if (environ['wsgi.url_scheme'] == 'https' and environ['SERVER_PORT'] != '443') \
             or (environ['wsgi.url_scheme'] == 'http' and environ['SERVER_PORT'] != '80'):
        s = environ['SERVER_PORT']
    return s    

# Return current URL.
def current_url(environ):
    url_scheme = current_url_scheme(environ)
    host = current_host(environ)
    port = current_port_string(environ)
    path = urllib.quote(environ.get('SCRIPT_NAME', '')) + urllib.quote(environ.get('PATH_INFO', '')) # FIXME
    url = url_scheme + '://' + host
    if port: url += ':' + port
    url += path
    if environ.get('QUERY_STRING'):
        url += '?' + environ['QUERY_STRING']
    return url

