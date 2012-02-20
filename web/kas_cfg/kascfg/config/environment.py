"""Pylons environment configuration"""
import os

from mako.lookup import TemplateLookup
from pylons import config
from kasmodel import RootConfigNode
import kascfg.lib.app_globals as app_globals
import kascfg.lib.helpers
from kascfg.config.routing import make_map
from kascfg.lib.config import cache_master_config
import kascfg.model as model
import kascfg.model.kcd as kcd_model
import kascfg.model.freemium as freemium_model

def load_environment(global_conf, app_conf):
    """Configure the Pylons environment via the ``pylons.config``
    object
    """
    # Pylons paths
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    top = os.path.abspath(os.path.join(root, '../..'))
    paths = dict(root=root,
                 controllers=os.path.join(root, 'controllers'),
                 static_files=os.path.join(root, 'public'),
                 templates=[os.path.join(root, 'templates'), os.path.join(top, 'templates/base')])

    # Initialize config with the basic options
    config.init_app(global_conf, app_conf, package='kascfg', paths=paths)

    config['routes.map'] = make_map()
    config['pylons.app_globals'] = app_globals.Globals()
    config['pylons.h'] = kascfg.lib.helpers

    # Create the Mako TemplateLookup, with the default auto-escaping
    config['pylons.app_globals'].mako_lookup = TemplateLookup(
        directories=paths['templates'],
        module_directory=os.path.join(app_conf['cache_dir'], 'templates'),
        input_encoding='utf-8', output_encoding='utf-8',
        imports=['from webhelpers.html import escape'],
        default_filters=['escape'])

    # Get default master file path (if not defined in the paster configuration file).
    if not config.has_key('master_file_path'):
        config['master_file_path'] = RootConfigNode.master_file_path

    # Cache master config.
    cache_master_config(path=config['master_file_path'])

    # Initialize models.
    #model.init_model() # not needed
    kcd_model.init_model()
    freemium_model.init_model()

