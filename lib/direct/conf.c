/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#include <direct/conf.h>
#include <direct/list.h>
#include <direct/log.h>
#include <direct/map.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/system.h>
#include <direct/util.h>

D_DEBUG_DOMAIN( Direct_Config, "Direct/Config", "Direct Runtime Configuration options" );

/**********************************************************************************************************************/

static DirectConfig conf = { 0 };

DirectConfig *direct_config = &conf;
const char   *direct_config_usage =
     "libdirect options:\n"
     "  disable-module=<module_name>   Suppress loading of this module\n"
     "  module-dir=<directory>         Override default module search directory\n"
     "  memcpy=<method>                Skip memcpy() probing (help = show list)\n"
     "  [no-]quiet                     Disable text output except debug/oom/ooshm messages or direct logs\n"
     "  [no-]quiet=<type>              Only quiet certain message types (cumulative with 'quiet')\n"
     "                                 [ info | warning | error | once | unimplemented | banner | bug ]\n"
     "  [no-]fatal-messages            Enable trap for all message types except banner and info\n"
     "  [no-]fatal-messages=<type>     Enable trap for certain message types (cumulative with 'fatal-messages')\n"
     "                                 [ warning | error | once | unimplemented | bug | oom | ooshm ]\n"
     "  [no-]debug=<domain>            Configure debug messages domain (if no debug level is specified, default = 8)\n"
     "  debug-level=<level>            Set global debug messages level used by domains (default = 0 for no debug)\n"
     "                                 Overload 'log-level', the maximum debug messages level is 9\n"
     "  log=<domain>                   Configure direct logs domain (if no log level is specified, default = 6)\n"
     "  log-level=<level>              Set global direct logs level used by domains (default = 6 for verbose)\n"
     "                                 [ 0: none, 1: fatal, 2: error, 3: warning, 4: notice, 5: info, 6: verbose ]\n"
     "                                 Overload 'debug-level'\n"
     "  log-all                        Enable all debug messages and direct logs output\n"
     "  log-none                       Disable all debug messages and direct logs output\n"
     "  [no-]debugmem                  Enable memory allocation tracking\n"
     "  [no-]trace                     Enable stack trace support\n"
     "  [no-]nm-for-trace              Enable running nm in a child process to retrieve symbols\n"
     "  log-file=<name>                Write all messages to the specified file\n"
     "  log-udp=<host>:<port>          Send all messages via UDP to the specified host and port\n"
     "  fatal-level=<fatal_name>       Abort on NONE, ASSERT (default) or ASSUME (incl. ASSERT)\n"
     "  [no-]sighandler                Enable signal handling (default enabled)\n"
     "  [no-]sighandler-thread         Enable signal handler thread (default enabled)\n"
     "  dont-catch=<num>[,<num>...]    Don't catch these signals\n"
     "  [no-]thread-block-signals      Block all signals in new threads (default enabled)\n"
     "  thread-priority-scale=<100th>  Apply scaling factor on thread type based priorities\n"
     "  thread-priority=<priority>     Set priority for the default thread type (default = 100)\n"
     "  thread-scheduler=<policy>      Select thread scheduler (default = other)\n"
     "  thread-stacksize=<stacksize>   Set thread stack size (default = auto)\n"
     "  default-interface-implementation=<type/name>\n"
     "                                 Probe interface_type/implementation_name first\n"
     "  log-delay-rand-loops=<loops>   Add random loops (of max loops) to central logging code for testing purpose\n"
     "  log-delay-rand-us=<us>         Add random sleep (of max us) to central logging code for testing purpose\n"
     "  log-delay-min-loops=<loops>    Set minimum busy loops after each log message\n"
     "  log-delay-min-us=<us>          Set minimum sleep after each log message\n"
     "  delay-trap-ms=<ms>             Set period to wait instead of raising\n"
     "\n";

static DirectMap               *config_options = NULL;
static bool                     config_option_compare( DirectMap *map, const void *key, void *object, void *ctx );
static unsigned int             config_option_hash   ( DirectMap *map, const void *key, void *ctx );
static DirectEnumerationResult  config_option_free   ( DirectMap *map, void *object, void *ctx );

/**********************************************************************************************************************/

