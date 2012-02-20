from kwmo.tests import *

class TestChatController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='chat', action='index'))
        # Test response...
