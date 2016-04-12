#!/bin/bash -ex
DOCKER_REGISTRY="vm-10-100-0-25.ch.bbcarchdev.net"
PROJECT_NAME="twine"
INTEGRATION="docker/integration.yml"

# Build the project
docker build -t ${PROJECT_NAME} -f docker/Dockerfile-build .

# If successfully built, tag and push to registry
if [ ! "${JENKINS_HOME}" = '' ]
then
	docker tag -f ${PROJECT_NAME} ${DOCKER_REGISTRY}/${PROJECT_NAME}
	docker push ${DOCKER_REGISTRY}/${PROJECT_NAME}
fi

if [ -f "${INTEGRATION}.default" ]
then
	if [ ! -f "${INTEGRATION}" ]
	then
		cp ${INTEGRATION}.default ${INTEGRATION}
	fi

	# Tear down integration from previous run if it was still running
	# FIXME
	docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} stop
	docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} rm -f

	# Start project integration
	docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} up -d

	# Build and run integration tester
	# docker run --rm=true --link=${PROJECT_NAME//-}_${PROJECT_NAME}_1:${PROJECT_NAME} ${PROJECT_NAME}-test
	docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} run cucumber

	# Tear down integration
	# FIXME
	docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} stop
	docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} rm -f
fi
