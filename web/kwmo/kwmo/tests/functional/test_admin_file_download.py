from kwmo.tests import *

class TestAdminFileDownloadController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='admin_file_download', action='index'))
        # Test response...
