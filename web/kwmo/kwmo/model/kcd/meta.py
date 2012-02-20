# Model metadata and session

from sqlalchemy import MetaData
from sqlalchemy.orm import scoped_session, sessionmaker

# Create SQLAlchemy metadata instance (don't use the one that is automatically created
# in elixir - at least version 0.7.1 - because it doesn't work when using several metadatas).
metadata = MetaData()

# Create SQLAlchemy thread scoped session (don't use the one that is automatically created
# in elixir - at least version 0.7.1 - because it doesn't work when using several sessions).
Session = scoped_session(sessionmaker())

