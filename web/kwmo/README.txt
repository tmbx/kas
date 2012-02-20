This file is for you to describe the kwmo application. Typically
you would include information such as the information below:

Installation and Setup
======================

Install ``kwmo`` using easy_install::

    easy_install kwmo

Make a config file as follows::

    paster make-config kwmo config.ini

Tweak the config file as appropriate and then setup the application::

    paster setup-app config.ini

Then you are ready to go.

Creating models
======================

To create a new model class, type::

    paster model mymodel

Once you have defined your model classes in mymodel, import them in kwmo/models/__init__.py::

    from mymodel import MyEntity

To create tables use create_sql::

    paster create_sql

To drop tables use drop_sql::

    paster drop_sql

Note that you must first import your classes into kwmo/models/__init__.py in order for the database commands to work !

