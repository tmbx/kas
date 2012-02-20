import logging

from pylons import url, request, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render

log = logging.getLogger(__name__)

class OldDispatcherController(BaseController):

    def dispatch_v1(self, kws_id):

        if 'eid' in request.params: #skurl
            redirect_to(url('teambox_pubws_show', workspace_id = kws_id, email_id = request.params['eid']))
            pass
        else:
            redirect_to(url('authorize_old_wleu', workspace_id = kws_id))
    
    def dispatch_v0(self):
        if (('eid' in request.params) and ('kws_id' in request.params)): #skurl
            redirect_to(url('teambox_pubws_show', workspace_id = request.params['kws_id'], email_id = request.params['eid']))
            pass
            
        elif ('kws_id' in request.params): #wleu v0
            redirect_to(url('authorize_old_wleu', workspace_id = request.params['kws_id']))

        else: #invalid old url format
            abort(404)
