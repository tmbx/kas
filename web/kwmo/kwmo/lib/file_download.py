from kfs_lib import *
from kwmo.lib.kwmo_kcd_client import KcdClient

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

# This class is a KFS file download generator.
# It yields dictionaries at the beginning of a file download, and data chunks after that.
# It allows downloading several files.
#
# Arguments:
#   ticket: kcd ticket for KFS
#   kfs_files:  list of kfs_file, which are dictionaries with those keys: inode_id, offset, commit_id
#
# Usage: 
#   kc = KcdClient(...)
#   file = None
#   for x in KFSDownload(ticket, kfs_files, kc, kc):
#       if type(x) == dict:
#           if file: 
#               # Close previous file.
#               file.close()
#               file = None
#           # Open new file.
#          file = open("some_file", "wb")
#       else:
#           # Write chunk to current file.
#           file.write(x)
#   if file:
#       # Close current file. 
#       file.close()
#       file = None
#
def kfs_download_generator(kcd_conf, workspace_id, share_id, user_id, kfs_files):
    kd = KFSDownload(kcd_conf, workspace_id, share_id, user_id, kfs_files)
    for x in kd:
        #if not x: break
        if type(x) == str:
            yield x

# Asynchronous KFS file download
class KFSDownload:
    STATE_IDLE = "state_idle"
    STATE_FILE_INFO = "state_file_info"
    STATE_CHUNKS = "state_chunks"

    def __init__(self, kcd_conf, workspace_id, share_id, user_id, kfs_files):
        # State variables.
        self.kfs_files = kfs_files
        self.download_idx = 0 
        self.state = KFSDownload.STATE_IDLE
        self.next_state = KFSDownload.STATE_FILE_INFO
        self.iteration_id = 0

        # Misc
        self.kfs_files = kfs_files

        # Instantiate a KCD client.
        self.kcd_client = KcdClient(kcd_conf)
   
        # Get download ticket.
        ticket = self.kcd_client.get_kcd_download_ticket(workspace_id, share_id, user_id)
        log.debug("Got a download ticket that contains '%i' bytes." % ( len(ticket) ))
 
        # Connect to KCD.
        self.kcd_client.connect()

        # Select KFS role.
        self.kcd_client.select_role(kanp.KANP_KCD_ROLE_FILE_XFER)
        log.debug("kfs_create_dir(): selected kfs role.")

        # Create a download request ANP message.
        m = kanp.ANP_msg()
        m.add_bin(ticket)
        m.add_u32(len(kfs_files))
        for kfs_file in kfs_files:
            m.add_u64(kfs_file['inode_id'])
            try: offset = kfs_file['offset']
            except Exception, e: offset = 0
            m.add_u64(offset)
            m.add_u64(kfs_file['commit_id'])

        # Send the request to KCD.
        payload = m.get_payload()
        self.kcd_client.send_command_header(kanp.KANP_CMD_KFS_DOWNLOAD_DATA, len(payload))
        self.kcd_client.write(payload)
        log.debug("Download: sent download request.")

    def __iter__(self):
        return self

    def next(self):
        self.iteration_id += 1

        #log.debug("KFSDownload.next(): iteration %i" % ( self.iteration_id ) )
        if self.download_idx == len(self.kfs_files):
            # Finished.
            #return None
            raise StopIteration

        if self.state == KFSDownload.STATE_IDLE:
            # Get the next ANP message.
            h, self.cur_message = kanp.get_anpt_all(self.kcd_client)
            #log.debug("Download: got a message type '%s'." % ( str(h.type) ))
            if h.type == kanp.KANP_RES_FAIL:
                raise kanp.KANPFailure(self.cur_message.get_u32(), self.cur_message.get_str())
            assert h.type == kanp.KANP_RES_KFS_DOWNLOAD_DATA
            log.debug("Download: got a download reply with a payload of size='%i' bytes." % \
                ( h.size ))

            # Get the number of sub-messages in the ANP message.
            self.nb_sub_messages = self.cur_message.get_u32()
            self.cur_sub_message = 0
            
            # State roumba
            self.state = self.next_state
            ###???###self.next_state = None
            ###???###if not self.state: self.state = KFSDownload.STATE_FILE_INFO

        if self.state == KFSDownload.STATE_FILE_INFO:
            # Sub-message contains file meta information.
            junk = self.cur_message.get_u32()
            assert junk == 4
            subm_type = self.cur_message.get_u32()
            assert subm_type == kanp.KANP_KFS_SUBMESSAGE_FILE
            self.total_size = self.cur_message.get_u64()
            sent_size = self.cur_message.get_u64()

            log.debug("Download: file submessage: size='%s', sent_size='%s'." % \
                ( str(self.total_size), str(sent_size) ))

            if sent_size > 0:
                # File chunk(s) available in next message(s).
                self.state = KFSDownload.STATE_CHUNKS
                self.bytes_read = 0
            else:
                # This is an empty file... jump to the next file.
                self.state = KFSDownload.STATE_FILE_INFO # no-op
                self.download_idx += 1
                log.debug("Download: file is empty.")

            # Return start of file.
            ret = {'size' : self.total_size, 'sent_size' : sent_size}

        else: ###self.state == KFSDownload.STATE_CHUNKS:
            # Sub-message is a file chunk.
            junk = self.cur_message.get_u32()
            assert junk == 3
            subm_type = self.cur_message.get_u32()
            assert subm_type == kanp.KANP_KFS_SUBMESSAGE_CHUNK
            log.debug("Download: got a file chunk submessage.")
            chunk_data = self.cur_message.get_bin()
            l = len(chunk_data)
            self.bytes_read += l

            if self.bytes_read == self.total_size:
                # File download is finished.. jump to next file if any.
                self.state = KFSDownload.STATE_FILE_INFO
                self.download_idx += 1
                log.debug("Download: this was the last chunk. File download is complete.")

            # Return chunk
            ret = chunk_data

        # Sub-message handled.
        self.cur_sub_message += 1

        if self.cur_sub_message == self.nb_sub_messages:
            self.next_state = self.state
            self.state = KFSDownload.STATE_IDLE

        return ret
 
