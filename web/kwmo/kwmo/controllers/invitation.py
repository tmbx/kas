import logging

from pylons import config, url, request, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kwmo.lib.kwmolib import *
from kwmo.model.workspace import Workspace
from kwmo.model.user import User

from kwmo.lib.base import init_session
from kwmo.model.kcd.invitation import Invitation
from kwmo.model.kcd.kcd_user import KcdUser
from kcd_lib import WorkspaceInvitee
from kwmo.lib.config import get_cached_kcd_external_conf_object
from kwmo.lib.kwmo_kcd_client import KcdClient
from kwmo.lib.uimessage import ui_info, ui_error, ui_flash_error

# Put after imports so log is not overridden by an imported module.
log = logging.getLogger(__name__)

class InvitationController(BaseController):
    
    def show(self, workspace_id, email_id):
           
        ws = c.workspace
        
        if not ws: 
            log.warn("InvitationController().show(): Workspace %s does not exist." % ( workspace_id ) )
            abort(403)
        
        invitation = Invitation.get_by(email_id = email_id)
       
        if not invitation: 
            log.warn("InvitationController().show(): No invitation '%s' for workspace %s" \
                % ( email_id, workspace_id ) )
            ui_error(code="invitation_link_not_valid")
            return render('message/show.mako')
            #abort(403)

        if 'user_id' in session:
            if session['user_id'] == invitation.user_id:
                # User is already logged - redirect.
                redirect_to(url('teambox', workspace_id = session['workspace_id']))
            else:
                # User is logged as another user - unlog.
                init_session(c.workspace, reinit=True)
 
        if (ws.id==invitation.kws_id):
            ### Temp workaround to show notification management page for users with no passwords. ###
            session['tmp_notif_user_id'] = invitation.user_id
            session.save()
            c.notif_flag = True
            ### End workaround ###

            if ws.secured:
                c.email_id = email_id
                
                kcd_user = KcdUser.get_by(user_id = invitation.user_id, kws_id = invitation.kws_id)
                if not kcd_user:
                    log.warn("kcd user " + str(invitation.user_id) + " for workspace " + str(workspace_id) + " not found in database")
                    abort(403)

                if kcd_user.pwd: #prompt for password and for credentials download
                    c.show_pass = True
                    pass
                else: #prompt for credintials download
                    c.show_pass = False
                    pass
                return render('/invitation/show.mako')
            else: #authorize
                self._login(invitation)
        else:
            abort(403)
            
    def resend_show(self):
        return render('invitation/resend_show.mako')
    
    def create(self, workspace_id):
        
        email_id = None
        if 'email_id' in request.params:
            email_id = request.params['email_id']
        else:
            abort(403)
        
        ws = c.workspace
        invitation = Invitation.get_by(email_id = email_id)

        if (ws and invitation and ws.id==invitation.kws_id):
            kcd_user = KcdUser.get_by(user_id = invitation.user_id, kws_id = invitation.kws_id)
            if(kcd_user and kcd_user.pwd and ('password' in request.params) and request.params['password']==kcd_user.pwd):            
                self._login(invitation, kcd_user.pwd)
            else:
                ui_flash_error(message="You have not provided a valid password.")
                redirect_to(url('invite_show', workspace_id=workspace_id, email_id=email_id))
        else:
            abort(403)
        
    def resend_create(self, workspace_id):
        #verify email valid and send invitation mail
        if 'email' in request.params:
            if request.params['email'] == '':
                return render('invitation/resend_show.mako')
            user = User.get_by_lower_email(int(workspace_id), request.params['email'])
            if (user):
                # Instantiate a Kcd client.
                invitees = []
                invitee = WorkspaceInvitee()
                invitee.email_address = user.email
                invitee.send_mail = True
                
                invitees.append(invitee)
                message = "click on the link to join the teambox"
                
                kc = KcdClient(get_cached_kcd_external_conf_object())
                #TODO handle kcd errors, render confirmation messages
                kc.invite_users(workspace_id, message, invitees)
                ui_info(message="You have been re-invited to the Teambox, please check you email and follow instructions.")
                return render('invitation/resend_success.mako')

            else:
                #please contact ws owner
                ui_error(message="No invitation to this teambox had been sent to this email address." + \
                              " If you were not invited to this teambox, please contact the teambox owner.")
                return render('invitation/resend_show.mako')
                pass
        else:
            abort(403)
        
    def _login(self, invitation, password= None):
        # Set info in session.
        session['mode'] = MODE_WS
        session['email_id'] = invitation.email_id
        session['user_id'] = invitation.user_id
        session['secure'] = c.workspace.secured
        # TODO account for password changes
        session['password'] = password

        c.perms.addRole('normal')

        user = User.get_by(workspace_id=invitation.kws_id, id=invitation.user_id)
        if user:
            session['user'] = user.to_dict()
 
        # Save session.
        session.save()
       
        # Do a last minute permissions check on KCD.
        self._check_perms()
        
        # Redirect to teambox url.
        redirect_to(url('teambox', workspace_id = session['workspace_id']))

