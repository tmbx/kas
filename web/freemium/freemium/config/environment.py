"""Pylons environment configuration"""
import os

from mako.lookup import TemplateLookup
from pylons import config
from sqlalchemy import engine_from_config

import freemium.lib.app_globals as app_globals
import freemium.lib.helpers
from freemium.config.routing import make_map
from kasmodel import RootConfigNode
from freemium.lib.config import cache_master_config
import freemium.model as model

def load_environment(global_conf, app_conf):
    """Configure the Pylons environment via the ``pylons.config``
    object
    """
    # Pylons paths
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    paths = dict(root=root,
                 controllers=os.path.join(root, 'controllers'),
                 static_files=os.path.join(root, 'public'),
                 templates=[os.path.join(root, 'templates')])

    # Initialize config with the basic options
    config.init_app(global_conf, app_conf, package='freemium', paths=paths)

    config['routes.map'] = make_map()
    config['pylons.app_globals'] = app_globals.Globals()
    config['pylons.h'] = freemium.lib.helpers

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
    model.init_model()
 
