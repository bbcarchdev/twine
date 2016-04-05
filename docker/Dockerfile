FROM debian:wheezy
MAINTAINER Simeon van der Steen <simeon.vandersteen@bbc.co.uk>

# Add bbcarchdev apt repo
ENV DEBIAN_FRONTEND noninteractive
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 9E16A4D302DA096A && \
	echo "deb [arch=amd64] http://apt.bbcarchdev.net/debian/ wheezy main ports live stage dev" \
	>> /etc/apt/sources.list.d/bbcarchdev-wheezy.list

# Install Twine and everything else needed
# Note: we need twine-anansi-bridge here for a lib but it runs in its own container
RUN apt-get -y update && apt-get install -y --no-install-recommends \
	netcat netcat-traditional twine twine-processor-spindle twine-anansi-bridge \
	&& rm -rf /var/lib/apt/lists/*
	
# Overwrite the default config
COPY twine.conf /usr/etc/twine.conf

# Set an entry point for configuring
COPY run.sh /
ENTRYPOINT ["/run.sh"]

# Install the remote control
COPY remote.py /usr/local/bin/remote.py
EXPOSE 8000

# Launch supervisord
COPY supervisord.conf /etc/supervisor/supervisord.conf
CMD ["/usr/bin/supervisord"]
