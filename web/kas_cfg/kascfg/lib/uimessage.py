from pylons import session, tmpl_context as c 

# Quick port from kwmo - don't use a message code map.
#from strings import message_codes_map
message_codes_map = {'unknown_code' : 'Unknown error.'}

# Show an information message in the web interface.
def ui_info(code=None, message=None, hide_after_ms=None):
    uim = UIMessage.info(code=code, message=message, hide_after_ms=hide_after_ms)
    c.glob_messages.append(uim)

# Show a warning message in the web interface.
def ui_warn(code=None, message=None, hide_after_ms=None):
    uim = UIMessage.warn(code=code, message=message, hide_after_ms=hide_after_ms)
    c.glob_messages.append(uim)

# Show an error message in the web interface.
def ui_error(code=None, message=None, hide_after_ms=None):
    uim = UIMessage.error(code=code, message=message, hide_after_ms=hide_after_ms)
    c.glob_messages.append(uim)

# Show an information message in the web interface (at next request).
def ui_flash_info(code=None, message=None, hide_after_ms=None):
    uim = UIMessage.info(code=code, message=message, hide_after_ms=hide_after_ms)
    session['uimessage'] = uim
    session.save()

# Show a warning message in the web interface (at next request).
def ui_flash_warn(code=None, message=None, hide_after_ms=None):
    uim = UIMessage.warn(code=code, message=message, hide_after_ms=hide_after_ms)
    session['uimessage'] = uim
    session.save()

# Show an error message in the web interface (at next request).
def ui_flash_error(code=None, message=None, hide_after_ms=None):
    uim = UIMessage.error(code=code, message=message, hide_after_ms=hide_after_ms)
    session['uimessage'] = uim
    session.save()

# UIMessage object.
class UIMessage(object):
    def __init__(self):
        self.reset()

    def reset(self):
        self.type = None
        self.message = None
        self.hide_after_ms = None

    def from_dict(self, d):
        self.reset()

        if d.has_key('type'): self.type = d['type']
        if d.has_key('message'): self.message = d['message']
        if d.has_key('hide_after_ms'): self.hide_after_ms = d['hide_after_ms']

        # Return self, although changes happen in place too.
        return self

    def to_dict(self):
        return {'type' : self.type, 'message' : self.message, 'hide_after_ms' : self.hide_after_ms}

    def set_code(self, code):
        if message_codes_map.has_key(code): self.message = message_codes_map[code]
        else: self.message = message_codes_map['unknown_code']

    def __repr__(self):
        return "<%s type='%s' message='%s' hide_after_ms='%s'>" % ( self.__class__.__name__, self.type, self.message, str(self.hide_after_ms) )
 
    @staticmethod
    def info(code=None, message=None, hide_after_ms=None):
        uim = UIMessage()
        uim.type = 'info'
        uim.set_code(code)
        if message: uim.message = message
        uim.hide_after_ms = hide_after_ms
        return uim

    @staticmethod
    def warn(code=None, message=None, hide_after_ms=None):
        uim = UIMessage()
        uim.type = 'warn'
        uim.set_code(code)
        if message: uim.message = message
        uim.hide_after_ms = hide_after_ms
        return uim

    @staticmethod
    def error(code=None, message=None, hide_after_ms=None):
        uim = UIMessage()
        uim.type = 'error'
        uim.set_code(code)
        if message: uim.message = message
        uim.hide_after_ms = hide_after_ms
        return uim

