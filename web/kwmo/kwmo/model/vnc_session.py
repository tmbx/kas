from kwmo.model.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *
from sqlalchemy.databases.postgres import PGBigInteger
from sqlalchemy.orm import relation
from sqlalchemy import ForeignKeyConstraint

from kwmo.model.workspace import Workspace
from kwmo.model.user import User

# Requests types, and needed indexes:
#  - select all vnc sessions with [workspace_id]
#  - select all vnc sessions with [workspace_id, session_id]
# INDEXES: [workspace_id, session_id] OK IMPLICIT PRIMARY KEY

class VncSession(Entity):
    # Options
    using_options(shortnames=True)
   
    # Relations, fields
    workspace = ManyToOne('Workspace', primary_key=True)
    session_id = Field(PGBigInteger, primary_key=True, autoincrement=False)
    date = Field(PGBigInteger)
    user_id = Field(Integer)
    subject =  Field(Text)
    port = Field(Integer)
    
    # User many to one relation
    using_table_options(ForeignKeyConstraint(["user_id", "workspace_id"], ["user.id", "user.workspace_id"]))
    user = GenericProperty(relation(User))

    # Static
    # Create a vnc session from a workspace event.
    def start_from_evt(evt):
        # Create a new vnc session instance.
        vnc_sess = VncSession()

        # Fill vnc session.
        vnc_sess.workspace_id = evt.kws_id
        # workspace_id # = evt.anp.get_u64(0)
        vnc_sess.date = evt.anp.get_u64(1)
        vnc_sess.user_id = evt.anp.get_u32(2)
        vnc_sess.session_id = evt.anp.get_u64(3)
        vnc_sess.subject = evt.anp.get_str(4)

        return vnc_sess
    start_from_evt = staticmethod(start_from_evt)

    # Static
    # Delete a vnc session from a workspace event.
    def stop_from_evt(evt):
        # Get informations from event.
        workspace_id = evt.kws_id
        # workspace_id # = evt.anp.get_u64(0)
        # date # = evt.anp.get_u64(1)
        user_id = evt.anp.get_u32(2)
        session_id = evt.anp.get_u64(3)

        # Get the corresponding vnc request.
        vnc_sess = Session.query(VncSession).filter_by(workspace_id=workspace_id, session_id=session_id).first()

        # Delete the session.
        vnc_sess.delete()

        return vnc_sess
    stop_from_evt = staticmethod(stop_from_evt)

