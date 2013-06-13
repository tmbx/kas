from kodict import odict

# MAS service
class MASService(object):
    # Initialize object.
    def __init__(self):
        self.name = 'mas'
        self.enabled = False
        self.configured = False

    # Update object from configuration.
    def update_from_conf(self, conf):
        self.enabled = bool(conf.mas_service)
        self.configured = conf.is_mas_config_complete()

# WPS service
class WPSService(object):
    # Initialize object.
    def __init__(self):
        self.name = 'wps'
        self.enabled = False
        self.configured = False

    # Update object from configuration.
    def update_from_conf(self, conf):
        self.enabled = bool(conf.wps_service)
        self.configured = conf.is_wps_config_complete()

# TBSOS service
class TBSOSService(object):
    # Initialize object.
    def __init__(self):
        self.name = 'tbsos'
        self.enabled = False
        self.configured = False

    # Update object from configuration.
    def update_from_conf(self, conf):
        self.enabled = bool(conf.tbsos_service)
        self.configured = conf.is_tbxsos_config_complete()

# Freemium service
class FreemiumService(object):
    # Initialize object.
    def __init__(self):
        self.name = 'freemium'
        self.enabled = False
        self.configured = False

    # Update object from configuration.
    def update_from_conf(self, conf):
        self.enabled = bool(conf.freemium_service)
        self.configured = conf.is_freemium_config_complete()

# List of K2 services
class K2Services(odict):
    def __init__(self, *args, **kwargs):
        # Super
        odict.__init__(self, *args, **kwargs)

        # Initialize services.
        self['tbsos'] = TBSOSService()
        self['freemium'] = FreemiumService()

