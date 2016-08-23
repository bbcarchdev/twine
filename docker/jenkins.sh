#!/bin/bash -ex
DOCKER_REGISTRY="vm-10-100-0-25.ch.bbcarchdev.net"
PROJECT_NAME="twine"
INTEGRATION="docker/integration.yml"

# Remove previous test results (they're in gitignore so checkout doesn't remove them)
rm -f docker/cucumber/testresults.json

# Build the project
docker build --no-cache -t ${PROJECT_NAME} -f docker/Dockerfile-build ../

# If successfully built, tag and push to registry
if [ ! "${JENKINS_HOME}" = '' ]
then
	docker tag ${PROJECT_NAME} ${DOCKER_REGISTRY}/${PROJECT_NAME}
	docker push ${DOCKER_REGISTRY}/${PROJECT_NAME}
fi

if [ -f "${INTEGRATION}.default" ]
then
	# Copy the YML script and adjust the paths
	cp ${INTEGRATION}.default ${INTEGRATION}

	# Turn the local paths into absolute ones
	if [ ! "${JENKINS_HOME}" = '' ]
	then
		# Change "in-container" mount path to host mount path
                sed -i -e "s|- \./|- ${HOST_DATADIR}jobs/${JOB_NAME}/workspace/twine/docker/|" "${INTEGRATION}"
	fi

	# Tear down integration from previous run if it was still running
	docker-compose -p ${PROJECT_NAME} -f ${INTEGRATION} down

	# Build and run integration tester
	docker-compose -p ${PROJECT_NAME} -f ${INTEGRATION} run cucumber

	# Tear down integration
	docker-compose -p ${PROJECT_NAME} -f ${INTEGRATION} down
fi
