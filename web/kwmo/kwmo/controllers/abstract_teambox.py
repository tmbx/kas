import logging
import simplejson

from pylons import request, response, url, session, config, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kwmo.lib.jsonify import kjsonify # Custom version
from kwmo.lib.staterequest import *
import kwmo.lib.url as kurl #import get_base_url_paths
from kwmo.lib.base import init_session
from kwmo.lib.uimessage import ui_info, ui_warn, ui_error, ui_flash_info, ui_flash_warn, ui_flash_error

import kbase

log = logging.getLogger(__name__)

class AbstractTeamboxController(BaseController):

    # Log user out.
    def logout(self, workspace_id):
        log.debug("Logout.")
        init_session(c.workspace, reinit=True)
        redirect_to(url('invite_resend_show', workspace_id=workspace_id, info_code='logout'))

    # This method handles multiplexed ajax state requests.
    # It uses a custom @kjsonify decorator that catches exceptions and sends 
    # json dictionary with the exception instead of dumping an html trace.
    @kjsonify
    def updater(self, workspace_id):
        workspace_id = int(workspace_id)

        # Get request object.
        state_request_str = request.params.get('state_request')
        state_request_dict = simplejson.loads(state_request_str)

        # Support kwmo versions < 1.9, which did not have state version and request ID support.
        if not 'version' in state_request_dict: state_request_dict['version'] = '0'
        if not 'req_id' in state_request_dict: state_request_dict['req_id'] = '0'

        # Handle the request.
        result = { }
        if state_request_dict:
            version = int(state_request_dict['version'])
            req_id = int(state_request_dict['req_id'])
            flags = int(state_request_dict['req_flags'])
            params =  state_request_dict['req_params']
            result = state_request_get(c, session, flags, params, version, req_id)
        else:
            raise Exception("State request with no request data.")

        return result

