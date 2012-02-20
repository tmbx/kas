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

class WSRequest(Entity):
    # Options
    using_options(shortnames=True) #, tablename="ws_request")

    # Relations, Fields
    workspace = ManyToOne('Workspace', colname="workspace_id", primary_key=True)
    request_id = Field(PGBigInteger, primary_key = True, autoincrement=False)
    req_date = Field(PGBigInteger)
    user_id = Field(Integer)
    subject = Field(Text)

    # User many to one relation
    using_table_options(ForeignKeyConstraint(["user_id", "workspace_id"], ["user.id", "user.workspace_id"]))
    user = GenericProperty(relation(User))

    # Static
    # Create a workspace request from a workspace event.
    def from_evt(evt):
        # Extract information from event.
        workspace_id = evt.kws_id
        # workspace id # = evt.anp.get_u64(0)
        date = evt.anp.get_u64(1)
        if evt.minor >= 3: request_id = evt.anp.get_u64(2)
        else: request_id = evt.anp.get_u32(2)
        user_id = evt.anp.get_u32(3)
        subject = evt.anp.get_str(4)

        # Workaround part A:
        # Delete former requests with the same (workspace_id - request_id) key.
        while 1:
            ws_req = Session.query(WSRequest).filter_by(workspace_id=workspace_id, request_id=request_id).first()
            if not ws_req: break
            ws_req.delete()

        # Create a new workspace request instance.
        ws_req = WSRequest()

        # Fill request.
        ws_req.workspace_id = workspace_id
        ws_req.request_id = request_id
        ws_req.req_date = date
        ws_req.user_id = user_id
        ws_req.subject = subject

        return ws_req
    from_evt = staticmethod(from_evt)

    # Return if specified user requested a workspace in the last <max_seconds>.
    @staticmethod
    def requested_in_last_seconds(workspace_id, user_id, max_seconds):
        query = """SELECT * FROM wsrequest
                                WHERE workspace_id=%i
                                    AND user_id=%i
                                    AND (date_part('epoch', now()) - req_date) < %i
                                    ORDER BY req_date DESC
                                    LIMIT 1""" \
                                % ( workspace_id, user_id, max_seconds)
        log.debug("requested_in_last_seconds(): query='%s'." % ( query ) )
        r = Session.execute(query)
        rr = r.fetchone()
        Session.rollback()
        if rr:
            return True
        return False

