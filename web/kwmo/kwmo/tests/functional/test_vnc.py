from kwmo.tests import *

class TestVncController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='vnc', action='index'))
        # Test response...
