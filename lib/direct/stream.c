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

#include <direct/debug.h>
#include <direct/filesystem.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/stream.h>
#include <direct/util.h>

D_DEBUG_DOMAIN( Direct_Stream, "Direct/Stream", "Direct Stream wrapper" );

/**********************************************************************************************************************/

struct __D_DirectStream {
     int                   magic;
     int                   ref;

     int                   fd;
     DirectFile            file;

     off_t                 offset;
     ssize_t               length;

     char                 *mime;

     /* Cache for piped streams. */
     void                 *cache;
     unsigned int          cache_size;

     /* Remote streams data. */
     struct {
          int              sd;

          char            *host;
          int              port;
          struct addrinfo *addr;

          char            *user;
          char            *pass;
          char            *auth;

          char            *path;

          int              redirects;

          void            *data;
     } remote;

     DirectResult (*wait)( DirectStream *stream, unsigned int length, struct timeval *timeout );
     DirectResult (*peek)( DirectStream *stream, unsigned int length, int offset, void *buf, unsigned int *read_out );
     DirectResult (*read)( DirectStream *stream, unsigned int length, void *buf, unsigned int *read_out );
     DirectResult (*seek)( DirectStream *stream, unsigned int offset );
};

/**********************************************************************************************************************/

#define NET_TIMEOUT        15
#define HTTP_PORT          80
#define FTP_PORT           21
#define HTTP_MAX_REDIRECTS 15

static DirectResult file_open( DirectStream *stream, const char *filename );
static DirectResult http_open( DirectStream *stream, const char *filename );
static DirectResult ftp_open ( DirectStream *stream, const char *filename );

static void
direct_stream_close( DirectStream *stream )
{
     if (stream->remote.host) {
          D_FREE( stream->remote.host );
          stream->remote.host = NULL;
     }

     if (stream->remote.user) {
          D_FREE( stream->remote.user );
          stream->remote.user = NULL;
     }

     if (stream->remote.pass) {
          D_FREE( stream->remote.pass );
          stream->remote.pass = NULL;
     }

     if (stream->remote.auth) {
          D_FREE( stream->remote.auth );
          stream->remote.auth = NULL;
     }

     if (stream->remote.path) {
          D_FREE( stream->remote.path );
          stream->remote.path = NULL;
     }

     if (stream->remote.addr) {
          freeaddrinfo( stream->remote.addr );
          stream->remote.addr = NULL;
     }

     if (stream->remote.data) {
          D_FREE( stream->remote.data );
          stream->remote.data = NULL;
     }

     if (stream->remote.sd > 0) {
          close( stream->remote.sd );
          stream->remote.sd = -1;
     }

     if (stream->mime) {
          D_FREE( stream->mime );
          stream->mime = NULL;
     }

     if (stream->cache) {
          D_FREE( stream->cache );
          stream->cache = NULL;
          stream->cache_size = 0;
     }

     if (stream->fd >= 0) {
          fcntl( stream->fd, F_SETFL, fcntl( stream->fd, F_GETFL ) & ~O_NONBLOCK );
          close( stream->fd );
          stream->fd = -1;
     }
}

/**********************************************************************************************************************/

static __inline__ char *
trim( char *s )
{
     char *e;

     #define space(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n' || (c) == '"' || (c) == '\'')

     for (; space(*s); s++);

     e = s + strlen( s ) - 1;
     for (; e > s && space(*e); *e-- = '\0');

     #undef space

     return s;
}

