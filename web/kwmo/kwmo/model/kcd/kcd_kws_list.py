from kwmo.model.kcd.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *

class KcdKwsList(Entity):
    # Options
    using_options(shortnames=True, tablename="kcd_kws_list",  autoload=True)

