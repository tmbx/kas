from kascfg.tests import *

class TestTbsosConfigController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='tbsos_config', action='index'))
        # Test response...
