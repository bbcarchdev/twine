#!/bin/bash
set -e

# Wait for postgres, if we're using spindle
if [ "${ENGINE}" = "spindle" ]; then
        until nc -z postgres 5432; do
            echo "$(date) - waiting for postgres..."
            sleep 2
        done
fi

# Adjust the configuration on first run
if [ ! -f /init-done ]; then
	echo "Initialising Twine.."

	# Host name
	sed -i -e "s|HOST_NAME|http://${HOST_NAME-acropolis.org.uk}/|" /usr/etc/twine.conf

	# 4store settings
	#sed -i -e "s|FOURSTORE_PORT_9000_TCP_ADDR|$FOURSTORE_PORT_9000_TCP_ADDR|g" /usr/etc/twine.conf
	#sed -i -e "s|FOURSTORE_PORT_9000_TCP_PORT|$FOURSTORE_PORT_9000_TCP_PORT|g" /usr/etc/twine.conf

	# s3 settings
	sed -i -e "s|S3_PORT_4569_TCP_ADDR|$S3_PORT_4569_TCP_ADDR|" /usr/etc/twine.conf
	sed -i -e "s|S3_ENV_ANANSI_BUCKET|$S3_ENV_ANANSI_BUCKET|" /usr/etc/twine.conf
	sed -i -e "s|S3_ENV_SPINDLE_BUCKET|$S3_ENV_SPINDLE_BUCKET|" /usr/etc/twine.conf
	sed -i -e "s|S3_ENV_ACCESS_KEY|${S3_ENV_ACCESS_KEY-x}|" /usr/etc/twine.conf
	sed -i -e "s|S3_ENV_SECRET_KEY|${S3_ENV_SECRET_KEY-x}|" /usr/etc/twine.conf

        if [ "${ENGINE}" = "spindle" ]; then
                # spindle workflow?
                sed -i -e "s|WORKFLOW_CLI|spindle-strip,sparql-get,spindle-correlate,sparql-put|" /usr/etc/twine.conf
                sed -i -e "s|WORKFLOW_WRITER|spindle-generate|" /usr/etc/twine.conf
                sed -i -e "s|PLUGINS|plugin=spindle-strip.so\nplugin=spindle-correlate.so\nplugin=spindle-generate.so\n|" /usr/etc/twine.conf
        else
                sed -i -e "s|WORKFLOW_CLI|sparql-get,sparql-put|" /usr/etc/twine.conf
                sed -i -e "s|WORKFLOW_WRITER|spindle-generate|" /usr/etc/twine.conf
                sed -i -e "s|PLUGINS||" /usr/etc/twine.conf
        fi

        if [ "${CLUSTER}" = "true" ]; then
                # activate a cluster
                sed -i -e "s|CLUSTER|cluster-name=twine\ncluster-verbose=yes\nnode-id=${HOSTNAME}\nenvironment=testing\nregistry=pgsql://postgres:postgres@postgres/spindle|" /usr/etc/twine.conf
        else
                sed -i -e "s|CLUSTER||" /usr/etc/twine.conf
        fi

        # Initialise the database, so that all depending containers can use Twine straight away
        twine -d -c /usr/etc/twine.conf -S

	touch /init-done
fi

# Run the requested command
exec "$@"
