"""The base Controller API

Provides the BaseController class for subclassing.
"""
from pylons.controllers import WSGIController
from pylons.controllers.util import redirect_to
from pylons.templating import render_mako as render
from pylons import config, session as web_session, tmpl_context as c
from routes import url_for
import kascfg.model as model
import kascfg.model.kcd as kcd_model
import kascfg.model.freemium as freemium_model
from kascfg.lib.config import detect_cached_config_change, get_cached_master_config
from kascfg.lib.services import K2Services
from kascfg.lib.uimessage import ui_info, ui_warn, ui_error, ui_flash_info, ui_flash_warn, ui_flash_error
from sqlalchemy.exceptions import OperationalError
import logging

log = logging.getLogger(__name__)

class BaseController(WSGIController):

    def __before__(self, action, controller, *args, **kwargs):
        log.debug("Connection at %s.%s." % ( controller, action ) )

        # Global messages
        c.glob_messages = []

        # Detect global message passed in session (flash).
        if 'uimessage' in web_session:
            c.glob_messages.append(web_session['uimessage'])
            del web_session['uimessage']
            web_session.save()

        # Detect configuration changes.
        def config_has_changed():
            #model.new_engine() # not needed
            #model.init_local() # not needed
            kcd_model.new_engine()
            kcd_model.init_local()
            freemium_model.new_engine()
            freemium_model.init_local()
        detect_cached_config_change(config_has_changed)

        # Get cached master configuration.
        c.mc = get_cached_master_config()

        # Initialize models in local thread.
        #model.init_local() # not needed
        kcd_model.init_local()
        freemium_model.init_local()

        require_auth = None
        try:
            # Get the require_auth attribute.
            require_auth = getattr(self, 'require_auth')
        except AttributeError:
            # Class has no require_auth parameter.
            pass

        if require_auth and action in require_auth:
            # Object has a require_auth parameter.
            if not web_session.has_key('logged') or web_session['logged'] != True:
                # User is not logged.
                log.info("User not logged... redirecting to the login page.")
                return redirect_to(url_for('login'))

        c.logged = False
        if web_session.has_key('logged') and web_session['logged']:
            # Show logout button.
            c.logged = True

        # Get services objects.
        c.services = K2Services()

        # Load services status.
        for name, service in c.services.items():
            service.update_from_conf(c.mc)


    def __call__(self, environ, start_response):
        """Invoke the Controller"""
        # WSGIController.__call__ dispatches to the Controller method
        # the request is routed to. This routing information is
        # available in environ['pylons.routes_dict']

        # Insert any code to be run per request here.

        try:
            return WSGIController.__call__(self, environ, start_response)
        finally:
            #model.clean_local() # not needed
            kcd_model.clean_local()
            freemium_model.clean_local()

