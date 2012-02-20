import logging

from pylons import request, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kwmo.lib.jsonify import kjsonify # Custom version

log = logging.getLogger(__name__)

class ClientLoggerController(BaseController):

    requires_auth = ['serverlog']
    
    # Log things to server for helping debugging vnc (could be used for other things too).
    @kjsonify
    def serverlog(self, workspace_id):
        message = request.POST['message']
        log.info("Client log: '%s'.", message)
