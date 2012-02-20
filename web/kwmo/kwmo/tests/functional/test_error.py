from kwmo.tests import *

class TestErrorController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='error', action='index'))
        # Test response...
