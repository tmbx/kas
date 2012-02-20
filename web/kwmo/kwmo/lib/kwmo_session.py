# from system
import logging
from beaker.session import Session, SessionObject
from beaker.middleware import SessionMiddleware
from pylons import tmpl_context as c

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

class KwmoSession(Session):
    def __init__(self, request, environ=None, id=None, invalidate_corrupt=False,
             use_cookies=True, type=None, data_dir=None,
             key='beaker.session.id', timeout=None, cookie_expires=True,
             cookie_domain=None, secret=None, secure=False,
             namespace_class=None, **namespace_args):
        
        self.environ = environ

        # Call parent constructor.
        Session.__init__(self, request, id, invalidate_corrupt,
             use_cookies, type, data_dir,
             key, timeout, cookie_expires,
             cookie_domain, secret, secure,
             namespace_class, **namespace_args)

    def _create_id(self):
        # Call parent _create_id method.
        Session._create_id(self)
        if self.environ and self.environ.has_key('wsgiorg.routing_args') and (len(self.environ['wsgiorg.routing_args']) > 1):
            if 'workspace_id' in self.environ['wsgiorg.routing_args'][1]:
                if self.environ['wsgiorg.routing_args'][1]['controller'] in ['admin_teambox', 'admin_file_download']:
                    self.id = 'wsadminid' + self.environ['wsgiorg.routing_args'][1]['workspace_id'] + '__' + self.id
                    log.debug('kwmo admin session id created ' + self.id)
                else:
                    self.id = 'wsid' + self.environ['wsgiorg.routing_args'][1]['workspace_id'] + '__' + self.id
                    log.debug('kwmo session id created ' + self.id)
            else:
                log.info('_failed to create kwmo session id, creating session with cookie id ' + self.id)
        else:
            log.warn('__failed to create kwmo session id, creating session with cookie id ' + self.id)
    
    def load(self):
        if not self.is_new:            
            if self.environ and self.environ.has_key('wsgiorg.routing_args') and (len(self.environ['wsgiorg.routing_args']) > 1):
                if 'workspace_id' in self.environ['wsgiorg.routing_args'][1]:
                    if self.environ['wsgiorg.routing_args'][1]['controller'] in ['admin_teambox', 'admin_file_download']:
                        self.id = 'wsadminid' + self.environ['wsgiorg.routing_args'][1]['workspace_id'] + '__' + self.id
                        log.debug('kwmo admin session loaded ' + self.id)
                    else:
                        self.id = 'wsid' + self.environ['wsgiorg.routing_args'][1]['workspace_id'] + '__' + self.id
                        #log.debug('kwmo session loaded ' + self.id)

                else:
                    log.info('_failed to load kwmo session, loading session with cookie id ' + self.id)
            else:
                log.warn('__failed to load kwmo session, loading session with cookie id ' + self.id)
                
        #call parent load        
        Session.load(self)

    def save(self):
        # Save permissions in session if needed.
        if c.perms.dirty:
            # Save permissions.
            log.debug("Permissions have changed: saving them into session.")
            self['perms'] = c.perms.to_dict()

        # Call parent save method.
        log.debug("Saving session.")
        Session.save(self)

class KwmoSessionObject(SessionObject):
    def _session(self):
        """Lazy initial creation of session object"""
        if self.__dict__['_sess'] is None:
            params = self.__dict__['_params']
            environ = self.__dict__['_environ']
            self.__dict__['_headers'] = req = {'cookie_out':None}
            req['cookie'] = environ.get('HTTP_COOKIE')
            if params.get('type') == 'cookie':
                self.__dict__['_sess'] = CookieSession(req, **params)
            else:
                self.__dict__['_sess'] = KwmoSession(req, environ, use_cookies=True,
                                                 **params)
        return self.__dict__['_sess']

class KwmoSessionMiddleware(SessionMiddleware):
    def __call__(self, environ, start_response):
        session = KwmoSessionObject(environ, **self.options)
        if environ.get('paste.registry'):
            if environ['paste.registry'].reglist:
                environ['paste.registry'].register(self.session, session)
        environ[self.environ_key] = session
        environ['beaker.get_session'] = self._get_session
        
        def session_start_response(status, headers, exc_info = None):
            if session.dirty() or (session.accessed() and self.options.get('auto')):
                session.persist()
                if session.__dict__['_headers']['set_cookie']:
                    cookie = session.__dict__['_headers']['cookie_out']
                    if cookie:
                        headers.append(('Set-cookie', cookie))
            return start_response(status, headers, exc_info)
        return self.wrap_app(environ, session_start_response)

