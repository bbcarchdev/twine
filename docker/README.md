For development, make sure the spindle folder exists in the parent dir of twine, and then run:
`docker-compose -f docker/integration.yml.default -f docker/development.yml.default run cucumber`
from the twine directory.

For just testing images, run:
`docker-compose -f docker/integration.yml.default run cucumber`
from the twine directory.