void
__D_conf_init()
{
     direct_map_create( 123, config_option_compare, config_option_hash, NULL, &config_options );

     direct_config->log_level             = DIRECT_LOG_DEBUG_0;
     direct_config->trace                 = true;
     direct_config->fatal                 = DCFL_ASSERT;
     direct_config->sighandler            = true;
     direct_config->sighandler_thread     = true;
     direct_config->thread_block_signals  = true;
     direct_config->thread_priority_scale = 100;

     char *args = direct_getenv( "D_ARGS" );

     if (args) {
          args = D_STRDUP( args );

          char *p = NULL, *r, *s = args;

          while ((r = direct_strtok_r( s, ",", &p ))) {
               char *v;

               direct_trim( &r );

               v = strchr( r, '=' );
               if (v)
                    *v++ = 0;

               direct_config_set( r, v );

               s = NULL;
          }

          D_FREE( args );
     }
}

void
__D_conf_deinit()
{
     if (config_options) {
          direct_map_iterate( config_options, config_option_free, NULL );
          direct_map_destroy( config_options );
          config_options = NULL;
     }
}

/**********************************************************************************************************************/

#define OPTION_NAME_LENGTH 128

typedef struct {
     char        name[OPTION_NAME_LENGTH];
     DirectLink *values;
} ConfigOption;

typedef struct {
     DirectLink  link;
     char       *value;
} ConfigOptionValue;

static void
config_option_value_add( ConfigOption *option,
                         const char   *name )
{
     ConfigOptionValue *value;

     if (!name)
          return;

     value = D_CALLOC( 1, sizeof(ConfigOptionValue) + strlen( name ) + 1 );
     if (!value) {
          D_OOM();
          return;
     }

     value->value = direct_snputs( (char*) (value + 1), name, OPTION_NAME_LENGTH );

     direct_list_append( &option->values, &value->link );
}

static ConfigOption*
config_option_create( const char *name,
                      const char *value )
{
     ConfigOption *option;

     option = D_CALLOC( 1, sizeof(ConfigOption) );
     if (!option) {
          D_OOM();
          return NULL;
     }

     direct_snputs( option->name, name, OPTION_NAME_LENGTH );

     config_option_value_add( option, value );

     direct_map_insert( config_options, name, option );

     return option;
}

static bool
config_option_compare( DirectMap  *map,
                       const void *key,
                       void       *object,
                       void       *ctx )
{
     const char   *map_key   = key;
     ConfigOption *map_entry = object;

     return strcmp( map_key, map_entry->name ) == 0;
}

static unsigned int
config_option_hash( DirectMap  *map,
                    const void *key,
                    void       *ctx )
{
     size_t        i       = 0;
     unsigned int  hash    = 0;
     const char   *map_key = key;

     while (map_key[i]) {
          hash = hash * 131 + map_key[i];

          i++;
     }

     return hash;
}

static DirectEnumerationResult
config_option_free( DirectMap *map,
                    void      *object,
                    void      *ctx )
{
     ConfigOption      *option = object;
     ConfigOptionValue *value, *next;

     direct_list_foreach_safe (value, next, option->values) {
          D_FREE( value );
     }

     D_FREE( option );

     return DENUM_OK;
}

/**********************************************************************************************************************/

