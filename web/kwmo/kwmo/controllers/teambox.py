from kwmo.controllers.abstract_teambox import *

# Local libs
from kwmo.lib.staterequest import StateRequest, state_request
from kwmo.model.kfs_node import get_root_dir
from routes import url_for
from pylons import response
import simplejson

# Local models

# From kpython
import kfile

import kbase

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

class TeamboxController(AbstractTeamboxController):
    
    requires_auth = ['show']

    url_paths = kurl.get_base_url_paths(
            'teambox_updater',
            'credentials_download',
            'teambox_post_chat',
            'teambox_download',
            'teambox_upload',
            'vnc_session_start',
            'teambox_serverlog')

    # Show workspace main page.
    def show(self, workspace_id):
        workspace_id = int(workspace_id)
        ws = c.workspace
        if ws.public and not c.is_admin:
            log.warning("Workspace %i is public." % ( workspace_id ) )
            abort(404)
       
        c.user_id = session['user_id']

        c.notif_flag = True
        
        #TODO check if admin is logged in instead
        if 'email_id' in session:
            c.email_id = session['email_id']
        
        # Make those informations available in template (mako) code.
        #c.dojo_base_url = 'http://127.0.0.1/dojo'
        c.dojo_base_url = '/javascripts/base/dojo'
        c.dyn_version = 15
        share_id = 0
        root_kfs_dir_dict = get_root_dir(workspace_id, share_id).to_dict()
        del root_kfs_dir_dict['file_hash'] # not json serializable
        c.kfs_dir = simplejson.dumps(root_kfs_dir_dict)
        c.vnc_meta_proxy_port = 443
        c.teambox_cert = kfile.read_file(c.mc.kcd_ssl_cert_path).replace("\n", "%")
        c.base_url_paths = self.url_paths

        # Get first update directly.
        flags = ( StateRequest.STATE_FORCE_SYNC 
                | StateRequest.STATE_WANT_PERMS
                | StateRequest.STATE_WANT_WS
                | StateRequest.STATE_WANT_MEMBERS
                | StateRequest.STATE_WANT_CHAT
                | StateRequest.STATE_WANT_VNC
                | StateRequest.STATE_WANT_KFS )
        params = { 'kfs_dir' : root_kfs_dir_dict, 'chat_channel_id' : 0 }
        updater_state_dict = state_request_get(c, session, flags, params)
        c.updater_state_json = simplejson.dumps(updater_state_dict)

        # Render template.
        return render("/teambox/show.mako")

