from kwmo.model.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *
from sqlalchemy.databases.postgres import PGBigInteger
from kpg import escape_pg_string

# Workspace user, this is a weak entity.
class User(Entity):
    # Options
    using_options(shortnames=True)

    # Relations, fields
    id = Field(Integer, primary_key=True, autoincrement = False)
    workspace = ManyToOne('Workspace', primary_key=True)

    real_name = Field(Text)
    admin_name = Field(Text)
    email = Field(Text)
    evt_id = Field(PGBigInteger, autoincrement=False)
    root = Field(Boolean, default=False)
    admin = Field(Boolean, default=False)
    manager = Field(Boolean, default=False)
    out = Field(Boolean, default=False)
    locked = Field(Boolean, default=False)
    banned = Field(Boolean, default=False)

    # Static
    # Create the root user of a workspace.
    def create_root_user(workspace_id):
        user = User()
        user.id = 0
        user.workspace_id = workspace_id
        user.real_name = "System Administrator"
        user.admin_name  = ""
        user.email = ""
        user.evt_id = 0
        user.root = True
        user.admin = True
        user.manager = True

        return user
    create_root_user = staticmethod(create_root_user)

    # Static
    # Create workspace owner user from the workspace creation event. 
    def from_creation_evt(evt):
        # Create workspace owner user instance.
        user = User()
        user.evt_id = evt.evt_id

        # Fill user.
        if evt.minor >= 3:
            user.workspace_id = evt.kws_id
            # workspace_id # = evt.anp.get_u64(0)
            # date # = evt.anp.get_u64(1)
            user.id = evt.anp.get_u32(2)
            user.real_name = evt.anp.get_str(3)
            user.email = evt.anp.get_str(4)
            # ... #
 
        else:
            user.workspace_id = evt.kws_id
            # workspace_id # = anp.get_u64(0)
            # date # = evt.anp.get_u64(1)
            user.id = evt.anp.get_u32(2)
            user.real_name = evt.anp.get_str(3)
            user.email = evt.anp.get_str(4)
            # ... #

        return user
    from_creation_evt = staticmethod(from_creation_evt)

    # Static
    # Create new users from a workspace event (several users can be invited in a single event).
    def from_invitation_evt(evt):
        users_list = []

        workspace_id =  evt.anp.get_u64(0)
        # date # = evt.anp.get_u64(1)

        if evt.minor >= 3:
            # invitor user id # =  evt.anp.get_u32(2)

            # Number of invited users
            nb = evt.anp.get_u32(3)

            # Get all users from the event.
            offset = 4
            for i in range(0, nb):
                user = User()
                user.evt_id = evt.evt_id
                user.workspace_id = workspace_id
                user.id = evt.anp.get_u32(offset + 0)
                user.real_name = evt.anp.get_str(offset + 1)
                user.email = evt.anp.get_str(offset + 2)
                # ... #
                offset += 4

                users_list.append(user)

        else:
            # Number of invited users
            nb = evt.anp.get_u32(2)

            offset = 3
            for i in range(0, nb):
                # Create new user instance.
                user = User()
                user.evt_id = evt.evt_id

                # Fill user.
                user.workspace_id = workspace_id
                user.id = evt.anp.get_u32(offset + 0)
                user.real_name = evt.anp.get_str(offset + 1)
                user.email = evt.anp.get_str(offset + 2)
                # ... #
                offset += 6

                users_list.append(user)

        return users_list
    from_invitation_evt = staticmethod(from_invitation_evt)

    # Update user status when registered.
    @staticmethod
    def update_registered_from_evt(evt):
        workspace_id = evt.kws_id
        # workspace_id # =  evt.anp.get_u64(0)
        # date # = evt.anp.get_u64(1)
        user_id = evt.anp.get_u32(2)
        real_name = evt.anp.get_str(3)
        # ...
        user = Session.query(User).filter_by(workspace_id=workspace_id, id=user_id).first()
        user.real_name = real_name
        user.evt_id = evt.evt_id

        return user

    # Return first user having a matching workspace ID and lower case email.
    @staticmethod
    def get_by_lower_email(workspace_id, email):
        query = """SELECT * FROM "user"
                                WHERE workspace_id = %i
                                    AND lower(email) = lower(%s)
                                LIMIT 1""" % ( workspace_id, escape_pg_string(email) )
        r = Session.execute(query)
        rr = r.fetchone()
        Session.rollback()
        return rr

