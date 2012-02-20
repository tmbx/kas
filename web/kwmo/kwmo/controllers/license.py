import logging

from pylons import request, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render

log = logging.getLogger(__name__)

class LicenseController(BaseController):

    # License page.
    def show(self):
        c.add_headers = '<link href="css/kwmo/kwmo.css" rel="Stylesheet" type="text/css" />'
        return render('/teambox/license.mako')