from kwmo.tests import *

class TestFileUploadController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='file_upload', action='index'))
        # Test response...
