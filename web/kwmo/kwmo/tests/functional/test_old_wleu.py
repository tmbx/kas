from kwmo.tests import *

class TestOldWleuController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='old_wleu', action='index'))
        # Test response...
