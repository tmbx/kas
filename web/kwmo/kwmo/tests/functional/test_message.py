from kwmo.tests import *

class TestMessageController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='message', action='index'))
        # Test response...
