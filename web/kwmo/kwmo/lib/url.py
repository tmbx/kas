base_url_paths = {
    'credentials_download' : '/teambox/d',
    'teambox_pubws_set_identity' : '/teambox/pb_set_identity',
    'teambox_pubws_chat_request' : '/teambox/chat_request',
    'teambox_pubws_chat_request_result' : '/teambox/chat_request_result',
    'teambox_pubws_kfsup_request' : '/teambox/kfs_upload_request',
    'teambox_pubws_kfsdown_request' : '/teambox/kfs_download_request',
    'teambox_pubws_create_request' : '/teambox/ws_create_request',
    'teambox_updater' : '/teambox/updater',
    'teambox_post_chat' : '/teambox/chat',
    'teambox_download' : '/teambox/download',
    'admin_updater' : '/teambox_admin/updater',
    'admin_file_download' : '/teambox_admin/download',
    'teambox_upload' : '/teambox/upload',
    'vnc_session_start' : '/teambox/vnc_start',
    'teambox_serverlog' : '/teambox/serverlog'
}

# Get base URL for sending to javascript.
def get_base_url_path(arg):
    return base_url_paths[arg]

# Get a list of base URLs for sending to javascript.
def get_base_url_paths(*args):
    paths  = {}
    for url_name in args:
        paths[url_name] = base_url_paths[url_name]
    return paths

