from kwmo.tests import *

class TestTeamboxController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='teambox', action='index'))
        # Test response...
