#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libtwine.h"

/* The current version of the database schema: each schema version number in
 * 1..DB_SCHEMA_VERSION must be handled individually in twine_db_migrate_
 * below.
 */
#define DB_SCHEMA_VERSION  1

static int twine_db_migrate_(SQL *restrict, const char *identifier, int newversion, void *restrict userdata);

int
twine_db_schema_update_(TWINE *twine)
{
	if(sql_migrate(twine->db, "com.github.bbcarchdev.twine", twine_db_migrate_, NULL))
	{
		twine_logf(LOG_CRIT, "failed to update database schema to latest version\n");
		return -1;
	}
	return 0;
}

static int
twine_db_migrate_(SQL *restrict sql, const char *identifier, int newversion, void *restrict userdata)
{
	SQL_LANG lang;
	SQL_VARIANT variant;

	(void) identifier;
	(void) userdata;

	lang = sql_lang(sql);
	variant = sql_variant(sql);
	if(lang != SQL_LANG_SQL)
	{
		twine_logf(LOG_CRIT, ": only SQL databases can be used as back-ends for Twine\n");
		return -1;
	}
	if(variant != SQL_VARIANT_POSTGRES)
	{
		twine_logf(LOG_CRIT, ": only PostgreSQL databases can be used as back-ends for Twine\n");
		return -1;
	}
	if(newversion == 0)
	{
		/* Return target version */
		return DB_SCHEMA_VERSION;
	}
	twine_logf(LOG_NOTICE, ": updating database schema to version %d\n", newversion);
	if(newversion == 1)
	{
		if(sql_execute(sql, "CREATE TABLE \"subject_objects\" ("
					   "\"graph\" text NOT NULL,"
					   "\"subjects\" text[],"
  				       "\"objects\" text[],"
					   "PRIMARY KEY (\"graph\")"
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"subject_objects_subjects\" ON \"subject_objects\" USING hash (\"subjects\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"subject_objects_objects\" ON \"subject_objects\" USING hash (\"objects\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE TABLE \"target_media\" ("
					   "\"graph\" text NOT NULL,"
					   "\"subject\" text NOT NULL,"
	    			   "\"objects\" text[],"
					   "PRIMARY KEY (\"graph\", \"subject\")"
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"target_media_graph\" ON \"target_media\" USING hash (\"graph\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"target_media_subject\" ON \"target_media\" USING hash (\"subject\")"))
		{
			return -1;
		}
		return 0;		
	}
	twine_logf(LOG_NOTICE, ": unsupported database schema version %d\n", newversion);
	return -1;
}
