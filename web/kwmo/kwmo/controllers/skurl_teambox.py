from kwmo.controllers.abstract_teambox import *

import time
from kwmo.lib.kwmo_kcd_client import KcdClient
from kwmo.lib.config import get_cached_kcd_external_conf_object
from kfs_lib import *

from kwmo.lib.base import init_session
from kwmo.lib.kwmolib import *

from kwmo.model.user import User
from kwmo.model.kfs_node import KfsNode
from kwmo.model.chat_request import ChatRequest
from kwmo.model.ws_request import WSRequest
import kbase
import simplejson

log = logging.getLogger(__name__)

class SkurlTeamboxController(AbstractTeamboxController):

    # Internal: check if workspace is public.
    def _check_public(self, workspace_id):
        if not c.workspace.public:
            log.warning("_check_public(): workspace %i is not public." % ( workspace_id ) )
            abort(404)

    # Internal: login as a skurl user.
    def _login(self, user):
        session['user'] = user.to_dict()
        session['user_id'] = session['user']['id']
        c.perms.allow('kfs.download.share.0')
        c.perms.allow('kfs.upload.share.0')
        session.save()

        # Last minute permissions check.
        self._check_perms()

    # Internal: set chat request permissions.
    def _set_chat_requests_perms(self, flag):
        if flag:
            # Allow chat requests.
            c.perms.allow('pubws.req.chat')

        else:
            # Deny furthur chat requests.
            c.perms.deny('pubws.req.chat')

    # Internal: set chat permissions.
    def _set_chat_perms(self, flag):
        if flag:
            # Allow chat.
            c.perms.allow('chat.list.channel.' + str(session['user_id']))
            c.perms.allow('chat.post.channel.' + str(session['user_id']))

        else:
            # Deny chat.
            c.perms.deny('chat.list.channel.' + str(session['user_id']))
            c.perms.deny('chat.post.channel.' + str(session['user_id']))

    # Internal: set workspace creation requests permissions.
    def _set_ws_creation_requests_perms(self, flag):
        if flag:
            # Deny furthur workspace creation requests.
            c.perms.allow('pubws.req.wscreate')

        else:
            # Allow workspace requests.
            c.perms.deny('pubws.req.wscreate')

    # Log user out.
    def logout(self, workspace_id, email_id):
        log.debug("Skurl logout.")
        init_session(c.workspace, reinit=True)
        ui_flash_info(code='logout', hide_after_ms=5000)
        redirect_to(url('teambox_pubws_show', workspace_id=workspace_id, email_id=email_id))

    # Show public workspace main page.
    def show(self, workspace_id, email_id):
        workspace_id = int(workspace_id)
        email_id = int(email_id)

        # Set logout url.
        c.logout_url = url('teambox_pubws_logout', workspace_id=workspace_id, email_id=email_id)

        # Check if the workspace is public.
        self._check_public(workspace_id)

        if 'email_id' in session and session['email_id'] != email_id:
            # User is logged but wants to access a different email. Reinit session.
            log.debug("Reinitializing session because user is using another email id: previous='%s', new='%s'." \
                % ( str(session['email_id']), str(email_id) ) )
            init_session(c.workspace, reinit=True)

        notif = request.GET.get('notif', 0)
        if notif:
            # This is the sender (user 1)... [re-]login automatically.
            log.debug("User is accessing a public workspace using a notification link... automatically log user in.")
            user = User.get_by(workspace_id=workspace_id, id=1)
            log.debug("Reinitializing session because user is logging as user 1 (notif management).")
            init_session(c.workspace, reinit=True)
            self._login(user)
            c.notif_flag = True
        else:
            if 'user' in session and session['user'] and session['user']['id'] == 1:
                # Sender is logged (as a sender) but he's using a regular skurl link: logout.
                log.debug("Reinitializing session because user was logged as user 1 but is using a regular skurl link.")
                init_session(c.workspace, reinit=True)

        if not c.perms.hasRole('skurl'):
            # Give skurl role, if not already done.
            c.perms.addRole('skurl')

            # Save session.
            session.save()

        if not 'email_id' in session:
            # Set email information in session.

            # Instantiate a Kcd client.
            kc = KcdClient(get_cached_kcd_external_conf_object())

            # Check that email ID is valid.
            email_info = kc.pubws_get_email_info(workspace_id, email_id)
            if not email_info:
                log.debug("PubWS: invalild email ID: %i" % ( email_id ) )
                abort(404)

            # Get the email sender.
            sender_user = User.get_by(workspace_id=workspace_id, id=1)
            sender = kbase.PropStore()
            sender.name = sender_user.real_name
            sender.email = sender_user.email

            # Get the email recipients (list of PropStores, having name and email keys).
            raw_recipients = kc.pubws_get_eid_recipient_identities(workspace_id, email_id)

            # Strip sender email from recipients, if needed.
            recipients = []
            for recipient in raw_recipients:
                if recipient.email != sender.email:
                    recipients.append(recipient)

            # Merge sender and recipients.
            identities = [sender] + recipients

            # Set needed informations in session.
            session['email_id'] = email_id
            session['email_info'] = email_info.to_dict()
            session['identities'] = map(lambda x: x.to_dict(), identities)
            session.save()

        # Get informations that will be published in the template. 
        c.dyn_version = 15
        c.email_info = session['email_info']
        c.json_email_info_str = simplejson.dumps(c.email_info)
        c.identities = session['identities']
        c.json_identities_str = simplejson.dumps(c.identities)

        # Check if a chat request was accepted lately (delay is hardcoded in accepted_lately()). 
        c.user_id = None
        if 'user_id' in session and session['user_id']:
            c.user_id = session['user_id']
            if ChatRequest.accepted_lately(workspace_id, session['user_id']):
                # Deny chat requests and allow chat since a request was accepted lately.
                self._set_chat_requests_perms(False)
                self._set_chat_perms(True)
            else:
                # Allow chat requests and deny chat since no request was accepted lately.
                self._set_chat_requests_perms(True)
                self._set_chat_perms(False)

            # Allow workspace creation request.
            self._set_ws_creation_requests_perms(True)

            # Save session.
            session.save()

        c.base_url_paths = kurl.get_base_url_paths(
            'teambox_updater',
            'teambox_post_chat',
            'teambox_download',
            'teambox_upload',
            'teambox_pubws_set_identity',
            'teambox_pubws_chat_request',
            'teambox_pubws_chat_request_result',
            'teambox_pubws_kfsup_request',
            'teambox_pubws_kfsdown_request',
            'teambox_pubws_create_request')

        # Get first update directly.
        flags = ( StateRequest.STATE_FORCE_SYNC 
                            | StateRequest.STATE_WANT_PERMS 
                            | StateRequest.STATE_WANT_MEMBERS
                            | StateRequest.STATE_WANT_KFS 
                            | StateRequest.STATE_WANT_PUBWS_INFO )
        params = { }
        if 'user_id' in session and session['user_id']:
            flags |= StateRequest.STATE_WANT_CHAT
            params['chat_channel_id'] = session['user_id']
        updater_state_dict = state_request_get(c, session, flags, params)
        c.updater_state_json = simplejson.dumps(updater_state_dict)

        return render('/teambox/pubwsshow.mako')
        
    # Get a user ID matching the identity ID selected by the user.
    # If user is not invited, he is invited first.
    @kjsonify
    def pb_set_identity(self, workspace_id):
        import select
        from kcd_lib import WorkspaceInvitee

        workspace_id = int(workspace_id)

        # Get the workspace.
        if not c.workspace.public:
            log.warning("pb_set_identity: Workspace %i is not public." % ( workspace_id ) )
            abort(404)

        # Get POST parameters.
        identity_id = request.params['identity_id']
        identity_id = int(identity_id)

        # Shortcuts
        identity = session['identities'][identity_id]
        log.debug("Recipient: %s" % ( str(identity) ) )

        if identity_id == 0:
            # This is the sender (user 1).
            user = User.get_by(workspace_id=workspace_id, id=1)
            self._login(user)
            log.debug("Found matching user(0): '%s'." % ( str(user) ) )
            return { 'result' : 'ok', 'user' : session['user'] }

        # This is a real recipient... try to get the user.
        user = User.get_by(workspace_id=workspace_id, email=identity['email'])
        if user:
            self._login(user)
            log.debug("Found matching user(1): '%s'." % ( str(user) ) )
            return { 'result' : 'ok', 'user' : session['user'] }

        # Instantiate a Kcd client.
        kc = KcdClient(get_cached_kcd_external_conf_object())

        # Invite user.
        invitee = WorkspaceInvitee(real_name=identity['name'], email_address=identity['email'])
        junk_url, invitees = kc.invite_users(workspace_id, "empty message", [invitee])
        if invitees[0].error:
            log.debug("User could not be invited: '%s'." % ( str(invitees[0].error) ) )
            raise Exception('Internal error.')

        # Get user. If not present, retry a few times, until new user is fetched by kwsfetcher or until timeout.
        
        wait_seconds = 0.5
        timeout_seconds = 8
        time_started = time.time()
        while 1:
            # Get user, if it exists (fetched by kwsfetcher).
            user = User.get_by(workspace_id=workspace_id, email=identity['email'])
            if user: 
                self._login(user)
                log.debug("Found matching user (2): '%s'." % ( str(user) ) )
                return { 'result' : 'ok', 'user' : session['user'] }
           
            # Check for timeout. 
            if time.time() > time_started + timeout_seconds: break

            # Wait 
            select.select([], [], [], wait_seconds)  

        # Reached timeout.
        log.error("Error: reached end of pb_set_identity(). KWSFetcher might be too loaded or down.");
        raise Exception('Temporary server error: please try again later.');


    # Internal: do stuff related to every pubws request.
    def _request_common(self, workspace_id):
        # Check that the user is logged.
        if not session['user']:
            log.error("_request_common(): user is not logged.")
            abort(404)

        # Instantiate a Kcd client in the context-global variable.
        c.pubws_kc = KcdClient(get_cached_kcd_external_conf_object())

    # PubWS chat request.
    @kjsonify
    def chat_request(self, workspace_id):
        workspace_id = int(workspace_id)

        # Do some checks and initialization.
        self._check_public(workspace_id)
        self._request_common(workspace_id)

        # Time to allow the workspace owner to respond.
        # Keep PubWSChat javascript object code in sync for the global chat 
        # request timeout (which must be a little longer than this one).
        req_timeout = 60

        # Shortcuts.
        user_id = session['user']['id']
        subject = session['email_info']['subject']

        # Post request.
        chat_req_id = c.pubws_kc.pubws_chat_request(workspace_id, user_id, c.workspace.compat_v2, subject, req_timeout)
        log.debug("Chat request: got chat_req_id '%i'." % ( chat_req_id ) )
        return { "chat_req_id" : chat_req_id }

    # PubWS chat request result request.
    @kjsonify
    def chat_request_result(self, workspace_id, req_id):
        workspace_id = int(workspace_id)
        req_id = int(req_id)
        req_start_time = request.params['req_start_time']

        # Do some checks and initialization.
        self._check_public(workspace_id)
        self._request_common(workspace_id)

        # Get the request.
        req = ChatRequest.get_by(workspace_id=workspace_id, request_id=req_id)

        if req:
            # Check request status.
            if req.accepted:
                # Modify permissions.
                self._set_chat_requests_perms(False)
                self._set_chat_perms(True)

                # Save session.
                session.save()

                log.debug("chat_request_result(): accepted.")
                return { "result" : "ok" }

            # Enable when debugging to enable automatic chat acceptation.
            if 0:
                from kanp import KANP_MINOR
                from pylons import config

                kc = KcdClient(get_cached_kcd_external_conf_object())
                # This function has to be rewritten.
                kc.pubws_chat_request_accept(workspace_id, user_id, KANP_MINOR, req_id)

        else:
            # Bad request ID or kwsfetcher has not yet fetched the request.
            pass

        log.debug("chat_request_result(): pending, chat_req_id='%s', req_start_time='%s'." \
            % ( str(req_id), str(req_start_time) ) )
        return { "result" : "pending", "chat_req_id" : req_id, 'req_start_time' : req_start_time }

    # PubWS KFS upload request.
    @kjsonify
    def kfs_upload_request(self, workspace_id):
        workspace_id = int(workspace_id)

        # Do some checks and initialization.
        self._check_public(workspace_id)
        self._request_common(workspace_id)

        # No-op
        return { "result" : "ok" }

    # PubWS KFS download request.
    @kjsonify
    def kfs_download_request(self, workspace_id):
        workspace_id = int(workspace_id)

        # Do some checks and initialization.
        self._check_public(workspace_id)
        self._request_common(workspace_id)

        # No-op
        return { "result" : "ok" }

    # PubWS workspace creation request.
    @kjsonify
    def ws_create_request(self, workspace_id):
        workspace_id = int(workspace_id)

        # Do some checks and initialization.
        self._check_public(workspace_id)
        self._request_common(workspace_id)

        # Shortcuts.
        user_id = session['user']['id']
        subject = session['email_info']['subject']

        # Post request.
        req_id = c.pubws_kc.pubws_workspace_creation_request(workspace_id, user_id, c.workspace.compat_v2, subject)

        # Modify permissions.
        self._set_ws_creation_requests_perms(False)

        # Save permissions.
        session.save()

        return { "result" : "ready" }

