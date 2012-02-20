import logging

from pylons import request, url, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kwmo.model.user_workspace_settings import UserWorkspaceSettings

log = logging.getLogger(__name__)

class TeamboxSettingsController(BaseController):
    
    ### Temp workaround to show notification management page for users with no passwords. ###
    # requires_auth = ['show', 'update']
    
    def show(self, workspace_id):
        #TODO: handle logged out users
        if 'tmp_notif_user_id' in session:
            user_id = session['tmp_notif_user_id']
        elif 'user_id' in session:
            user_id = session['user_id']
        else:
            abort(403)
            
        settings = UserWorkspaceSettings(user_id, workspace_id)
        
        c.notificationEnabled = settings.getNotificationsEnabled()
        c.summaryEnabled = settings.getSummaryEnabled()
        if (c.workspace.public):
            c.email_id = session['email_id']

        if 'HTTP_REFERER' in request.environ:
            session['notif_page_referer'] = request.environ['HTTP_REFERER']
            session.save()
            
        if 'notif_page_referer' in session:
            c.cancel_url = session['notif_page_referer']

            
        return render('/teambox/settings.mako')

    def update(self, workspace_id):

        notificationEnabled = (('notification' in request.params) and (request.params['notification']=='1'))
        summaryEnabled = (('summary' in request.params) and (request.params['summary']=='1'))
        
        #TODO: handle logged out users
        if 'tmp_notif_user_id' in session:
            user_id = session['tmp_notif_user_id']
        elif 'user_id' in session:
            user_id = session['user_id']
        else:
            abort(403)

        
        settings = UserWorkspaceSettings(user_id, workspace_id)
        settings.setNotificationsEnabled(notificationEnabled)
        settings.setSummaryEnabled(summaryEnabled)
        settings.save()
       
        if('notif_page_referer' in session):
             redirect_to(session['notif_page_referer'])
        else:
             redirect_to(request.environ['HTTP_REFERER'])


#        if (c.workspace.public):
#            redirect_to(url('teambox_pubws_show', workspace_id=workspace_id, email_id=session['email_id']))
#        else:
#            redirect_to(url('teambox', workspace_id = workspace_id))
