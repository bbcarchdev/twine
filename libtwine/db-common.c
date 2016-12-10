/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
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

#include "p_libtwine.h"

static int twine_db_querylog_(SQL *restrict sql, const char *query);
static int twine_db_noticelog_(SQL *restrict sql, const char *notice);
static int twine_db_errorlog_(SQL *restrict sql, const char *sqlstate, const char *message);

/* Initialise a Twine database connection, if configured to use one */
int
twine_db_init_(TWINE *twine)
{
	char *t;

	t = twine_config_geta("twine:db", NULL);
	if(!t)
	{
		return 0;
	}
	twine->db = sql_connect(t);
	if(!twine->db)
	{
		twine_logf(LOG_CRIT, "failed to connect to database <%s>\n", t);
		free(t);
		return -1;
	}
	free(t);
	sql_set_querylog(twine->db, twine_db_querylog_);
	sql_set_errorlog(twine->db, twine_db_errorlog_);
	sql_set_noticelog(twine->db, twine_db_noticelog_);
	if(twine_db_schema_update_(twine))
	{
		return -1;
	}
	return 0;	
}

/* Clean up resources used by a Spindle database connection */
int
twine_db_cleanup(TWINE *twine)
{
	if(twine->db)
	{
		sql_disconnect(twine->db);
	}
	twine->db = NULL;
	return 0;
}

static int
twine_db_querylog_(SQL *restrict sql, const char *query)
{
	(void) sql;

	twine_logf(LOG_DEBUG, ": SQL: %s\n", query);
	return 0;
}

static int
twine_db_noticelog_(SQL *restrict sql, const char *notice)
{
	(void) sql;

	twine_logf(LOG_NOTICE, "%s\n", notice);
	return 0;
}

static int
twine_db_errorlog_(SQL *restrict sql, const char *sqlstate, const char *message)
{
	(void) sql;

	twine_logf(LOG_ERR, "[%s] %s\n", sqlstate, message);
	return 0;
}
