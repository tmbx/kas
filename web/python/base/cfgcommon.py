from kbase import *
import ConfigParser
import strings_base
import kgetstrings
import kweb_getstrings

# This store contains the globals shared between the interfaces.
cfg_common = PropStore()

# Global strings store. Keys are application names and values are string
# dictionaries.
cfg_common.strings = {}
cfg_common.strings["base"] = strings_base.strings

# Reference to the request object.
cfg_common.request = None

# Reference to the mako_render() function.
cfg_common.mako_render = None

# This function is a wrapper around kgetstrings.get_string(). It uses a global
# strings dictionary.
def GT(key, app=None, allow_basic_html=0):
    if allow_basic_html: return kweb_getstrings.get_html_escaped_string(cfg_common.strings, key, app, 0, 1)
    return kgetstrings.get_string(cfg_common.strings, key, app)

# Return the value of the GET or POST variable specified. If the variable does
# not exist, 'None' is returned. Files are not retrieved with this method.
def get_var(key):
    if cfg_common.request.params.has_key(key): return cfg_common.request.params[key]
    return None

# Convert latin1 string to UTF8 string.
def ltu(s):
    #return unicode(s, 'latin1').encode('utf8')
    return unicode(s, 'latin1')

# This function obtains configuration from a ConfigParser instance, and allows a
# default value as well.
def parser_get_default(parser, section, option, def_value):
    try:
        return parser.get(section, option)
    except ConfigParser.NoOptionError:
        return def_value

# This class is used to manage variable stores used in template rendering.
class TemplateStore(object):
    def __init__(self):
        # Dictionary of variable stores.
        self.store_dict = {}
    
    # Add the variable store specified.
    def add_store(self, name):
        store = PropStore()
        store.name = name
        self.store_dict[store.name] = store
        return store
    
    # Return the variable store specified.
    def get_store(self, name):
        return self.store_dict[name]
     
    # Add the variables specified to the store specified.
    def fill_store(self, store, d):
        for key in d.keys():
            store[key] = d[key]
    
    # Render the template specified and return the result.
    def render(self, template_path):
        return cfg_common.mako_render(template_path, { "store_dict" : self.store_dict })

