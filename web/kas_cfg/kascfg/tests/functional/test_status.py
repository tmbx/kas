from kascfg.tests import *

class TestStatusController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='status', action='index'))
        # Test response...
