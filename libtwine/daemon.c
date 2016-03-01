/* Twine: Daemonize a process
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_TWINE_CONFIG_H
# include "config.h"
#endif

#include "p_libtwine.h"

pid_t
twine_daemonize(TWINE *context, const char *default_pidfile)
{
	pid_t child;	
	char *file;
	FILE *f;
	int fd;

	context->is_daemon = 1;
	file = twine_config_geta("*:pidfile", default_pidfile);
	child = fork();
	if(child == -1)
	{
		twine_logf(LOG_CRIT, "failed to fork child process: %s\n", strerror(errno));
		free(file);
		return -1;
	}
	if(child > 0)
	{
		/* Parent process */
		if(file)
		{
			f = fopen(file, "w");
			if(!f)
			{
				twine_logf(LOG_CRIT, "failed to open PID file %s: %s\n", file, strerror(errno));
				return child;
			}
			fprintf(f, "%ld\n", (long int) child);
			fclose(f);
		}
		return child;
	}
	/* Child process */
	free(file);
	umask(0);
	log_reset()
	if(setsid() < 0)
	{
		twine_logf(LOG_CRIT, "failed to create new process group: %s\n", strerror(errno));
		return -1;
	}
	if(chdir("/"))
	{
		twine_logf(LOG_CRIT, "failed to change working directory: %s\n", strerror(errno));
		return -1;
	}
    close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	do
	{
		fd = open("/dev/null", O_RDWR);
	}
	while(fd == -1 && errno == EINTR);
	if(fd == -1)
	{
		twine_logf(LOG_CRIT, "failed to open /dev/null: %s\n", strerror(errno));
		return -1;
	}
	if(fd != 0)
	{
		dup2(fd, 0);
	}
	if(fd != 1)
	{
		dup2(fd, 1);
	}
	if(fd != 2)
	{
		dup2(fd, 2);
	}
	return 0;
}
