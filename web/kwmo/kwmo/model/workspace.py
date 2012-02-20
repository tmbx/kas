from kwmo.model.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *
from sqlalchemy.databases.postgres import PGBigInteger

class Workspace(Entity):
    # Options
    using_options(shortnames=True)
    
    # Relations and fields
    id = Field(PGBigInteger, primary_key=True, autoincrement = False)
    name = Field(Text)
    creation_date = Field(PGBigInteger)
    compat_v2 = Field(Boolean, default=True)
    secured = Field(Boolean, default=True)
    public = Field(Boolean, default=False)
    deleted = Field(Boolean, default=False)
    frozen = Field(Boolean, default=False)
    deep_frozen = Field(Boolean, default=False)
    last_perm_check_id = Field(PGBigInteger, default=0)
    last_event_id = Field(PGBigInteger, default=0)
    evt_ws_id = Field(PGBigInteger, default=0)
    evt_user_id = Field(PGBigInteger, default=0)
    evt_kfs_id = Field(PGBigInteger, default=0)
    evt_chat_id = Field(PGBigInteger, default=0)
    evt_vnc_id = Field(PGBigInteger, default=0)
    evt_skurl_id = Field(PGBigInteger, default=0)

    users = OneToMany('User')

    # Delete workspace. It must always work, even in an already deleted workspace. 
    @staticmethod
    def delete_workspace(workspace_id):
        # This part is done with the current transaction (commit must be done in the callers).
        ws = Workspace.get_by(id=workspace_id)
        if not ws: return
        ws.deleted = True

        Session.execute("DELETE FROM chatmessage WHERE workspace_id=%i" % ( workspace_id ))
        Session.execute("DELETE FROM chatrequest WHERE workspace_id=%i" % ( workspace_id ))
        Session.execute("DELETE FROM kfsnode WHERE workspace_id=%i" % ( workspace_id ))
        Session.execute("DELETE FROM vncsession WHERE workspace_id=%i" % ( workspace_id ))
        Session.execute("DELETE FROM wsrequest WHERE workspace_id=%i" % ( workspace_id ))
        Session.execute("DELETE FROM \"user\" WHERE workspace_id=%i" % ( workspace_id ))

