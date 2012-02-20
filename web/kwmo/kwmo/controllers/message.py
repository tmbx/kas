import logging

from pylons.controllers.util import abort, redirect_to
from kwmo.lib.strings import message_codes_map
from kwmo.lib.base import BaseController, render
from kwmo.lib.uimessage import ui_warn

log = logging.getLogger(__name__)

class MessageController(BaseController):

    # Show generic message and/or error - deprecated - remove in a few months after 2009-12-02,
    # where everyone (assumption) have reloaded KWMO pages and won't be redirected by javascript the old way).
    def showold(self, code):
        ui_warn(code=code)
        return render('message/show.mako')

    # Show generic message or error.
    def show(self):
        return render('message/show.mako')

