# Kas configuration library

KPLATPATH='/usr/bin/kplatshell'

import os, logging, threading

log = logging.getLogger(__name__)

# from kpython
from kbase import PropStore
from krun import get_cmd_output

# from teambox-console-setup
from kasmodel import RootConfigNode
from kasmodeltool import Dumper

# from kas-python
from kcd_lib import get_kcd_external_conf_object

# Default master file path
default_master_file_path = RootConfigNode.master_file_path

# Configuration cache lock
_lock = threading.Lock()

# Last configuration version
_last_config_cache_version = None

# Configuration cache
_config_cache = None

# Return master configuration.
def load_master_config(path=default_master_file_path):
    # Get the configuration model.
    root_node = RootConfigNode()

    # Load configuration.
    root_node.load_master_config(path=path)

    return root_node

# Cache master config.
def cache_master_config(callback=None, path=default_master_file_path):
    global _config_cache, _last_config_cache_version

    _lock.acquire()
    try:
        # Get current configuration version.
        current_config_version = get_current_config_version(path=path)

        # Update cached configuration.
        _config_cache = load_master_config(path=path)

        # Update last configuration version.
        _last_config_cache_version = current_config_version

        # Run callback, if any.
        if callback: callback()

    finally:
        _lock.release()

# Get master config.
def get_cached_master_config():
    if not _config_cache:
        raise Exception("Configuration is not cached... you might want to use cache_master_config() first.")
    return _config_cache

# Get last modification date of configuration (float).
def get_current_config_version(path=default_master_file_path):
    if not os.path.exists(path): return 1
    return os.stat(path).st_mtime

# Check if configuration file has changed. If so, reload cache and run the callback.
def detect_cached_config_change(callback, path=default_master_file_path):
    # Get current config version.
    current_config_version = get_current_config_version(path=path)

    # Get current config, if needed. 
    if _last_config_cache_version != current_config_version:
        cache_master_config(callback=callback, path=path)

# Get local database url.
def get_local_db_url():
    url = 'postgres:///kwmo?host=/var/run/postgresql/'

    return url

# Get KCD database url.
def get_kcd_db_url():
    if _config_cache.kcd_host != None and _config_cache.kcd_host != '':
        kcd_host = _config_cache.kcd_host
        kcd_db_user = 'external'
        kcd_db_pwd = _config_cache.kcd_pwd
        if not kcd_db_pwd: kcd_db_pwd = _config_cache.admin_pwd

        url = 'postgres://%s:%s@%s:5432/kcd' % ( kcd_db_user, kcd_db_pwd, kcd_host )

    else:
        url = 'postgres:///kcd?host=/var/run/postgresql/'

    return url

# Get cached kcd external config object.
def get_cached_kcd_external_conf_object():
    if not _config_cache:
        raise Exception("Configuration is not cached... you might want to use cache_master_config()" +\
                        " in config/environment.py.")

    return get_kcd_external_conf_object(master_config=_config_cache)

