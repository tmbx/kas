"""Pylons environment configuration"""
import os

from mako.lookup import TemplateLookup
from pylons import config
from kasmodel import RootConfigNode
from kwmo.lib.config import cache_master_config
import kwmo.lib.app_globals as app_globals
import kwmo.lib.helpers
from kwmo.config.routing import make_map
import kwmo.model as model
import kwmo.model.kcd as kcd_model
import kcd_lib

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
    config.init_app(global_conf, app_conf, package='kwmo', paths=paths)

    config['routes.map'] = make_map()
    config['pylons.app_globals'] = app_globals.Globals()
    config['pylons.h'] = kwmo.lib.helpers

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
    else:
        RootConfigNode.master_file_path = config['master_file_path']
  
    # Cache master config.
    cache_master_config(path=config['master_file_path'])
 
    # Initialize models.
    kcd_model.init_model() # dependency - load before main model
    model.init_model()

