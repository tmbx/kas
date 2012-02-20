"""Routes configuration

The more specific and detailed routes should be defined first so they
may take precedent over the more generic routes. For more information
refer to the routes manual at http://routes.groovie.org/docs/
"""
from pylons import config
from routes import Mapper

def make_map():
    """Create, configure and return the routes Mapper"""
    map = Mapper(directory=config['pylons.paths']['controllers'],
                 always_scan=config['debug'])
    map.minimization = False

    # The ErrorController route (handles 404/500 error pages); it should
    # likely stay at the top, ensuring it can always be resolved
    map.connect('/error/{action}', controller='error')
    map.connect('/error/{action}/{id}', controller='error')

    # CUSTOM ROUTES HERE
  
    # Keep compatibility for a while, because it could still be used by javascript. 
    map.connect('message_showold', '/message/{code}', controller="message", action="showold")
    # /Keep
    map.connect('message_show', '/teambox/message/{workspace_id}', controller="message", action="show")
    map.connect('wleu', '/teambox/s/{workspace_id}/{email_id}', controller="teambox", action="show")
    map.connect('invitation_url', '/teambox/i/{workspace_id}/{email_id}', controller="invitation", action="show")

    map.connect('invite_show', '/invite/{workspace_id}/email/{email_id}', controller="invitation", action="show")
    map.connect('invite_create', '/invite/{workspace_id}', controller="invitation", action="create")

    map.connect('invite_resend_show', '/invite/{workspace_id}/resend_show', controller="invitation", action="resend_show")
    map.connect('invite_resend_create', '/invite/{workspace_id}/resend', controller="invitation", action="resend_create", 
        conditions=dict(method=['POST']))
    
    map.connect('credentials_download', '/teambox/d/{workspace_id}/{email_id}', controller="credentials_file", action="get")

    map.connect('teambox_pubws_show', '/teambox/public/{workspace_id}/{email_id}', 
        controller="skurl_teambox", action="show")
    map.connect('teambox_pubws_logout', '/teambox/publib/logout/{workspace_id}/{email_id}',
        controller="skurl_teambox", action="logout")
    map.connect('teambox_pubws_set_identity', '/teambox/pb_set_identity/{workspace_id}', 
        controller="skurl_teambox", action="pb_set_identity")
    map.connect('teambox_pubws_chat_request', '/teambox/chat_request/{workspace_id}', 
        controller="skurl_teambox", action="chat_request")
    map.connect('teambox_pubws_chat_request_result', '/teambox/chat_request_result/{workspace_id}/{req_id}',
        controller="skurl_teambox", action="chat_request_result")
    map.connect('teambox_pubws_kfsup_request', '/teambox/kfs_upload_request/{workspace_id}', 
        controller="skurl_teambox", action="kfs_upload_request")
    map.connect('teambox_pubws_kfsdown_request', '/teambox/kfs_download_request/{workspace_id}', 
        controller="skurl_teambox", action="kfs_download_request")
    map.connect('teambox_pubws_create_request', '/teambox/ws_create_request/{workspace_id}', 
        controller="skurl_teambox", action="ws_create_request")
    map.connect('license', '/teambox/license', controller='license', action='show')

    # Normal and skurl
    map.connect('teambox_admin_login', '/teambox_admin/login/{workspace_id}', controller='admin_teambox', action='login')
    map.connect('teambox_admin_logout', '/teambox_admin/logout/{workspace_id}', controller='admin_teambox', action='logout')
    map.connect('teambox_logout', '/teambox/logout/{workspace_id}', controller='teambox', action='logout')
    map.connect('teambox_updater', '/teambox/updater/{workspace_id}', controller='teambox', action='updater')
    map.connect('teambox_post_chat', '/teambox/chat/{workspace_id}', controller='chat', action='post_message')
    map.connect('teambox_download', '/teambox/download/{workspace_id}', controller='file_download', action='download')
    map.connect('teambox_upload', '/teambox/upload/{workspace_id}', controller='file_upload', action='upload')
    map.connect('teambox_serverlog', '/teambox/serverlog/{workspace_id}', controller='client_logger', action='serverlog')

    map.connect('vnc_session_start', '/teambox/vnc_start/{workspace_id}/{vnc_session_id}', controller='vnc', action='start_vnc_session')

    map.connect('teambox_admin', '/teambox_admin/{workspace_id}', controller='admin_teambox', action='show')
    map.connect('teambox_admin_updater', '/teambox_admin/updater/{workspace_id}', controller='admin_teambox',
        action='updater')
    map.connect('teambox_admin_download', '/teambox_admin/download/{workspace_id}', controller='admin_file_download',
        action='download')
    map.connect('teambox_settings', '/teambox/{workspace_id}/settings', controller='teambox_settings', action='show', conditions=dict(method=['GET']))
    map.connect('teambox_settings_update', '/teambox/{workspace_id}/settings', controller='teambox_settings', action='update', conditions=dict(method=['POST']))
    map.connect('teambox', '/teambox/{workspace_id}', controller="teambox", action="show")

    #old url support
    map.connect('v1_wleu', '/workspaces/{kws_id}/', controller='old_dispatcher', action='dispatch_v1')
    map.connect('v0_wleu', '/', controller='old_dispatcher', action='dispatch_v0')

    map.connect('authorize_old_wleu', '/teambox/authorize_wleu/{workspace_id}', controller='old_wleu', action='authorize_show', conditions=dict(method=['GET']))
    map.connect('verify_pwd_old_wleu', '/teambox/authorize_wleu/{workspace_id}', controller='old_wleu', action='verify_pwd', conditions=dict(method=['POST']))
    map.connect('show_select_member_old_wleu', '/teambox/select_user_wleu/{workspace_id}', controller='old_wleu', action='show_members', conditions=dict(method=['GET']))
    map.connect('select_member_old_wleu', '/teambox/select_user_wleu/{workspace_id}', controller = 'old_wleu', action='set_user', conditions=dict(method=['POST']))
    
    #map.connect('/{controller}/{action}')
    #map.connect('/{controller}/{action}/{id}')

    return map
