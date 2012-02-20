import logging

from pylons import request, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to
from routes import url_for
from kascfg.lib.base import BaseController, render, ui_info, ui_warn, ui_flash_error
from kascfg.lib.services import K2Services
from kascfg.lib.config import save_master_config, get_status_line, KPLATPATH

# from kpython
from krun import get_cmd_output

log = logging.getLogger(__name__)

class StatusController(BaseController):

    # List of actions that require authentication.
    require_auth = ['show', 'switch_to_prod', 'switch_to_maint']

    # Show kas status.
    def show(self):

        if c.mc.production_mode:
            # Get kplatshell status line.
            status_line = get_status_line()
            if status_line == 'OK': ui_info(message='The services are running normally.')
            else: ui_warn(message=status_line)

        # Show production line.
        if c.mc.production_mode: ui_info(message='This server is in production mode.')
        else: ui_warn(message='This server is in maintenance mode.')

        c.production_mode = bool(c.mc.production_mode)

        # Return template.
        return render('/status/show.mako')

    # Switch to production mode.
    def switch_to_prod(self):

        # Go to production mode.
        try:
            get_cmd_output(['sudo', '/usr/bin/unblock_signals', KPLATPATH, 'production'])
        except Exception, e:
            ui_flash_error(message=str(e))

        # Redirect to status page.
        redirect_to(url_for('status'))

    # Switch to maintenance mode.
    def switch_to_maint(self):

        # Go to maintenance mode.
        try:
            get_cmd_output(['sudo', '/usr/bin/unblock_signals', KPLATPATH, 'maintenance'])
        except Exception, e:
            ui_flash_error(message=str(e))

        # Redirect to status page.
        redirect_to(url_for('status'))

