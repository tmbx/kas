from kwmo.tests import *

class TestTeamboxSettingsController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='teambox_settings', action='index'))
        # Test response...
