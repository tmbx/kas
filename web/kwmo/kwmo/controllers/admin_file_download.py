# Admin file download controller

from kwmo.controllers.file_download import FileDownloadController

class AdminFileDownloadController(FileDownloadController):

    # Do not send download notification to KCD.
    send_notification = False

    # Download a file.
    def download(self, workspace_id):
        return FileDownloadController.download(self, workspace_id)

