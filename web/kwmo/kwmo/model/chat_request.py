from kwmo.model.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *
from sqlalchemy import ForeignKeyConstraint
from sqlalchemy.orm import relation
from sqlalchemy.databases.postgres import PGBigInteger

from kwmo.model.workspace import Workspace
from kwmo.model.user import User

# Note: this class includes a workaround for a former bug in which request IDs were re-used.
# This happended because requests IDs were stored in the KWMO db and the DB was rebuilt at every update...
# Current requests and their acceptation are serial, and are of no use today, so the workaround is
# to delete former entries with the same (workspace_id - request_id) key.

import logging
# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

class ChatRequest(Entity):
    # Options
    using_options(shortnames=True)

    # Relations, Fields
    workspace = ManyToOne('Workspace', colname="workspace_id", primary_key=True)
    request_id = Field(PGBigInteger, primary_key = True, autoincrement=False)
    req_date = Field(PGBigInteger)
    user_id = Field(Integer)
    timeout = Field(Integer)
    accepted = Field(Boolean, default = False)
    accepted_date = Field(PGBigInteger)
    channel_id = Field(Integer)

    # User many to one relation
    using_table_options(ForeignKeyConstraint(["user_id", "workspace_id"], ["user.id", "user.workspace_id"]))
    user = GenericProperty(relation(User))

    # Create a chat request from a workspace event.
    @staticmethod
    def from_evt(evt):
        # Extract information from event.
        workspace_id = evt.kws_id
        # workspace id # = evt.anp.get_u64(0)
        date = evt.anp.get_u64(1)
        if evt.minor >= 3: request_id = evt.anp.get_u64(2)
        else: request_id = evt.anp.get_u32(2)
        user_id = evt.anp.get_u32(3)
        # subject # = evt.anp.get_str(4)
        timeout = evt.anp.get_u32(5)

        # Workaround part A:
        # Delete former requests with the same (workspace_id - request_id) key.
        while 1:
            chat_req = Session.query(ChatRequest).filter_by(workspace_id=workspace_id, request_id=request_id).first()
            if not chat_req: break
            chat_req.delete()

        # Create a new chat request instance.
        chat_req = ChatRequest()

        # Fill chat request.
        chat_req.workspace_id = workspace_id
        chat_req.request_id = request_id
        chat_req.req_date = date
        chat_req.user_id = user_id
        chat_req.timeout = timeout

        return chat_req

    # Accept a chat request.
    @staticmethod
    def accept_from_evt(evt):
        # Extract informations from event.
        workspace_id = evt.anp.get_u64(0)
        date = evt.anp.get_u64(1)
        if evt.minor >= 3: request_id = evt.anp.get_u64(2)
        else: request_id = evt.anp.get_u32(2)
        user_id = evt.anp.get_u32(3)
        channel_id = evt.anp.get_u32(4)

        # Get the corresponding chat request.
        chat_req = Session.query(ChatRequest).filter_by(workspace_id=workspace_id, request_id=request_id).first()

        # Update the chat request.
        chat_req.accepted = True
        chat_req.accepted_date = date
        chat_req.channel_id = channel_id

        # Workaround part B:
        # Nothing to do - read the note above.

        return chat_req

    # Return if specified user had a chat acceptation in the last <max_seconds>.
    @staticmethod
    def accepted_lately(workspace_id, user_id):
        max_seconds = 600
        query = """SELECT * FROM chatrequest 
                                WHERE workspace_id=%i 
                                    AND user_id=%i 
                                    AND accepted=True
                                    AND (date_part('epoch', now()) - accepted_date) < %i
                                    ORDER BY accepted_date DESC
                                    LIMIT 1""" \
                                % ( workspace_id, user_id, max_seconds)
        log.debug("accepted_in_last_seconds(): query='%s'." % ( query ) )
        r = Session.execute(query)
        rr = r.fetchone()
        Session.rollback()
        if rr:
            return True
        return False


