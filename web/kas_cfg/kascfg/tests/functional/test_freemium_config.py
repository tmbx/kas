from kascfg.tests import *

class TestFreemiumConfigController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='freemium_config', action='index'))
        # Test response...
