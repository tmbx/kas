from kwmo.tests import *

class TestOldDispatcherController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='old_dispatcher', action='index'))
        # Test response...
