import logging

from pylons import request,url, response, session, config, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render

from kwmo.model.kcd.invitation import Invitation
from kwmo.model.kcd.kcd_user import KcdUser
from kcd_lib import WorkspaceInvitee
from kwmo.lib.config import get_cached_kcd_external_conf_object
from kwmo.lib.kwmo_kcd_client import KcdClient
from kwmo.lib.uimessage import ui_flash_error

log = logging.getLogger(__name__)

#TODO: refactor this appropriately together with invitation controller
class OldWleuController(BaseController):

    def authorize_show(self, workspace_id):
        #prompt for password
        if 'user_id' in session:
            redirect_to(url('teambox', workspace_id = session['workspace_id']))
        
        if c.workspace.compat_v2:
            return render('/old_wleu/authorize_show.mako')
        else:
            abort(404)

    def verify_pwd(self, workspace_id):
        # check for password in users table
        # if found and one user:
        #    send invitation if one doesn't exist from system user
        #    login with new/retrieved email id
        # else if found and multiple users:
        #    show members with same password
        # else:
        #    display error msg
        if 'password' in request.params and request.params['password'] and c.workspace.compat_v2:
            kcd_users = KcdUser.query.filter_by(pwd = request.params['password'], kws_id = workspace_id).all()
            if (len(kcd_users)==1):
                self._invite_and_login(kcd_users[0], workspace_id, request.params['password'])
            elif (len(kcd_users)>1):
                #FIXME: DO NOT save objects in session
                session['same_pwd_members'] = kcd_users
                session['member_password'] = request.params['password']
                session.save()

                redirect_to(url('show_select_member_old_wleu', workspace_id=workspace_id))
            else:
                ui_flash_error(message='You have provided an invalid password.')
                redirect_to(url('authorize_old_wleu', workspace_id=workspace_id))
        else:
            ui_flash_error(message='You have not provided a valid password.')
            redirect_to(url('authorize_old_wleu', workspace_id=workspace_id))
        
    def show_members(self, workspace_id):
        if 'same_pwd_members' in session:
            c.same_pwd_members = session['same_pwd_members']
            return render('/old_wleu/show_members.mako')
        else:
            abort(403)
    
    def set_user(self, workspace_id):

        if 'member_password' in session and 'user_id' in request.params:
            password = session['member_password']
            del session['member_password']
            del session['same_pwd_members']
            session.save()

            user = KcdUser.get_by(user_id = request.params['user_id'], pwd=password, kws_id=workspace_id)
            if(user):
                self._invite_and_login(user, workspace_id, password)
            else:
                abort(403)
        else:
            abort(403)
            
    def _invite_and_login(self, usr, workspace_id, password):
        invitation = Invitation.get_by(kws_id=workspace_id, inviting_user_id=0, user_id=usr.user_id)
        if invitation:
            email_id = invitation.email_id
        else:
            invitees = []
            invitee = WorkspaceInvitee()
            invitee.email_address = usr.email
            invitee.send_mail = False
            invitees.append(invitee)
            
            kc = KcdClient(get_cached_kcd_external_conf_object())
            #TODO handle kcd errors
            ws_url, invitees_result = kc.invite_users(workspace_id, '', invitees)
            email_id = invitees_result[0].email_id
            
        redirect_to(url('invite_create',workspace_id=workspace_id, email_id=email_id, password=password))
