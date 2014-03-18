/* Twine: Daemonize
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libutils.h"

pid_t
utils_daemon(const char *configkey, const char *pidfile)
{
	pid_t child;	
	char *file;
	FILE *f;
	int fd;

	utils_is_daemon = 1;
	if(configkey)
	{
		file = config_geta(configkey, pidfile);
	}
	else if(pidfile)
	{
		file = strdup(pidfile);
		if(!file)
		{
			log_printf(LOG_CRIT, "failed to allocate memory: %s\n", strerror(errno));
			return -1;
		}
	}
	else
	{
		file = NULL;
	}
	child = fork();
	if(child == -1)
	{
		log_printf(LOG_CRIT, "failed to fork child process: %s\n", strerror(errno));
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
				log_printf(LOG_CRIT, "failed to open PID file %s: %s\n", file, strerror(errno));
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
	log_reset();
	if(setsid() < 0)
	{
		log_printf(LOG_CRIT, "failed to create new process group: %s\n", strerror(errno));
		return -1;
	}
	if(chdir("/"))
	{
		log_printf(LOG_CRIT, "failed to change working directory: %s\n", strerror(errno));
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
		log_printf(LOG_CRIT, "failed to open /dev/null: %s\n", strerror(errno));
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
