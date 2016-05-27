# encoding: utf-8
"""
A simple remote control for Twine

Derived from http://stackoverflow.com/questions/17435261/python-3-x-simplehttprequesthandler-not-outputting-any-response
and https://reecon.wordpress.com/2014/04/02/simple-http-server-for-testing-get-and-post-requests-python/
"""
import sys

import http.server
from http.server import HTTPServer
from http.server import BaseHTTPRequestHandler

import subprocess
from subprocess import CalledProcessError
import json

import logging
logging.basicConfig(level=logging.DEBUG)

import select
import psycopg2
from psycopg2.extensions import ISOLATION_LEVEL_AUTOCOMMIT
DSN = 'host=postgres dbname=spindle user=postgres password=postgres'

REMOTE_DATA = '/tmp/remote-data.nq'

class Handler(BaseHTTPRequestHandler):
    def __init__(self, req, client_addr, server):
        BaseHTTPRequestHandler.__init__(self, req, client_addr, server)      
        
    def do_GET(self):
        '''
        Handle a GET
        '''
        logging.debug('Received a GET on {}'.format(self.path))
        response = {'message':''}

        if self.path == '/update':
            curs = self.server.db.cursor()
            curs.execute("UPDATE \"state\" SET \"status\" = 'DIRTY';")

            # Now wait for all updates to complete
            self._wait_for_ingest()
            response['message'] = 'Update completed'
        else:
            response['message'] = 'Twine remote control. Call /update to update the currently ingested data'

        self._reply_with(200, response)

    def do_POST(self):
        '''
        Handle a POST
        '''
        logging.debug('Received a POST on {}'.format(self.path))
        response = {'message':''}
        
        # Save the data in a temporary file
        length = int(self.headers['Content-Length'])
        data = self.rfile.read(length)
        with open(REMOTE_DATA, 'wb') as output:
            output.write(data)
        logging.debug('Wrote {} bytes to {}'.format(length, REMOTE_DATA))
        
        try:
            # Execute an ingest
            if self.path == '/ingest':
                args = 'twine -d -c /usr/etc/twine.conf {}'.format(REMOTE_DATA)
                logs = subprocess.check_output(args, 
                                               stderr=subprocess.STDOUT, 
                                               universal_newlines=True, shell=True)
                # Now wait for all updates to complete
                self._wait_for_ingest()
                response['logs'] = logs
                response['command'] = args
                response['message'] = 'Ingest completed'
                self._reply_with(200, response)
        except CalledProcessError as e:
            response['logs'] = e.output
            response['message'] = 'Error: {}'.format(e)
            self._reply_with(500, response)
            
        
    def _reply_with(self, code, data):
        '''
        Send an UTF-8 encoded JSON reply
        '''
        self.send_response(code)
        self.send_header("Content-type", "text/json;charset=utf-8")
        self.end_headers()
        self.wfile.write(json.dumps(data, indent=True).encode("utf-8"))
        self.wfile.flush()

    def _wait_for_ingest(self):
        '''
        Wait for ingest to complete
        '''
        curs = self.server.db.cursor()
        curs.execute("LISTEN ingest")

        logging.debug("Waiting for ingest done notification")
        while 1:
            if select.select([self.server.db],[],[])!=([],[],[]):
                self.server.db.poll()
                while self.server.db.notifies:
                    notify = self.server.db.notifies.pop(0) # Not using the data
                    logging.debug("Received notification")
                break;

class DbHTTPServer(HTTPServer):
    def __init__(self, address, handler):
        HTTPServer.__init__(self, address, handler)

        # Initiate Postgres connection
        self.db = psycopg2.connect(DSN)
        self.db.set_isolation_level(ISOLATION_LEVEL_AUTOCOMMIT)
        logging.debug("Database connection established")
        self._init_db_trigger()

    def close(self):
        self.db.close()

    def _init_db_trigger(self):
        '''
        Initiate Postgres trigger
        '''
        curs = self.db.cursor()
        curs.execute('''
            CREATE OR REPLACE FUNCTION notify_ingest_done()
              RETURNS trigger AS
            $BODY$
            BEGIN
             IF NOT EXISTS(SELECT 1 FROM "state" WHERE "status" = 'DIRTY') THEN
              NOTIFY ingest;
             END IF;
             RETURN NULL;
            END;
            $BODY$
            LANGUAGE plpgsql;
        ''')
        curs.execute('''
            DROP TRIGGER IF EXISTS state_changes ON state;
        ''')
        curs.execute('''
            CREATE TRIGGER state_changes
            AFTER UPDATE OF "status"
            ON state
            FOR EACH STATEMENT
            EXECUTE PROCEDURE notify_ingest_done();
        ''')
        logging.debug("Trigger created")

if __name__ == '__main__':
    # Start the server
    httpd = DbHTTPServer(('', 8000), Handler)
    try:
        logging.info("Server Started")
        httpd.serve_forever()
    except (KeyboardInterrupt, SystemExit):
        logging.info('Shutting down server')
        if httpd:
            httpd.close()
        
