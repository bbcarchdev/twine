#!/bin/bash

# Wait for postgres
until nc -z $POSTGRES_PORT_5432_TCP_ADDR $POSTGRES_PORT_5432_TCP_PORT; do
    echo "$(date) - waiting for postgres..."
    sleep 2
done

# Adjust the configuration on first run
if [ ! -f /init-done ]; then
	echo "Initialising Twine.."

	# Host name
	sed -i -e "s|HOST_NAME|http://${HOST_NAME-acropolis.org.uk}/|" /usr/etc/twine.conf
	
	# Postgres settings
	sed -i -e "s|POSTGRES_ENV_POSTGRES_PASSWORD|$POSTGRES_ENV_POSTGRES_PASSWORD|g" /usr/etc/twine.conf
	sed -i -e "s|POSTGRES_PORT_5432_TCP_ADDR|$POSTGRES_PORT_5432_TCP_ADDR|g" /usr/etc/twine.conf
	sed -i -e "s|POSTGRES_PORT_5432_TCP_PORT|$POSTGRES_PORT_5432_TCP_PORT|g" /usr/etc/twine.conf
	
	# 4store settings
	sed -i -e "s|FOURSTORE_PORT_9000_TCP_ADDR|$FOURSTORE_PORT_9000_TCP_ADDR|g" /usr/etc/twine.conf
	sed -i -e "s|FOURSTORE_PORT_9000_TCP_PORT|$FOURSTORE_PORT_9000_TCP_PORT|g" /usr/etc/twine.conf
	
	# s3 settings
	sed -i -e "s|S3_PORT_4569_TCP_ADDR|$S3_PORT_4569_TCP_ADDR|" /usr/etc/twine.conf
	sed -i -e "s|S3_ENV_ANANSI_BUCKET|$S3_ENV_ANANSI_BUCKET|" /usr/etc/twine.conf
	sed -i -e "s|S3_ENV_SPINDLE_BUCKET|$S3_ENV_SPINDLE_BUCKET|" /usr/etc/twine.conf
	sed -i -e "s|S3_ENV_ACCESS_KEY|${S3_ENV_ACCESS_KEY-x}|" /usr/etc/twine.conf
	sed -i -e "s|S3_ENV_SECRET_KEY|${S3_ENV_SECRET_KEY-x}|" /usr/etc/twine.conf

	touch /init-done
fi

# Run the requested command
exec "$@"