DirectResult
direct_config_set( const char *name,
                   const char *value )
{
     if (strcmp( name, "disable-module" ) == 0) {
          if (value) {
               int n = 0;

               while (direct_config->disable_module &&
                      direct_config->disable_module[n])
                    n++;

               direct_config->disable_module = D_REALLOC( direct_config->disable_module, sizeof(char*) * (n + 2) );

               direct_config->disable_module[n] = D_STRDUP( value );
               direct_config->disable_module[n+1] = NULL;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No module name specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "module-dir" ) == 0) {
          if (value) {
               if (direct_config->module_dir)
                    D_FREE( direct_config->module_dir );
               direct_config->module_dir = D_STRDUP( value );
          }
          else {
               D_ERROR( "Direct/Config: '%s': No directory name specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "memcpy" ) == 0) {
          if (value) {
               if (direct_config->memcpy)
                    D_FREE( direct_config->memcpy );
               direct_config->memcpy = D_STRDUP( value );
          }
          else {
               D_ERROR( "Direct/Config: '%s': No method specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "quiet" ) == 0 || strcmp( name, "no-quiet" ) == 0) {
          /* Enable/disable all at once by default. */
          DirectMessageType type = DMT_ALL;

          /* Find out the specific message type being configured. */
          if (value) {
               if (!strcmp( value, "info" ))          type = DMT_INFO;          else
               if (!strcmp( value, "warning" ))       type = DMT_WARNING;       else
               if (!strcmp( value, "error" ))         type = DMT_ERROR;         else
               if (!strcmp( value, "once" ))          type = DMT_ONCE;          else
               if (!strcmp( value, "unimplemented" )) type = DMT_UNIMPLEMENTED; else
               if (!strcmp( value, "banner" ))        type = DMT_BANNER;        else
               if (!strcmp( value, "bug" ))           type = DMT_BUG;
               else {
                    D_ERROR( "Direct/Config: '%s': Unknown message type '%s'!\n", name, value );
                    return DR_INVARG;
               }
          }

          /* Set/clear the corresponding flag in the configuration. */
          if (name[0] == 'q')
               D_FLAGS_SET( direct_config->quiet, type );
          else
               D_FLAGS_CLEAR( direct_config->quiet, type );
     } else
     if (strcmp( name, "fatal-messages" ) == 0 || strcmp( name, "no-fatal-messages" ) == 0) {
          /* Enable/disable all at once by default. */
          DirectMessageType type = DMT_ALL & ~(DMT_BANNER | DMT_INFO);

          /* Find out the specific message type being configured. */
          if (value) {
               if (!strcmp( value, "warning" ))        type = DMT_WARNING;       else
               if (!strcmp( value, "error" ))          type = DMT_ERROR;         else
               if (!strcmp( value, "once" ))           type = DMT_ONCE;          else
               if (!strcmp( value, "unimplemented" ))  type = DMT_UNIMPLEMENTED; else
               if (!strcmp( value, "bug" ))            type = DMT_BUG;           else
               if (!strcmp( value, "oom" ))            type = DMT_OOM;           else
               if (!strcmp( value, "ooshm" ))          type = DMT_OOSHM;
               else {
                    D_ERROR( "Direct/Config: '%s': Unknown message type '%s'!\n", name, value );
                    return DR_INVARG;
               }
          }

          /* Set/clear the corresponding flag in the configuration. */
          if (name[0] == 'f')
               D_FLAGS_SET( direct_config->fatal_messages, type );
          else
               D_FLAGS_CLEAR( direct_config->fatal_messages, type );
     } else
     if (strcmp( name, "no-fatal-messages" ) == 0) {
          direct_config->fatal_messages = DMT_NONE;
     } else
     if (strcmp( name, "debug" ) == 0) {
          if (value) {
               DirectLogDomainConfig config = { 0 };

               if (value[0] && value[1] == ':') {
                    config.level = value[0] - '0' + DIRECT_LOG_DEBUG_0;

                    value += 2;
               }
               else
                    config.level = DIRECT_LOG_DEBUG;

               direct_log_domain_configure( value, &config );
          }
          else {
               D_ERROR( "Direct/Config: '%s': No domain specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "no-debug" ) == 0) {
          if (value) {
               DirectLogDomainConfig config = { 0 };

               config.level = DIRECT_LOG_DEBUG_0;

               direct_log_domain_configure( value, &config );
          }
          else {
               D_ERROR( "Direct/Config: '%s': No domain specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "debug-level" ) == 0) {
          if (value) {
               char level;

               if (sscanf( value, "%c", &level ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->log_level = level - '0' + DIRECT_LOG_DEBUG_0;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "log" ) == 0) {
          if (value) {
               DirectLogDomainConfig config = { 0 };

               if (value[0] && value[1] == ':') {
                    config.level = value[0] - '0';

                    value += 2;
               }
               else
                    config.level = DIRECT_LOG_VERBOSE;

               direct_log_domain_configure( value, &config );
          }
          else {
               D_ERROR( "Direct/Config: '%s': No domain specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "log-level" ) == 0) {
          if (value) {
               char level;

               if (sscanf( value, "%c", &level ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->log_level = level - '0';
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "log-all" ) == 0) {
          direct_config->log_all = true;
     } else
     if (strcmp( name, "log-none" ) == 0) {
          direct_config->log_none = true;
     } else
     if (strcmp( name, "debugmem" ) == 0) {
          direct_config->debugmem = true;
     } else
     if (strcmp( name, "no-debugmem" ) == 0) {
          direct_config->debugmem = false;
     } else
     if (strcmp( name, "trace" ) == 0) {
          direct_config->trace = true;
     } else
     if (strcmp( name, "no-trace" ) == 0) {
          direct_config->trace = false;
     } else
     if (strcmp( name, "nm-for-trace" ) == 0) {
          direct_config->nm_for_trace = true;
     } else
     if (strcmp( name, "no-nm-for-trace" ) == 0) {
          direct_config->nm_for_trace = false;
     } else
     if (strcmp( name, "log-file" ) == 0 || strcmp( name, "log-udp" ) == 0) {
          if (value) {
               DirectResult  ret;
               DirectLog    *log;

               ret = direct_log_create( strcmp( name, "log-udp" ) ? DLT_FILE : DLT_UDP, value, &log );
               if (ret)
                    return ret;

               if (direct_config->log)
                    direct_log_destroy( direct_config->log );

               direct_config->log = log;

               direct_log_set_default( log );
          }
          else {
               if (strcmp( name, "log-udp" ))
                    D_ERROR( "Direct/Config: '%s': No file name specified!\n", name );
               else
                    D_ERROR( "Direct/Config: '%s': No host and port specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "fatal-level" ) == 0) {
          if (strcasecmp( value, "none" ) == 0) {
               direct_config->fatal = DCFL_NONE;
          }
          else
               if (strcasecmp( value, "assert" ) == 0) {
               direct_config->fatal = DCFL_ASSERT;
          }
          else
               if (strcasecmp( value, "assume" ) == 0) {
               direct_config->fatal = DCFL_ASSUME;
          }
          else {
               D_ERROR( "Direct/Config: '%s': Unknown level specified (use 'none', 'assert', 'assume')!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "sighandler" ) == 0) {
          direct_config->sighandler = true;
     } else
     if (strcmp( name, "no-sighandler" ) == 0) {
          direct_config->sighandler = false;
     } else
     if (strcmp( name, "sighandler-thread" ) == 0) {
          direct_config->sighandler_thread = true;
     } else
     if (strcmp( name, "no-sighandler-thread" ) == 0) {
          direct_config->sighandler_thread = false;
     } else
     if (strcmp( name, "dont-catch" ) == 0) {
          if (value) {
               char *signals   = D_STRDUP( value );
               char *p = NULL, *r, *s = signals;

               while ((r = direct_strtok_r( s, ",", &p ))) {
                    char          *error;
                    unsigned long  signum;

                    direct_trim( &r );

                    signum = strtoul( r, &error, 10 );

                    if (*error) {
                         D_ERROR( "Direct/Config: '%s': Error in number at '%s'!\n", name, error );
                         D_FREE( signals );
                         return DR_INVARG;
                    }

                    sigaddset( &direct_config->dont_catch, signum );

                    s = NULL;
               }

               D_FREE( signals );
          }
          else {
               D_ERROR( "Direct/Config: '%s': No signals specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "thread_block_signals" ) == 0) {
          direct_config->thread_block_signals = true;
     } else
     if (strcmp( name, "no-thread_block_signals" ) == 0) {
          direct_config->thread_block_signals = false;
     } else
     if (strcmp( name, "thread-priority-scale" ) == 0) {
          if (value) {
               int scale;

               if (sscanf( value, "%d", &scale ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->thread_priority_scale = scale;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "thread-priority" ) == 0) {
          if (value) {
               int priority;

               if (sscanf( value, "%d", &priority ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->thread_priority = priority;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "thread-scheduler" ) == 0) {
          if (value) {
               if (strcmp( value, "other" ) == 0) {
                    direct_config->thread_scheduler = DCTS_OTHER;
               }
               else if (strcmp( value, "fifo" ) == 0) {
                    direct_config->thread_scheduler = DCTS_FIFO;
               }
               else if (strcmp( value, "rr" ) == 0) {
                    direct_config->thread_scheduler = DCTS_RR;
               }
               else {
                    D_ERROR( "Direct/Config: '%s': Unknown scheduler '%s'!\n", name, value );
                    return DR_INVARG;
               }
          }
          else {
               D_ERROR( "Direct/Config: '%s': No thread scheduler specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "thread-stacksize" ) == 0) {
          if (value) {
               int size;

               if (sscanf( value, "%d", &size ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->thread_stack_size = size;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "default-interface-implementation" ) == 0) {
          if (value) {
               char  itype[255];
               char *iname = 0;
               int   n     = 0;

               while (direct_config->default_interface_implementation_types &&
                      direct_config->default_interface_implementation_types[n])
                    n++;

               direct_config->default_interface_implementation_types =
                    D_REALLOC( direct_config->default_interface_implementation_types, sizeof(char*) * (n + 2) );
               direct_config->default_interface_implementation_names =
                    D_REALLOC( direct_config->default_interface_implementation_names, sizeof(char*) * (n + 2) );

               iname = strstr( value, "/" );

               if (!iname) {
                    D_ERROR( "Direct/Config: '%s': No interface/implementation specified!\n", name );
                    return DR_INVARG;
               }

               if (iname <= value) {
                    D_ERROR( "Direct/Config: '%s': No interface specified!\n", name );
                    return DR_INVARG;
               }

               if (strlen( iname ) < 2) {
                    D_ERROR( "Direct/Config: '%s': No implementation specified!\n", name );
                    return DR_INVARG;
               }

               direct_snputs( itype, value, iname - value + 1 );

               direct_config->default_interface_implementation_types[n] = D_STRDUP( itype );
               direct_config->default_interface_implementation_types[n+1] = NULL;

               direct_config->default_interface_implementation_names[n] = D_STRDUP( iname + 1 );
               direct_config->default_interface_implementation_names[n+1] = NULL;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No interface/implementation specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "log-delay-rand-loops" ) == 0) {
          if (value) {
               int max;

               if (sscanf( value, "%d", &max ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->log_delay_rand_loops = max;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "log-delay-rand-us" ) == 0) {
          if (value) {
               int max;

               if (sscanf( value, "%d", &max ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->log_delay_rand_us = max;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "log-delay-min-loops" ) == 0) {
          if (value) {
               int min;

               if (sscanf( value, "%d", &min ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->log_delay_min_loops = min;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "log-delay-min-us" ) == 0) {
          if (value) {
               int min;

               if (sscanf( value, "%d", &min ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->log_delay_min_us = min;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "delay-trap-ms" ) == 0) {
          if (value) {
               int ms;

               if (sscanf( value, "%d", &ms ) < 1) {
                    D_ERROR( "Direct/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               direct_config->delay_trap_ms = ms;
          }
          else {
               D_ERROR( "Direct/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     }
     else {
          ConfigOption *option = direct_map_lookup( config_options, name );
          if (option)
               config_option_value_add( option, value );
          else
               config_option_create( name, value );
     }

     D_DEBUG_AT( Direct_Config, "Set %s '%s'\n", name, value ?: "" );

     return DR_OK;
}

DirectResult
direct_config_get( const char  *name,
                   char       **values,
                   const int    values_len,
                   int         *ret_num )
{
     ConfigOption      *option;
     ConfigOptionValue *value;
     int                num = 0;

     option = direct_map_lookup( config_options, name );
     if (!option)
          return DR_ITEMNOTFOUND;

     *ret_num = 0;

     if (!option->values)
          return DR_OK;

     direct_list_foreach (value, option->values) {
          if (num >= values_len)
               break;

          values[num++] = value->value;
     }

     *ret_num = num;

     return DR_OK;
}

bool
direct_config_has_name( const char *name )
{
     ConfigOption      *option;

     option = direct_map_lookup( config_options, name );
     if (!option)
          return false;

     return true;
}

const char *
direct_config_get_value( const char *name )
{
     ConfigOption      *option;
     ConfigOptionValue *value;

     option = direct_map_lookup( config_options, name );
     if (!option || !option->values)
          return NULL;

     value = direct_list_get_last( option->values );

     D_ASSERT( value != NULL );

     return value->value;
}

long long
direct_config_get_int_value( const char *name )
{
     return direct_config_get_int_value_with_default( name, 0 );
}

long long
direct_config_get_int_value_with_default( const char *name,
                                          long long   def )
{
     ConfigOption      *option;
     ConfigOptionValue *value;

     option = direct_map_lookup( config_options, name );
     if (!option || !option->values)
          return def;

     value = direct_list_get_last( option->values );

     D_ASSERT( value != NULL );

     return atoll( value->value );
}
