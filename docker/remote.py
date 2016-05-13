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

REMOTE_DATA = '/tmp/remote-data.nq'

class Handler(BaseHTTPRequestHandler):
    def __init__(self, req, client_addr, server):
        BaseHTTPRequestHandler.__init__(self, req, client_addr, server)      
        
    def do_GET(self):
        '''
        Handle a GET
        '''
        self._reply_with({'message':'Twine remote control'})

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

if __name__ == '__main__':
    # Start the server
    httpd = HTTPServer(('', 8000), Handler)
    try:
        logging.info("Server Started")
        httpd.serve_forever()
    except (KeyboardInterrupt, SystemExit):
        logging.info('Shutting down server')
        if httpd:
            httpd.close()
        