static void
parse_url( const char  *url,
           char       **ret_host,
           int         *ret_port,
           char       **ret_user,
           char       **ret_pass,
           char       **ret_path )
{
     char *host = NULL;
     int   port = 0;
     char *user = NULL;
     char *pass = NULL;
     char *path;
     char *tmp;

     tmp = strchr( url, '/' );
     if (tmp) {
          host = alloca( tmp - url + 1 );
          direct_memcpy( host, url, tmp - url );
          host[tmp-url] = '\0';
          path = tmp;
     }
     else {
          host = alloca( strlen( url ) + 1 );
          direct_memcpy( host, url, strlen( url ) + 1 );
          path = "/";
     }

     tmp = strrchr( host, '@' );
     if (tmp) {
          *tmp = '\0';
          pass = strchr( host, ':' );
          if (pass) {
               *pass = '\0';
               pass++;
          }
          user = host;
          host = tmp + 1;
     }

     tmp = strchr( host, ':' );
     if (tmp) {
          port = strtol( tmp + 1, NULL, 10 );
          *tmp = '\0';
     }

     /* Host within brackets. */
     if (*host == '[') {
          host++;
          tmp = strchr( host, ']' );
          if (tmp)
               *tmp = '\0';
     }

     if (ret_host)
          *ret_host = D_STRDUP( host );

     if (ret_port && port)
          *ret_port = port;

     if (ret_user && user)
          *ret_user = D_STRDUP( user );

     if (ret_pass && pass)
          *ret_pass = D_STRDUP( pass );

     if (ret_path)
          *ret_path = D_STRDUP( path );
}

/**********************************************************************************************************************/

static int
net_response( DirectStream *stream,
              char         *buf,
              size_t        size )
{
     fd_set         set;
     struct timeval timeout;
     int            i;

     D_DEBUG_AT( Direct_Stream, "%s()\n", __FUNCTION__ );

     FD_ZERO( &set );
     FD_SET( stream->remote.sd, &set );

     for (i = 0; i < size - 1; i++) {
          timeout.tv_sec  = NET_TIMEOUT;
          timeout.tv_usec = 0;
          select( stream->remote.sd + 1, &set, NULL, NULL, &timeout );

          if (recv( stream->remote.sd, buf + i, 1, 0 ) != 1)
               break;

          if (buf[i] == '\n') {
               if (i > 0 && buf[i-1] == '\r')
                    i--;
               break;
          }
     }

     buf[i] = '\0';

     D_DEBUG_AT( Direct_Stream, "  -> got [%s]\n", buf );

     return i;
}

static int
net_command( DirectStream *stream,
             char         *buf,
             size_t        size )
{
     fd_set         s;
     struct timeval t;
     int            status;
     int            version;
     char           space;

     D_DEBUG_AT( Direct_Stream, "%s()\n", __FUNCTION__ );

     FD_ZERO( &s );
     FD_SET( stream->remote.sd, &s );

     t.tv_sec  = NET_TIMEOUT;
     t.tv_usec = 0;

     switch (select( stream->remote.sd + 1, NULL, &s, NULL, &t )) {
          case 0:
               D_DEBUG_AT( Direct_Stream, "  -> timeout\n" );
          case -1:
               return -1;
     }

     send( stream->remote.sd, buf, strlen( buf ), 0 );
     send( stream->remote.sd, "\r\n", 2, 0 );

     D_DEBUG_AT( Direct_Stream, "  -> sent [%s]\n", buf );

     while (net_response( stream, buf, size ) > 0) {
          status = 0;
          if (sscanf( buf, "HTTP/1.%d %3d", &version, &status ) == 2 ||
              sscanf( buf, "%3d%[ ]", &status, &space )         == 2)
               return status;
     }

     return 0;
}

static DirectResult
net_wait( DirectStream   *stream,
          unsigned int    length,
          struct timeval *timeout )
{
     fd_set s;

     D_DEBUG_AT( Direct_Stream, "%s()\n", __FUNCTION__ );

     if (stream->cache_size >= length)
          return DR_OK;

     if (stream->fd == -1)
          return DR_EOF;

     FD_ZERO( &s );
     FD_SET( stream->fd, &s );

     switch (select( stream->fd + 1, &s, NULL, NULL, timeout )) {
          case 0:
               if (!timeout && !stream->cache_size)
                    return DR_EOF;
               return DR_TIMEOUT;
          case -1:
               return errno2result( errno );
     }

     return DR_OK;
}

