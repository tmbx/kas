from freemium.model.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *

class User(Entity):
    # Options
    using_options(tablename="freemium_users")

