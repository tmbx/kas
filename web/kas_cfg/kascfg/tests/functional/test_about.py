from kascfg.tests import *

class TestAboutController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='about', action='index'))
        # Test response...
