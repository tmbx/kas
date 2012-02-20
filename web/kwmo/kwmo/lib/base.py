"""The base Controller API

Provides the BaseController class for subclassing.
"""
from pylons.controllers import WSGIController
from pylons.templating import render_mako as render
from pylons import config, session, request, response, url, tmpl_context as c
import kwmo.model as model
import kwmo.model.kcd as kcd_model
from kwmo.model import Workspace
from kwmo.model.kcd import KcdKwsList, KcdUser, Invitation
from pylons.controllers.util import abort, redirect_to
from kwmo.lib.config import detect_cached_config_change, get_cached_master_config
from kwmo.lib.kwmolib import *
from kwmo.lib.perms import KWMOPermissions
from kanp import *
from kwmo.lib.strings import message_codes_map
import logging, time
from kwmo.lib.uimessage import ui_info, ui_warn, ui_error, ui_flash_info, ui_flash_warn, ui_flash_error

log = logging.getLogger(__name__)

PERM_CHECK_WS = (1<<0)
PERM_CHECK_USER = (1<<1)

def init_session(ws, reinit=False):
    from kanp import KANP_MINOR

    if reinit:
        log.debug("Re-initializing session.")

        # Re-init session.
        session.clear()

        # Re-init KWMO permissions object.
        c.perms = KWMOPermissions()

    else:
        log.debug("Initializing session.")

    # Set session default variables.
    session['version'] = 1
    session['workspace_id'] = ws.id
    if ws.public: session['mode'] = MODE_PUBWS
    else: session['mode'] = MODE_WS
    if ws.compat_v2: session['kanp_minor'] = 2
    else: session['kanp_minor'] = KANP_MINOR
    session['user'] = None
    session['secure'] = bool(c.workspace.secured)
    session['last_perm_check_id'] = ws.last_perm_check_id
    session['last_evt_ws_id'] = ws.evt_ws_id
    session['last_evt_user_id'] = ws.evt_user_id
    session['initialized'] = True
    session['perms'] = c.perms.to_dict()
    session.save()

