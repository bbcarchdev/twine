FROM acropolis-base
MAINTAINER Simeon van der Steen <simeon.vandersteen@bbc.co.uk>

ENV DEBIAN_FRONTEND noninteractive

# Install Twine-specific dependencies
RUN apt-get install -y --no-install-recommends \
	liburi libsparqlclient-dev libsql \
	libawsclient-dev libmq-dev \
	twine-processor-spindle

# Mount dir
COPY . /usr/local/src/

# Compile
RUN 	cd /usr/local/src && \
	autoreconf -i && \
	./configure --prefix=/usr --enable-debug && \
	make && \
	make install

# Adjust Twine config
RUN 	sed -i -e 's|update=|; update=|' /usr/etc/twine.conf && \
	sed -i -e 's|data=|; data=|' /usr/etc/twine.conf && \
	sed -i -e 's|query=|; query=|' /usr/etc/twine.conf && \
	echo '\n\
[plugins]\n\
module=rdf.so\n\
module=spindle.so\n\
\n\
[sparql]\n\
update=http://4store:8080/update/\n\
data=http://4store:8080/data/\n\
query=http://4store:8080/sparql/\n\
\n\
[spindle]\n\
graph=http://bbcarchdev.net/\n\
multigraph=yes\n\
bucket=spindle\n\
db=pgsql://postgres:postgres@postgres/postgres\n\
\n\
[s3]\n\
endpoint=s3:4569\n\
access=x\n\
secret=x' >> /usr/etc/twine.conf

CMD /bin/bash
