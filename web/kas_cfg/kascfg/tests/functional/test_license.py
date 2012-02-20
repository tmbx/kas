from kascfg.tests import *

class TestLicenseController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='license', action='index'))
        # Test response...
