#!/bin/bash
set -e

# Wait for postgres
until nc -z postgres 5432; do
    echo "$(date) - waiting for postgres..."
    sleep 2
done

# Wait for the twine cli
until nc -z twine-cli 8000; do
    echo "$(date) - waiting for twine-cli..."
    sleep 2
done

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

	# Settings for the cluster
    if [ "${CLUSTER}" = "true" ]; then
        # Set the parameters for a cluster
        sed -i -e "s|CLUSTER|cluster-name=twine\ncluster-verbose=yes\nnode-id=${HOSTNAME}\nenvironment=testing\nregistry=pgsql://postgres:postgres@postgres/spindle|" /usr/etc/twine.conf
    else
        sed -i -e "s|CLUSTER||" /usr/etc/twine.conf
    fi

	touch /init-done
fi

# Run the requested command
exec "$@"
