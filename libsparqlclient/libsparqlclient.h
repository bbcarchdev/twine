#ifndef LIBSPARQLCLIENT_H_
# define LIBSPARQLCLIENT_H_             1

# include <stddef.h>
# include <stdarg.h>
# include <librdf.h>

typedef struct sparql_connection_struct SPARQL;
typedef void (*sparql_logger_fn)(int priority, const char *format, va_list args);

SPARQL *sparql_create(void);
int sparql_destroy(SPARQL *connection);
int sparql_set_query_uri(SPARQL *connection, const char *uri);
int sparql_set_data_uri(SPARQL *connection, const char *uri);
int sparql_set_update_uri(SPARQL *connection, const char *uri);
int sparql_set_logger(SPARQL *connection, sparql_logger_fn logger);
int sparql_set_verbose(SPARQL *connection, int verbose);

int sparql_update(SPARQL *connection, const char *statement, size_t length);
int sparql_put(SPARQL *connection, const char *graph, const char *turtle, size_t length);

#endif /*!LIBSPARQLCLIENT_H_*/
