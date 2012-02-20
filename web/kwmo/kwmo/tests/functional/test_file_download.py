from kwmo.tests import *

class TestFileDownloadController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='file_download', action='index'))
        # Test response...