static DirectResult
net_peek( DirectStream *stream,
          unsigned int  length,
          int           offset,
          void         *buf,
          unsigned int *read_out )
{
     char    *tmp;
     ssize_t  size;

     D_DEBUG_AT( Direct_Stream, "%s()\n", __FUNCTION__ );

     if (offset < 0)
          return DR_UNSUPPORTED;

     tmp = alloca( length + offset );

     size = recv( stream->fd, tmp, length + offset, MSG_PEEK );
     switch (size) {
          case 0:
               return DR_EOF;
          case -1:
               if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return DR_BUFFEREMPTY;
               return errno2result( errno );
          default:
               if (size < offset)
                    return DR_BUFFEREMPTY;
               size -= offset;
               break;
     }

     direct_memcpy( buf, tmp + offset, size );

     if (read_out)
          *read_out = size;

     return DR_OK;
}

static DirectResult
net_read( DirectStream *stream,
          unsigned int  length,
          void         *buf,
          unsigned int *read_out )
{
     ssize_t size;

     D_DEBUG_AT( Direct_Stream, "%s()\n", __FUNCTION__ );

     size = recv( stream->fd, buf, length, 0 );
     switch (size) {
          case 0:
               return DR_EOF;
          case -1:
               if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return DR_BUFFEREMPTY;
               return errno2result( errno );
     }

     stream->offset += size;

     if (read_out)
          *read_out = size;

     return DR_OK;
}

static DirectResult
net_connect( struct addrinfo *addr,
             int              sock,
             int              proto,
             int             *ret_fd )
{
     DirectResult     ret = DR_OK;
     int              fd  = -1;
     struct addrinfo *tmp;

     D_DEBUG_AT( Direct_Stream, "%s()\n", __FUNCTION__ );

     D_ASSERT( addr != NULL );
     D_ASSERT( ret_fd != NULL );

     for (tmp = addr; tmp; tmp = tmp->ai_next) {
          int err;

          fd = socket( tmp->ai_family, sock, proto );
          if (fd < 0) {
               ret = errno2result( errno );
               D_DEBUG_AT( Direct_Stream, "  -> failed to create socket!\n" );
               continue;
          }

          fcntl( fd, F_SETFL, fcntl( fd, F_GETFL ) | O_NONBLOCK );

          D_DEBUG_AT( Direct_Stream, "  -> connecting to %s...\n", tmp->ai_canonname );

          if (proto == IPPROTO_UDP)
               err = bind( fd, tmp->ai_addr, tmp->ai_addrlen );
          else
               err = connect( fd, tmp->ai_addr, tmp->ai_addrlen );

          if (err == 0 || errno == EINPROGRESS) {
               struct timeval t = { NET_TIMEOUT, 0 };
               fd_set         s;

               /* Join multicast group. */
               if (tmp->ai_addr->sa_family == AF_INET) {
                    struct sockaddr_in *saddr = (struct sockaddr_in*) tmp->ai_addr;

                    if (IN_MULTICAST( ntohl(saddr->sin_addr.s_addr) )) {
                         struct ip_mreq req;

                         D_DEBUG_AT( Direct_Stream, "  -> joining multicast group (%d.%d.%d.%d)...\n",
                                     tmp->ai_addr->sa_data[2], tmp->ai_addr->sa_data[3],
                                     tmp->ai_addr->sa_data[4], tmp->ai_addr->sa_data[5] );

                         req.imr_multiaddr.s_addr = saddr->sin_addr.s_addr;
                         req.imr_interface.s_addr = 0;

                         err = setsockopt( fd, SOL_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req) );
                         if (err < 0) {
                              ret = errno2result( errno );
                              D_DEBUG_AT( Direct_Stream, "  -> could not join multicast group (%d.%d.%d.%d)\n",
                                          tmp->ai_addr->sa_data[2], tmp->ai_addr->sa_data[3],
                                          tmp->ai_addr->sa_data[4], tmp->ai_addr->sa_data[5] );
                              close( fd );
                              continue;
                         }

                         setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, saddr, sizeof(*saddr) );
                    }
               }

               FD_ZERO( &s );
               FD_SET( fd, &s );

               err = select( fd + 1, NULL, &s, NULL, &t );
               if (err < 1) {
                    D_DEBUG_AT( Direct_Stream, "  -> ...connection failed\n" );

                    close( fd );
                    fd = -1;

                    if (err == 0) {
                         ret = DR_TIMEOUT;
                         continue;
                    }
                    else {
                         ret = errno2result( errno );
                         break;
                    }
               }

               D_DEBUG_AT( Direct_Stream, "  -> ...connected\n" );

               ret = DR_OK;
               break;
          }
     }

     *ret_fd = fd;

     return ret;
}

