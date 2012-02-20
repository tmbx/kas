from kwmo.tests import *

class TestInvitationController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='invitation', action='index'))
        # Test response...
