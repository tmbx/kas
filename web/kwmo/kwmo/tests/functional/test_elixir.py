from kwmo.tests import *
from kwmo.tests import Session, metadata

class TestElixir(TestModel):
    def setUp(self):
        TestModel.setUp(self)

    def test_metadata(self):
        assert 'A collection of Tables and their associated schema constructs.' in metadata.__doc__

    def test_session(self):
        assert Session.connection().dialect.name is 'sqlite'
    
