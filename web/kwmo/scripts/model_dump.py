#!/usr/bin/env python

# Dump a generated schema of the KWMO database.
# This script need to be connected to the database, even though no changes will be done.

# WARNING: Make sure the database you set in the config file does not contain important informations, in case 
# the method overriding fails (see the execute function).
# WARNING 2: Make sure to dump with the same database engine that is used in production (postgres).

import sys, os, elixir, re
from paste.deploy import appconfig
from kwmo.config.environment import load_environment
from kwmo.model.meta import metadata
from sqlalchemy.engine.base import SchemaIterator

# from kpython
from kfile import first_existing_file

# Paster configuration file.
paster_config_file = first_existing_file([
    'development.ini'])

# Print SQL queries that would normally be executed by sqlalchemy
# (by overriding the SchemaIterator execute method - is this way safe?
#  SQLAlchemy does not provide a way to do this and we don't want 
#  to modify this package).
def execute(self):
    try:
        # Get lines in buffer.
        lines = self.buffer.getvalue().split('\n')

        # Strip empty lines.
        lines = filter(lambda x: bool(x), lines)

        # print query.
        query = '\n'.join(lines) + ';'
        print query
        print

        return True
    finally:
        self.buffer.truncate(0)
SchemaIterator.execute = execute

# Main function 
def main():
    global paster_config_file

    # Get config file from arguments, if specified.
    if len(sys.argv) > 1:
        paster_config_file = sys.argv[1]
    if not paster_config_file:
        print "Please provide a configuration file."
        sys.exit(1)

    # Load paster config and environment.
    conf = appconfig('config:' + os.path.join(os.getcwd(), paster_config_file))
    load_environment(conf.global_conf, conf.local_conf)

    # Dump the second matadata (kwmo).
    print "# Auto-generated dump of kwmo models by %s" % ( os.path.basename(sys.argv[0]) )
    engine = metadata.bind
    engine.create(metadata)
    print "# End of auto-generated dump"

if __name__ == "__main__":
    main()

