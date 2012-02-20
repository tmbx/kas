"""Routes configuration

The more specific and detailed routes should be defined first so they
may take precedent over the more generic routes. For more information
refer to the routes manual at http://routes.groovie.org/docs/
"""
from pylons import config
from routes import Mapper

def make_map():
    """Create, configure and return the routes Mapper"""
    map = Mapper(directory=config['pylons.paths']['controllers'],
                 always_scan=config['debug'])
    map.minimization = False

    # The ErrorController route (handles 404/500 error pages); it should
    # likely stay at the top, ensuring it can always be resolved
    map.connect('/error/{action}', controller='error')
    map.connect('/error/{action}/{id}', controller='error')

    # CUSTOM ROUTES HERE
    map.connect('/', controller='status', action='show')
    map.connect('login', '/login', controller='login', action='login')
    map.connect('logout', '/logout', controller='login', action='logout')
    map.connect('status', '/status', controller='status', action='show')
    map.connect('switch_to_prod', '/status/switch_to_prod', controller='status', action='switch_to_prod')
    map.connect('switch_to_maint', '/status/switch_to_maint', controller='status', action='switch_to_maint')
    map.connect('mas_config', '/config/mas', controller='mas_config', action='show')
    map.connect('wps_config', '/config/wps', controller='wps_config', action='show')
    map.connect('tbsos_config', '/config/tbsos', controller='tbsos_config', action='show')
    map.connect('tbsos_config_redirect', '/config/tbsos/redirect', controller='tbsos_config', action='redirect')
    map.connect('freemium_config', '/config/freemium', controller='freemium_config', action='show')
    map.connect('teambox', '/teambox', controller='about', action='show')
    map.connect('about', '/about', controller='about', action='show')
    map.connect('license', '/license', controller='license', action='show')
    map.connect('teamboxes', '/teamboxes', controller='teamboxes', action='show')
    map.connect('users', '/users', controller='user_management', action='show')
    map.connect('manage_users', '/users', controller='user_management', action='show')
    map.connect('manage_users_apply', '/users/apply', controller='user_management', 
        action='apply', conditions=dict(method=['POST']))
    map.connect('manage_users_add', '/users/add', controller='user_management',
        action='add', conditions=dict(method=['POST'])) 

    return map
    
