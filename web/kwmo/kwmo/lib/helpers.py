# -*- coding: utf-8 -*-
"""Helper functions

Consists of functions to typically be used within templates, but also
available to Controllers. This module is available to both as 'h'.
"""
# Import helpers as desired, or define your own, ie:
# from webhelpers.html.tags import checkbox, password
from webhelpers import *
from routes import url_for, redirect_to

# Scaffolding helper imports
from webhelpers.html.tags import *
from webhelpers.html import literal
from webhelpers.pylonslib import Flash
import sqlalchemy.types as types
flash = Flash()
# End of.

def get_object_or_404(model, **kw):
    from pylons.controllers.util import abort
    """
    Returns object, or raises a 404 Not Found is object is not in db.
    Uses elixir-specific `get_by()` convenience function (see elixir source: 
    http://elixir.ematia.de/trac/browser/elixir/trunk/elixir/entity.py#L1082)
    Example: user = get_object_or_404(model.User, id = 1)
    """
    obj = model.get_by(**kw)
    if obj is None:
        abort(404)
    return obj

def string_cut(s, max_length):
    if len(s) > max_length: s = s[0:max_length] + '...'
    return s
    