static DirectResult
net_open( DirectStream *stream,
          const char   *filename,
          int           proto )
{
     DirectResult    ret;
     int             sock = (proto == IPPROTO_TCP) ? SOCK_STREAM : SOCK_DGRAM;
     struct addrinfo hints;
     char            port[16];

     D_DEBUG_AT( Direct_Stream, "%s()\n", __FUNCTION__ );

     parse_url( filename,
                &stream->remote.host,
                &stream->remote.port,
                &stream->remote.user,
                &stream->remote.pass,
                &stream->remote.path );

     snprintf( port, sizeof(port), "%d", stream->remote.port );

     memset( &hints, 0, sizeof(hints) );
     hints.ai_flags    = AI_CANONNAME;
     hints.ai_socktype = sock;
     hints.ai_family   = PF_UNSPEC;

     if (getaddrinfo( stream->remote.host, port, &hints, &stream->remote.addr )) {
          D_ERROR( "Direct/Stream: Failed to resolve host '%s'\n", stream->remote.host );
          return DR_FAILURE;
     }

     ret = net_connect( stream->remote.addr, sock, proto, &stream->remote.sd );
     if (ret)
          return ret;

     stream->fd     = stream->remote.sd;
     stream->length = -1;
     stream->wait   = net_wait;
     stream->peek   = net_peek;
     stream->read   = net_read;

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
http_seek( DirectStream *stream,
           unsigned int  offset )
{
     DirectResult ret;
     char         buf[1280];
     int          status, len;

     close( stream->remote.sd );
     stream->remote.sd = -1;

     ret = net_connect( stream->remote.addr, SOCK_STREAM, IPPROTO_TCP, &stream->remote.sd );
     if (ret)
          return ret;

     stream->fd = stream->remote.sd;

     len = snprintf( buf, sizeof(buf),
                     "GET %s HTTP/1.0\r\n"
                     "Host: %s:%d\r\n",
                     stream->remote.path, stream->remote.host, stream->remote.port );

     if (stream->remote.auth) {
          len += snprintf( buf + len, sizeof(buf) - len,
                           "Authorization: Basic %s\r\n",
                           stream->remote.auth );
     }

     snprintf( buf + len, sizeof(buf) - len,
               "User-Agent: DirectFB\r\n"
               "Accept: */*\r\n"
               "Range: bytes=%u-\r\n"
               "Connection: Close\r\n",
               offset );

     status = net_command( stream, buf, sizeof(buf) );
     switch (status) {
          case 200 ... 299:
               stream->offset = offset;
               break;
          default:
               if (status)
                    D_ERROR( "Direct/Stream: Server returned status %d\n", status );
               return DR_FAILURE;
     }

     /* Discard remaining headers. */
     while (net_response( stream, buf, sizeof(buf) ) > 0);

     return DR_OK;
}

static DirectResult
http_open( DirectStream *stream,
           const char   *filename )
{
     DirectResult ret;
     char         buf[1280];
     int          status, len;

     stream->remote.port = HTTP_PORT;

     ret = net_open( stream, filename, IPPROTO_TCP );
     if (ret)
          return ret;

     if (stream->remote.user) {
          char *tmp;

          if (stream->remote.pass) {
               tmp = alloca( strlen( stream->remote.user ) + strlen( stream->remote.pass ) + 2 );
               len = sprintf( tmp, "%s:%s", stream->remote.user, stream->remote.pass );
          }
          else {
               tmp = alloca( strlen( stream->remote.user ) + 2 );
               len = sprintf( tmp, "%s:", stream->remote.user );
          }

          stream->remote.auth = direct_base64_encode( tmp, len );
     }

     len = snprintf( buf, sizeof(buf),
                     "GET %s HTTP/1.0\r\n"
                     "Host: %s:%d\r\n",
                     stream->remote.path, stream->remote.host, stream->remote.port );

     if (stream->remote.auth) {
          len += snprintf( buf + len, sizeof(buf) - len,
                           "Authorization: Basic %s\r\n",
                           stream->remote.auth );
     }

     snprintf( buf + len, sizeof(buf) - len,
               "User-Agent: DirectFB\r\n"
               "Accept: */*\r\n"
               "Connection: Close\r\n" );

     status = net_command( stream, buf, sizeof(buf) );

     while (net_response( stream, buf, sizeof(buf) ) > 0) {
          if (!strncasecmp( buf, "Accept-Ranges:", 14 )) {
               if (strcmp( trim( buf + 14 ), "none" ))
                    stream->seek = http_seek;
          }
          else if (!strncasecmp( buf, "Content-Type:", 13 )) {
               char *mime = trim( buf + 13 );
               char *tmp  = strchr( mime, ';' );
               if (tmp)
                    *tmp = '\0';
               if (stream->mime)
                    D_FREE( stream->mime );
               stream->mime = D_STRDUP( mime );
          }
          else if (!strncasecmp( buf, "Content-Length:", 15 )) {
               char *tmp = trim( buf + 15 );
               if (sscanf( tmp, ""_ZD"", &stream->length ) < 1)
                    sscanf( tmp, "bytes="_ZD"", &stream->length );
          }
          else if (!strncasecmp( buf, "Location:", 9 )) {
               direct_stream_close( stream );
               stream->seek = NULL;

               if (++stream->remote.redirects > HTTP_MAX_REDIRECTS) {
                    D_ERROR( "Direct/Stream: Reached maximum number of redirects (%d)\n", HTTP_MAX_REDIRECTS );
                    return DR_LIMITEXCEEDED;
               }

               filename = trim( buf + 9 );
               if (!strncmp( filename, "http://", 7 ))
                    return http_open( stream, filename + 7 );
               if (!strncmp( filename, "ftp://", 6 ))
                    return ftp_open( stream, filename + 6 );

               return DR_UNSUPPORTED;
          }
     }

     switch (status) {
          case 200 ... 299:
               break;
          default:
               if (status)
                    D_ERROR( "Direct/Stream: Server returned status %d\n", status );
               return (status == 404) ? DR_FILENOTFOUND : DR_FAILURE;
     }

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
ftp_open_pasv( DirectStream *stream,
               char         *buf,
               size_t        size )
{
     DirectResult ret;
     int          i, len;

     snprintf( buf, size, "PASV" );
     if (net_command( stream, buf, size ) != 227)
          return DR_FAILURE;

     for (i = 4; buf[i]; i++) {
          unsigned int d[6];

          if (sscanf( &buf[i], "%u,%u,%u,%u,%u,%u", &d[0], &d[1], &d[2], &d[3], &d[4], &d[5] ) == 6) {
               struct addrinfo hints, *addr;

               /* Address */
               len = snprintf( buf, size, "%u.%u.%u.%u", d[0], d[1], d[2], d[3] );
               /* Port */
               snprintf( buf + len + 1, size - len - 1, "%u", ((d[4] & 0xff) << 8) | (d[5] & 0xff) );

               memset( &hints, 0, sizeof(hints) );
               hints.ai_flags    = AI_CANONNAME;
               hints.ai_socktype = SOCK_STREAM;
               hints.ai_family   = PF_UNSPEC;

               if (getaddrinfo( buf, buf + len + 1, &hints, &addr )) {
                    D_ERROR( "Direct/Stream: Failed to resolve host '%s'\n", buf );
                    return DR_FAILURE;
               }

               ret = net_connect( addr, SOCK_STREAM, IPPROTO_TCP, &stream->fd );

               freeaddrinfo( addr );

               return ret;
          }
     }

     return DR_FAILURE;
}

static DirectResult
ftp_seek( DirectStream *stream,
          unsigned int  offset )
{
     DirectResult ret = DR_OK;
     char         buf[512];

     if (stream->fd > 0) {
          int status;

          close( stream->fd );
          stream->fd = -1;

          /* Ignore response. */
          while (net_response( stream, buf, sizeof(buf) ) > 0) {
               if (sscanf( buf, "%3d%[ ]", &status, buf ) == 2)
                    break;
          }
     }

     ret = ftp_open_pasv( stream, buf, sizeof(buf) );
     if (ret)
          return ret;

     snprintf( buf, sizeof(buf), "REST %u", offset );
     if (net_command( stream, buf, sizeof(buf) ) != 350)
          goto error;

     snprintf( buf, sizeof(buf), "RETR %s", stream->remote.path );
     switch (net_command( stream, buf, sizeof(buf) )) {
          case 150:
          case 125:
               break;
          default:
               goto error;
     }

     stream->offset = offset;

     return DR_OK;

error:
     close( stream->fd );
     stream->fd = -1;

     return DR_FAILURE;
}

static DirectResult
ftp_open( DirectStream *stream,
          const char   *filename )
{
     DirectResult ret;
     int          status = 0;
     char         buf[512];

     stream->remote.port = FTP_PORT;

     ret = net_open( stream, filename, IPPROTO_TCP );
     if (ret)
          return ret;

     while (net_response( stream, buf, sizeof(buf) ) > 0) {
          if (sscanf( buf, "%3d%[ ]", &status, buf ) == 2)
               break;
     }
     if (status != 220)
          return DR_FAILURE;

     /* Login. */
     snprintf( buf, sizeof(buf), "USER %s", stream->remote.user ?: "anonymous" );
     switch (net_command( stream, buf, sizeof(buf) )) {
          case 230:
          case 331:
               break;
          default:
               return DR_FAILURE;
     }

     if (stream->remote.pass) {
          snprintf( buf, sizeof(buf), "PASS %s", stream->remote.pass );
          if (net_command( stream, buf, sizeof(buf) ) != 230)
               return DR_FAILURE;
     }

     /* Enter binary mode. */
     snprintf( buf, sizeof(buf), "TYPE I" );
     if (net_command( stream, buf, sizeof(buf) ) != 200)
          return DR_FAILURE;

     /* Get file size. */
     snprintf( buf, sizeof(buf), "SIZE %s", stream->remote.path );
     if (net_command( stream, buf, sizeof(buf) ) == 213)
          stream->length = strtol( buf + 4, NULL, 10 );

     /* Enter passive mode by default. */
     ret = ftp_open_pasv( stream, buf, sizeof(buf) );
     if (ret)
          return ret;

     /* Retrieve file. */
     snprintf( buf, sizeof(buf), "RETR %s", stream->remote.path );
     switch (net_command( stream, buf, sizeof(buf) )) {
          case 125:
          case 150:
               break;
          default:
               return DR_FAILURE;
     }

     stream->seek = ftp_seek;

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
pipe_wait( DirectStream   *stream,
           unsigned int    length,
           struct timeval *timeout )
{
     fd_set s;

     if (stream->cache_size >= length)
          return DR_OK;

     FD_ZERO( &s );
     FD_SET( stream->fd, &s );

     switch (select( stream->fd + 1, &s, NULL, NULL, timeout )) {
          case 0:
               if (!timeout && !stream->cache_size)
                    return DR_EOF;
               return DR_TIMEOUT;
          case -1:
               return errno2result( errno );
     }

     return DR_OK;
}

static DirectResult
pipe_peek( DirectStream *stream,
           unsigned int  length,
           int           offset,
           void         *buf,
           unsigned int *read_out )
{
     unsigned int size = length;
     int          len;

     if (offset < 0)
          return DR_UNSUPPORTED;

     len = length + offset;
     if (len > stream->cache_size) {
          ssize_t sz;

          stream->cache = D_REALLOC( stream->cache, len );
          if (!stream->cache) {
               stream->cache_size = 0;
               return D_OOM();
          }

          sz = read( stream->fd, stream->cache + stream->cache_size, len - stream->cache_size );
          if (sz < 0) {
               if (errno != EAGAIN || stream->cache_size == 0)
                    return errno2result( errno );
               sz = 0;
          }

          stream->cache_size += sz;
          if (stream->cache_size <= offset)
               return DR_BUFFEREMPTY;

          size = stream->cache_size - offset;
     }

     direct_memcpy( buf, stream->cache + offset, size );

     if (read_out)
          *read_out = size;

     return DR_OK;
}

static DirectResult
pipe_read( DirectStream *stream,
           unsigned int  length,
           void         *buf,
           unsigned int *read_out )
{
     unsigned int size = 0;

     if (stream->cache_size) {
          size = MIN( stream->cache_size, length );

          direct_memcpy( buf, stream->cache, size );

          length -= size;
          stream->cache_size -= size;

          if (stream->cache_size) {
               direct_memcpy( stream->cache, stream->cache + size, stream->cache_size );
          }
          else {
               D_FREE( stream->cache );
               stream->cache = NULL;
          }
     }

     if (length) {
          ssize_t sz;

          sz = read( stream->fd, buf + size, length - size );
          switch (sz) {
               case 0:
                    if (!size)
                         return DR_EOF;
                    break;
               case -1:
                    if (!size) {
                         return (errno == EAGAIN) ? DR_BUFFEREMPTY : errno2result( errno );
                    }
                    break;
               default:
                    size += sz;
                    break;
          }
     }

     stream->offset += size;

     if (read_out)
          *read_out = size;

     return DR_OK;
}

static DirectResult
file_wait( DirectStream   *stream,
           unsigned int    length,
           struct timeval *timeout )
{
     if (stream->offset >= stream->length)
          return DR_EOF;

     return DR_OK;
}

static DirectResult
file_peek( DirectStream *stream,
           unsigned int  length,
           int           offset,
           void         *buf,
           unsigned int *read_out )
{
     DirectResult ret;
     size_t      size;

     ret = direct_file_seek( &stream->file, offset );
     if (ret)
          return ret;

     ret = direct_file_read( &stream->file, buf, length, &size );
     if (ret)
          return ret;

     ret = direct_file_seek( &stream->file, - offset - size );
     if (ret)
          return ret;

     if (read_out)
          *read_out = size;

     return DR_OK;
}

static DirectResult
file_read( DirectStream *stream,
           unsigned int  length,
           void         *buf,
           unsigned int *read_out )
{
     DirectResult ret;
     size_t       size;

     ret = direct_file_read( &stream->file, buf, length, &size );
     if (ret)
          return ret;

     stream->offset += size;

     if (read_out)
          *read_out = size;

     return DR_OK;
}

static DirectResult
file_seek( DirectStream *stream,
           unsigned int  offset )
{
     DirectResult ret;

     ret = direct_file_seek_to( &stream->file, offset );
     if (ret)
          return ret;

     stream->offset = offset;

     return DR_OK;
}

static DirectResult
file_open( DirectStream *stream,
           const char   *filename )
{
     DirectResult   ret;
     DirectFileInfo info;

     ret = direct_file_open( &stream->file, filename, O_RDONLY | O_NONBLOCK, 0644 );
     if (ret)
          return ret;

     stream->fd = stream->file.fd;

     ret = direct_file_seek( &stream->file, 0 );
     if (ret == DR_IO) {
          stream->length = -1;
          stream->wait   = pipe_wait;
          stream->peek   = pipe_peek;
          stream->read   = pipe_read;
     }
     else {
          ret = direct_file_get_info( &stream->file, &info );
          if (ret) {
               direct_file_close( &stream->file );
               return ret;
          }

          stream->length = info.size;
          stream->wait   = file_wait;
          stream->peek   = file_peek;
          stream->read   = file_read;
          stream->seek   = file_seek;
     }

     return DR_OK;
}

/**********************************************************************************************************************/

DirectResult
direct_stream_create( const char    *filename,
                      DirectStream **ret_stream )
{
     DirectStream *stream;
     DirectResult  ret;

     D_ASSERT( filename != NULL );
     D_ASSERT( ret_stream != NULL );

     D_DEBUG_AT( Direct_Stream, "%s( '%s' )\n", __FUNCTION__, filename );

     stream = D_CALLOC( 1, sizeof(DirectStream) );
     if (!stream)
          return D_OOM();

     D_MAGIC_SET( stream, DirectStream );

     stream->ref =  1;
     stream->fd  = -1;

     if (!strncmp( filename, "file://", 7 )) {
          ret = file_open( stream, filename + 7 );
     }
     else if (!strncmp( filename, "http://", 7 )) {
          ret = http_open( stream, filename + 7 );
     }
     else if (!strncmp( filename, "ftp://", 6 )) {
          ret = ftp_open( stream, filename + 6 );
     }
     else if (!strncmp( filename, "tcp://", 6 )) {
          ret = net_open( stream, filename + 6, IPPROTO_TCP );
     }
     else if (!strncmp( filename, "udp://", 6 )) {
          ret = net_open( stream, filename + 6, IPPROTO_UDP );
     }
     else {
          ret = file_open( stream, filename );
     }

     if (ret) {
          direct_stream_close( stream );
          D_FREE( stream );
          return ret;
     }

     *ret_stream = stream;

     return DR_OK;
}

DirectStream*
direct_stream_dup( DirectStream *stream )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     stream->ref++;

     return stream;
}

int
direct_stream_fileno( DirectStream  *stream )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     return stream->fd;
}

bool
direct_stream_seekable( DirectStream *stream )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     return stream->seek ? true : false;
}

