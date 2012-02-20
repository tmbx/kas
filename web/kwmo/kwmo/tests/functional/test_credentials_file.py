from kwmo.tests import *

class TestCredentialsFileController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='credentials_file', action='index'))
        # Test response...
