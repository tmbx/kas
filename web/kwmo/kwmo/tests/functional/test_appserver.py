from kwmo.tests import *

class TestAppServer(TestController):
    def test_index(self):
        response = self.app.get('/')
        # Test response...
        assert '<span style="color:lime">Elixir DSL</span>' in response
