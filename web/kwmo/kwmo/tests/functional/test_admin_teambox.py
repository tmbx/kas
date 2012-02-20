from kwmo.tests import *

class TestAdminTeamboxController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='admin_teambox', action='index'))
        # Test response...
