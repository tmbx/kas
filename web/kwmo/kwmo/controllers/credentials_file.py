import logging

from pylons import config
from pylons import request, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kwmo.model.kcd.invitation import Invitation
from kwmo.model.workspace import Workspace
from kwmo.model.user import User
from kwmo.lib.config import get_cached_kcd_external_conf_object


# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

class CredentialsFileController(BaseController):

    def get(self, workspace_id, email_id):
        # Return a rendered template
        #return render('/credentials_file.mako')
        # or, return a response
        response.content_type = "application/wsl"

        filename = "kwmo_creds.wsl"
        # Temporarily disabled - need to be debugged.
        if request.params.has_key('interactive') and str(request.params.get('interactive')) == "1":
            filename = c.workspace.name + ".wsl"

        response.headers['Content-disposition'] = 'attachment; filename="%s"' % ( filename.encode('latin1') )
        
        #fix for IE
        response.headers['Pragma'] = 'public'
        response.headers['Cache-Control'] = 'max-age-0'

        errorStr = None
        #ws = Workspace.get_by(id= workspace_id)
        ws = c.workspace
        invitation = Invitation.get_by(email_id = email_id)
        
        if (ws and invitation):
            user = User.get_by(id = invitation.user_id, workspace=ws)
            if(user and ws.id==invitation.kws_id):
                c.invitation = invitation
                c.user = user
                c.workspace = ws
                conf = get_cached_kcd_external_conf_object()
                c.kcd_host = conf.kcd_host
                c.kcd_port = conf.kcd_port
                if ('password' in request.params):
                    c.password = request.params['password']
                    
                if ('user_id' in session):
                    c.password = session['password']
            else:
                errorStr = "User was not invited to this workspace"

        else:
            errorStr = "Invalid email_id or workspace_id"
        
        if errorStr:
            c.errorStr = errorStr
            return render('credentials_file/error.mako')
        else:
            return render('credentials_file/get.mako')
