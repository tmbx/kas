import logging
import simplejson
import time

from kwmo.lib.config import get_cached_kcd_external_conf_object

from pylons import request, response, session, config, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kwmo.lib.jsonify import kjsonify # Custom version
from kwmo.lib.exceptions import KAjaxViewException
from kwmo.lib.staterequest import StateRequest, state_request_get
from kwmo.lib.kwmo_kcd_client import KcdClient

log = logging.getLogger(__name__)

class ChatController(BaseController):
    
    requires_auth = ['post_message']  

    # This method handles chat posts.
    # It uses a custom @kjsonify decorator that catches exceptions and sends
    # json dictionary with the exception instead of dumping an html trace.
    @kjsonify
    def post_message(self, workspace_id):
        workspace_id = int(workspace_id)

        # Get ajax parameters.
        channel_id = int(request.params['channel_id'])
        message = request.params['message'] #.encode('latin1')

        if not c.perms.hasPerm('chat.post.channel.%i' % (channel_id)):
            log.error("Chat post denied: user has not the right permissions.")
            raise KAjaxViewException("Chat post denied.")

        user_id = session['user_id']

        log.debug("Chat post request: workspace_id=%i channel_id=%i user_id=%i" % \
            ( workspace_id, channel_id, user_id ) )

        # Instantiate a Kcd client.
        kc = KcdClient(get_cached_kcd_external_conf_object())

        # Post the message.
        kc.post_chat_msg_request(workspace_id, channel_id, user_id, message)

        if 0:
            # Get first update directly (not finished: need other motifications at client side
            # so the update is not shown twice.
            state_req_id = int(request.params['state_req_id'])
            last_evt_chat_id = int(request.params['last_evt_chat_id'])

            time.sleep(0.5) # Wait for kwsfetcher to fetch the message.
            flags = ( StateRequest.STATE_FORCE_SYNC | StateRequest.STATE_WANT_CHAT )
            params = { 'req_id' : state_req_id, 'chat_channel_id' : channel_id, 'last_evt_chat_id' : last_evt_chat_id }
            updater_state_dict = state_request_get(c, session, flags, params)

            return { 'updater' : updater_state_dict }

        return {}

