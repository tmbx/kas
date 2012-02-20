from kwmo.tests import *

class TestSkurlTeamboxController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='skurl_teambox', action='index'))
        # Test response...
