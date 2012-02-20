from kwmo.tests import *

class TestAbstractTeamboxController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='abstract_teambox', action='index'))
        # Test response...
