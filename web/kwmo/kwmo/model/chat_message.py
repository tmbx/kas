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
#  - select all messages with [workspace_id]
#  - select all messages with [workspace_id] that are newer than [evt_id]
#  INDEX: [workspace, evt_id] OK IMPLICIT PRIMARY KEY
#  - (later?) select all messages with [workspace_id, channel_id]
#  - (later?) select all messages with [workspace_id, channel_id] that are newer than [evt_id]
#  (later?) INDEX: [workspace_id, channel_id, evt_id]

class ChatMessage(Entity):
    # Options
    using_options(shortnames=True)

    # Relations, fields    
    workspace = ManyToOne('Workspace', colname="workspace_id", primary_key=True)
    evt_id = Field(PGBigInteger, primary_key=True, autoincrement=False)
    date = Field(PGBigInteger)
    channel_id = Field(Integer)
    user_id = Field(Integer)
    msg = Field(Text)

    # User many to one relation
    using_table_options(ForeignKeyConstraint(["user_id", "workspace_id"], ["user.id", "user.workspace_id"]))
    user = GenericProperty(relation(User))

    # STATIC
    # Create a chat message from a workspace event.
    def from_evt(evt):
        # Create a new chat message instance.
        chat_msg = ChatMessage()

        # Fill chat message.
        chat_msg.workspace_id = evt.kws_id
        chat_msg.evt_id = evt.evt_id
        # workspace id # = evt.anp.get_u64(0)
        chat_msg.date = evt.anp.get_u64(1)
        chat_msg.channel_id = evt.anp.get_u32(2)
        chat_msg.user_id = evt.anp.get_u32(3)
        chat_msg.msg = evt.anp.get_str(4)

        return chat_msg
    from_evt = staticmethod(from_evt)

