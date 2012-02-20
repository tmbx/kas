import logging

from pylons import request, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kascfg.lib.base import BaseController, render

log = logging.getLogger(__name__)

class AboutController(BaseController):

    def show(self):
        return redirect_to('http://www.teambox.co')

