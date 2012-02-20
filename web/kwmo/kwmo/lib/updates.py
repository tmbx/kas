from kwmo.model import User

# Update session to version 1.
def update_session_v1(c, session):
    # Add session variables.
    session['version'] = 1
    session['secure'] = bool(c.workspace.secured)
    session['last_perm_check_id'] = c.workspace.last_perm_check_id
    session['last_evt_ws_id'] = c.workspace.evt_ws_id
    session['last_evt_user_id'] = c.workspace.evt_user_id

    # Reload user in sessin since some fields were added in the table.
    if 'user' in session and 'user_id' in session and session['user_id']:
        session['user'] = User.get_by(workspace_id=c.workspace.id, id=session['user_id'])
 
    # Clean unused session variables.
    if 'chat_ready' in session: del session['chat_ready']
    if 'kfs_ready' in session: del session['kfs_ready']
    if 'kfsup_ready' in session: del session['kfsup_ready']
    if 'kfsdown_ready' in session: del session['kfsdown_ready']
    if 'vnc_ready' in session: del session['vnc_ready']
    if 'max_upload_size' in session: del session['max_upload_size']
    if 'logged' in session: del session['logged']
    if 'priv' in session: del session['priv']
    if 'kfs_share_id' in session: del session['kfs_share_id']
    if 'kfs_chroot_inode_id' in session: del session['kfs_chroot_inode_id']
    if 'chat_channel_id' in session: del session['chat_channel_id']
    if 'pubws_perm_mode' in session: del session['pubws_perm_mode']

    # Update permissions.
    update_perms_v1(c, session)

# Update permissions to version 1.
def update_perms_v1(c, session):
    if c.is_admin:
        # Apply read-only admin interface permissions.
        c.perms.addRole('roadmin')

    elif c.workspace.is_public:
        # Apply basic skurl permissions.
        c.perms.addRole('skurl')

    else:
        # Apply normal KWMO permissions.
        c.perms.addRole('normal')

