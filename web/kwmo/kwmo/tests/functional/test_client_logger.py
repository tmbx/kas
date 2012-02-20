from kwmo.tests import *

class TestClientLoggerController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='client_logger', action='index'))
        # Test response...