bool
direct_stream_remote( DirectStream *stream )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     if (stream->remote.host)
          return true;

     return false;
}

const char*
direct_stream_mime( DirectStream *stream )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     return stream->mime;
}

unsigned int
direct_stream_offset( DirectStream *stream )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     return stream->offset;
}

unsigned int
direct_stream_length( DirectStream *stream )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     return (unsigned int) ((stream->length >= 0) ? stream->length : stream->offset);
}

DirectResult
direct_stream_wait( DirectStream   *stream,
                    unsigned int    length,
                    struct timeval *timeout )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     if (length && stream->wait)
          return stream->wait( stream, length, timeout );

     return DR_OK;
}

DirectResult
direct_stream_peek( DirectStream *stream,
                    unsigned int  length,
                    int           offset,
                    void         *buf,
                    unsigned int *read_out )
{
     D_MAGIC_ASSERT( stream, DirectStream );
     D_ASSERT( length != 0 );
     D_ASSERT( buf != NULL );

     if (stream->length >= 0 && (stream->offset + offset) >= stream->length)
          return DR_EOF;

     if (stream->peek)
          return stream->peek( stream, length, offset, buf, read_out );

     return DR_UNSUPPORTED;
}

DirectResult
direct_stream_read( DirectStream *stream,
                    unsigned int  length,
                    void         *buf,
                    unsigned int *read_out )
{
     D_MAGIC_ASSERT( stream, DirectStream );
     D_ASSERT( length != 0 );
     D_ASSERT( buf != NULL );

     if (stream->length >= 0 && stream->offset >= stream->length)
          return DR_EOF;

     if (stream->read)
          return stream->read( stream, length, buf, read_out );

     return DR_UNSUPPORTED;
}

DirectResult
direct_stream_seek( DirectStream *stream,
                    unsigned int  offset )
{
     D_MAGIC_ASSERT( stream, DirectStream );

     if (stream->offset == offset)
          return DR_OK;

     if (stream->length >= 0 && offset > stream->length)
          offset = (unsigned int) stream->length;

     if (stream->seek)
          return stream->seek( stream, offset );

     return DR_UNSUPPORTED;
}

void
direct_stream_destroy( DirectStream *stream )
{
     D_DEBUG_AT( Direct_Stream, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stream, DirectStream );

     if (--stream->ref == 0) {
          direct_stream_close( stream );

          D_MAGIC_CLEAR( stream );

          D_FREE( stream );
     }
}
