import os, logging

import kbase

from pylons import request, response, session, config, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kwmo.lib.jsonify import kjsonify # Custom version
from kwmo.model.vnc_session import VncSession
from kwmo.lib.exceptions import KAjaxViewException

log = logging.getLogger(__name__)

class VncController(BaseController):

    requires_auth = ['start_vnc_session']

    @kjsonify
    def start_vnc_session(self, workspace_id, vnc_session_id):
        if not c.perms.hasPerm('vnc.connect'):
            log.error("VNC connection denied: user has not the right permissions.")
            raise KAjaxViewException("Connection denied.")

        vnc_session = VncSession.get_by(workspace_id = workspace_id, session_id = vnc_session_id)
        if not vnc_session:
            raise KAjaxViewException("Bad Shared Application ID.")

        file_name = kbase.gen_random(26) + "_" + str(vnc_session.port).zfill(5)
        tmp = open(os.path.join(c.mc.kcd_vnc_cred_path, file_name), "wb")
        tmp.close()
        return {'teambox_auth' : file_name}

