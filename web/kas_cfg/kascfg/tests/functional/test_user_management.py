from kascfg.tests import *

class TestUserManagementController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='user_management', action='index'))
        # Test response...
