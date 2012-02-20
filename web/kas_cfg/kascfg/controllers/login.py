import logging

from pylons import request, response, session as web_session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to
from routes import url_for
from kascfg.lib.base import BaseController, render, ui_error
from kascfg.lib.config import load_master_config
from cfgcommon import cfg_common, GT
import kascfg.lib.strings_kascfg as strings_kascfg

# from teambox-console-setup
#import kplatsetup

# Setup cfg_common.
cfg_common.strings[None] = strings_kascfg.strings
cfg_common.request = request

log = logging.getLogger(__name__)

class LoginController(BaseController):

    # [Re-]initialize session.
    def _init_session(self):
        web_session['logged'] = False
        web_session.save()

    # Login.
    def _login(self):
        web_session['logged'] = True
        web_session.save()

    # Handle login.
    def login(self):
        # The user is already logged in; redirect to the status page.
        if web_session.has_key('logged') and web_session['logged']:
            return redirect_to(url_for('status'))

        # Initialize web session.
        self._init_session()

        # Load master config.
        master_config = load_master_config()
       
        # FIXME: isinstance(...) could be removed if property has a null=False parameter... check that with Laurent.
        pwd = None
        if isinstance(master_config.admin_pwd, basestring) and len(master_config.admin_pwd) > 0:
            # Admin password is set.

            # Get the provided password, if any.
            pwd = request.POST.get('cfg_password', None)

            if pwd:
                if pwd == master_config.admin_pwd:
                    # User has provided the right password.
                    self._login()

                    # Redirect to the status page.
                    return redirect_to(url_for('status'))

                else:
                    # Show a bad password message.
                    ui_error(message=GT("login.bad_password"))

        else:
            # Admin password is not set yet; show a warning message.
            ui_error(message=GT("locals.admin_password_not_set"))

        # Push variables to template.
        c.pwd = pwd 
        c.GT = GT

        return render('/login/login.mako')

    # Handle logout.
    def logout(self):
        # Reinitialize session.
        self._init_session()

        return redirect_to(url_for('login'))