class BaseController(WSGIController):

    requires_auth = []

    # Map global message variable names to callables.
    glob_msg_var_name_to_callable_map = { 'info_code' : ui_info,  'warning_code' : ui_warn, 'error_code' : ui_error }

    def __before__(self, action, controller, workspace_id = None, email_id = None):
        
        log.debug("Request to %s.%s, workspace_id=%s, email_id=%s, session_id=%s." % \
            ( controller, action, str(workspace_id), str(email_id), str(session.id) ) )

        # Detect changes in configuration.
        def config_has_changed():
            model.new_engine()
            kcd_model.new_engine()
        detect_cached_config_change(config_has_changed, config['master_file_path'])

        # Get cached master configuration.
        c.mc = get_cached_master_config()

        # Initialize models in local thread.
        model.init_local()
        kcd_model.init_local()

        # Initialize context variables.
        c.perms = KWMOPermissions()
        c.logout_url = None
        c.glob_messages = []

        # Prevent page caching.
        response.headers['Cache-Control'] = 'no-cache, must-revalidate'
        response.headers['Max-Age'] = '0'
        response.headers['Expires'] = 'Sat, 26 Jul 1997 05:00:00 GMT'

        # Detect global message passed in session (flash).
        if 'uimessage' in session:
            c.glob_messages.append(session['uimessage'])
            del session['uimessage']
            session.save()

        # Detect global message passed in a GET variable.
        for var_name, callable in self.glob_msg_var_name_to_callable_map.items():
            code = request.GET.get(var_name, None)
            if code:
                callable(code=code)
                break

        if workspace_id:
            # Get workspace.
            ws = Workspace.get_by(id=workspace_id)
            if not ws:
                log.warn("Workspace %s does not exit." % ( workspace_id ) )
                abort(404) # Not reliable here!

            # Initialize some context variables.
            c.workspace = ws
            c.is_admin = False
            if 'admin' in session and session['admin'] == True: 
                c.is_admin = True
            if 'user_id' in session and session['user_id']:
                # User is logged.
                c.logout_url = url('teambox_logout', workspace_id=c.workspace.id)
                if c.is_admin:
                    # User is admin.
                    c.logout_url = url('teambox_admin_logout', workspace_id=c.workspace.id)

            if 'initialized' in session:
                # Session is initialized.

                if not 'version' in session:
                    # Update session.
                    log.debug("Updating session.")
                    from kwmo.lib.updates import update_session_v1
                    update_session_v1(c, session)

                    # Save session.
                    session.save()

                # Fill the permission object with the session permissions dictionary.
                c.perms.from_dict(session['perms'])
     
            else:
                # Initialize session.
                init_session(ws)

            # Detect some workspace property changes.
            self._check_workspace_prop(controller, action)

            # Detect perm_check.
            if ws.last_perm_check_id > session['last_perm_check_id']:
                session['last_perm_check_id'] = ws.last_perm_check_id
                session.save()
                self._check_perms()

            # Set welcome name to use in the header partial
            if (('user_id' in session) and ('user' in session) and session['user']):
                if c.is_admin:
                    c.welcome_name = 'Administrator'
                elif session['user']['admin_name']:
                    c.welcome_name = session['user']['admin_name']
                elif session['user']['real_name']:
                    c.welcome_name = session['user']['real_name']
                else:
                    c.welcome_name = session['user']['email']

            # Check session expiration if set.
            if 'expiration_time' in session and \
                    not (controller == 'admin_teambox' and action == 'login'):
                if time.time() > session['expiration_time']:
                    log.debug("Admin session expired.")
                    init_session(c.workspace, reinit=True)
                    redirect_to(url('message_show', workspace_id=c.workspace.id, warning_code='admin_sess_expired'))
                
            # Authenticate
            if action in self.requires_auth:
                if 'user_id' not in session:
                    redirect_to(url('invite_resend_show', workspace_id = workspace_id))

    # Detect some workspace property changes.
    def _check_workspace_prop(self, controller, action):

        if 'user_id' in session and session['user_id'] or c.workspace.public:

            # Detect if workspace is frozen.
            c.workspace_frozen = bool(c.workspace.frozen or c.workspace.deep_frozen)

            # Show actions list.
            show_actions = [('teambox', 'show'), ('admin_teambox', 'show'), ('skurl_teambox', 'show')]

            # Ajust permissions depending on weither workspace is frozen or not.
            if c.workspace_frozen:
                if not c.perms.hasRole('freeze'):
                    log.debug("_check_workspace_prop(): workspace is now frozen.")
                    c.perms.addRole('freeze')
                    session.save()
                    if (controller, action) not in show_actions:
                        # Only show change in non-show actions.
                        ui_warn(code='workspace_now_frozen', hide_after_ms=5000)
                if (controller, action) in show_actions:
                    # Always show frozen message in show actions.
                    ui_warn(code='workspace_is_frozen', hide_after_ms=5000)
        
            if not c.workspace_frozen and c.perms.hasRole('freeze'):
                log.debug("_check_workspace_prop(): workspace is no longer frozen.")
                c.perms.dropRole('freeze')
                session.save()
                if (controller, action) not in show_actions:
                     # Only show change in non-show actions.
                    ui_info(code='workspace_now_unfrozen', hide_after_ms=5000)

    # Check permissions.
    # Called at login and
    # when a perm_check is needed.
    def _check_perms(self, checks=PERM_CHECK_WS|PERM_CHECK_USER):
        log.debug("_check_perms() called.")

        if checks & PERM_CHECK_WS:
            # Get workspace flags from KCD.
            kcd_workspace = KcdKwsList.get_by(kws_id=c.workspace.id)
            kcd_ws_flags = kcd_workspace.flags

            # Detect if workspace is deleted.
            if kcd_ws_flags & KANP_KWS_FLAG_DELETE:
                log.debug("_check_perms(): workspace deleted... unlogging and redirecting.")
                init_session(c.workspace, reinit=True)
                redirect_to(url('message_show', workspace_id=c.workspace.id, warning_code='workspace_deleted'))

            # Detect if workspace was changed from non-secure to secure.
            if kcd_ws_flags & KANP_KWS_FLAG_SECURE and not session['secure']:
                log.debug("Reinitializing session because workspace changed from non-secure to secure.")
                if not c.workspace.public:
                    email_id = None
                    if 'email_id' in session and session['email_id']:
                        email_id = session['email_id']
                    if email_id:
                        init_session(c.workspace, reinit=True)
                        return redirect_to(url('invitation_url', workspace_id=c.workspace.id, email_id=email_id, warning_code='nstos'))

                init_session(c.workspace, reinit=True)
                return redirect_to(url('teambox', workspace_id=c.workspace.id))

        if checks & PERM_CHECK_USER:
            if 'user' in session and session['user']:
                # Get user flags from KCD.
                kcd_user = KcdUser.get_by(kws_id=c.workspace.id, user_id=session['user']['id'])
                kcd_user_flags = kcd_user.flags

                # Detect if user is out.
                user_locked = bool(kcd_user_flags & KANP_USER_FLAG_LOCK)
                user_banned = bool(kcd_user_flags & KANP_USER_FLAG_BAN)
                user_generic_out = False
                if session['mode'] == MODE_WS and not c.is_admin:
                    # User is logged as a regular user (workspace mode).
                    if not Invitation.get_by(email_id = session['email_id']):
                        # Invitation email no longer exist.
                        user_generic_out = True

                # Destroy session if needed.
                if user_locked or user_banned or user_generic_out:
                    init_session(c.workspace, reinit=True)

                # Set URL for redirecting.
                u = None
                if user_locked: 
                    u = url('message_show', workspace_id=c.workspace.id, warning_code='user_locked')
                    log.debug("_check_perms(): user locked... redirecting to '%s'." % ( str(u) ) )
                elif user_banned: 
                    u = url('message_show', workspace_id=c.workspace.id, warning_code='user_banned')
                    log.debug("_check_perms(): user banned... redirecting to '%s'." % ( str(u) ) )
                elif user_generic_out: 
                    u = url('message_show', workspace_id=c.workspace.id, warning_code='user_generic_out')
                    log.debug("_check_perms(): user out, we don't know why... redirecting to '%s'." % ( str(u) ) )
                if u: redirect_to(u)

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
            kcd_model.clean_local()

