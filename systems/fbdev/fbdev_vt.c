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

#include <core/core.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <linux/fb.h>
#include <linux/vt.h>
#include <sys/kd.h>
#include <termios.h>

#include "fbdev_vt.h"

D_DEBUG_DOMAIN( VT, "FBDev/VT", "FBDev VT Handling" );

/**********************************************************************************************************************/

#define SIG_SWITCH_FROM SIGUSR1
#define SIG_SWITCH_TO   SIGUSR2

typedef struct {
     CoreDFB          *core;

     int               fd0;       /* file descriptor of /dev/tty0 */
     int               fd;        /* file descriptor of /dev/ttyN where N is the VT number where DirectFB runs */

     int               prev;      /* VT number from which DirectFB was started */
     int               num;       /* VT number where DirectFB runs (use the given VT number instead of the current or a
                                     new one allocated for a VT switch), num = prev if no VT switch */
     bool              graphics;  /* put terminal into graphics mode */
     bool              vt_switch; /* allocate a new VT or use the given VT, and then switch to it */
     bool              switching; /* allow VT switching by pressing Ctrl+Alt+<F?> */


     int               old_fb;    /* original fb mapped to vt */
     struct termios    old_ts;    /* original termios */
     struct vt_mode    old_vtm;   /* original vt mode */

     struct sigaction  sig_usr1;  /* previous action for SIGUSR1 */
     struct sigaction  sig_usr2;  /* previous action for SIGUSR2 */
     int               sig;       /* signal signum */

     DirectMutex       lock;
     DirectWaitQueue   wait;
     DirectThread     *thread;

     bool              flush;
     DirectThread     *flush_thread;
} VirtualTerminal;

/**********************************************************************************************************************/

static VirtualTerminal *vt = NULL;

static int
vt_get_fb( int fbdev_fd,
           int num )
{
     struct fb_con2fbmap c2m;

     D_DEBUG_AT( VT, "%s( %d )\n", __FUNCTION__, num );

     c2m.console = num;

     if (ioctl( fbdev_fd, FBIOGET_CON2FBMAP, &c2m )) {
          D_PERROR( "FBDev/VT: FBIOGET_CON2FBMAP failed!\n" );
          return -1;
     }

     D_DEBUG_AT( VT, "  -> %u\n", c2m.framebuffer );

     return c2m.framebuffer;
}

static void
vt_set_fb( int fbdev_fd,
           int num,
           int fb )
{
     struct fb_con2fbmap c2m;
     struct stat         st;

     D_DEBUG_AT( VT, "%s( %d, %d )\n", __FUNCTION__, num, fb );

     if (fstat( fbdev_fd, &st )) {
          D_PERROR( "FBDev/VT: fstat() failed!\n" );
          return;
     }

     if (fb >= 0)
          c2m.framebuffer = fb;
     else
          c2m.framebuffer = (st.st_rdev & 0xff) >> 5;

     c2m.console = num;

     if (ioctl( fbdev_fd, FBIOPUT_CON2FBMAP, &c2m ) < 0) {
          D_PERROR( "FBDev/VT: FBIOPUT_CON2FBMAP failed!\n" );
     }
}

static void *
vt_switcher( DirectThread *thread,
             void         *arg )
{
     D_DEBUG_AT( VT, "%s( %p, %p )\n", __FUNCTION__, thread, arg );

     direct_mutex_lock( &vt->lock );

     while (true) {
          direct_thread_testcancel( thread );

          D_DEBUG_AT( VT, "%s() <- signal %d\n", __FUNCTION__, vt->sig );

          switch (vt->sig) {
               default:
                    D_BUG( "unexpected sig" );
                    /* fall through */

               case -1:
                    direct_waitqueue_wait( &vt->wait, &vt->lock );
                    continue;

               case SIG_SWITCH_FROM:
                    if (dfb_core_suspend( vt->core ) == DFB_OK) {
                         if (ioctl( vt->fd, VT_RELDISP, VT_ACKACQ ) < 0)
                              D_PERROR( "FBDev/VT: VT_RELDISP failed!\n" );
                    }
                    break;

               case SIG_SWITCH_TO:
                    if (dfb_core_resume( vt->core ) == DFB_OK) {
                         if (ioctl( vt->fd, VT_RELDISP, VT_ACKACQ ) < 0)
                              D_PERROR( "FBDev/VT: VT_RELDISP failed!\n" );

                         if (vt->graphics) {
                              if (ioctl( vt->fd, KDSETMODE, KD_GRAPHICS ) < 0)
                                   D_PERROR( "FBDev/VT: KD_GRAPHICS failed!\n" );
                         }
                    }
                    break;
          }

          vt->sig = -1;

          direct_waitqueue_signal( &vt->wait );
     }

     return NULL;
}

static void
vt_switch_handler( int signum )
{
     D_DEBUG_AT( VT, "%s( %d )\n", __FUNCTION__, signum );

     direct_mutex_lock( &vt->lock );

     while (vt->sig != -1)
          direct_waitqueue_wait( &vt->wait, &vt->lock );

     vt->sig = signum;

     direct_waitqueue_signal( &vt->wait );

     direct_mutex_unlock( &vt->lock );
}

static DFBResult
vt_init_switching()
{
     char           buf[32];
     struct termios ts;
     const char     blankoff_str[]  = "\033[9;0]";
     const char     cursoroff_str[] = "\033[?1;0;0c";
     ssize_t        sz;

     D_UNUSED_P( sz );

     D_DEBUG_AT( VT, "%s()\n", __FUNCTION__ );

     snprintf( buf, sizeof(buf), "/dev/tty%d", vt->num );
     vt->fd = open( buf, O_RDWR | O_NOCTTY );
     if (vt->fd < 0) {
          if (errno == ENOENT) {
               snprintf( buf, sizeof(buf), "/dev/vc/%d", vt->num );
               vt->fd = open( buf, O_RDWR | O_NOCTTY );
               if (vt->fd < 0) {
                    if (errno == ENOENT) {
                         D_PERROR( "FBDev/VT: Couldn't open neither '/dev/tty%d' nor '/dev/vc/%d'!\n",
                                    vt->num, vt->num );
                    }
                    else
                         D_PERROR( "FBDev/VT: Error opening '%s'!\n", buf );

                    return DFB_INIT;
               }
          }
          else {
               D_PERROR( "FBDev/VT: Error opening '%s'!\n", buf );

               return DFB_INIT;
          }
     }

     ioctl( vt->fd, TIOCSCTTY, 0 );

     if (ioctl( vt->fd, KDSKBMODE, K_MEDIUMRAW ) < 0) {
          D_PERROR( "FBDev/VT: K_MEDIUMRAW failed!\n" );
          close( vt->fd );
          return DFB_INIT;
     }

     if (tcgetattr( vt->fd, &vt->old_ts ) < 0) {
          D_PERROR( "FBDev/VT: tcgetattr() failed!\n" );
          ioctl( vt->fd, KDSKBMODE, K_XLATE );
          close( vt->fd );
          return DFB_INIT;
     }

     ts = vt->old_ts;
     ts.c_iflag      = 0;
     ts.c_lflag     &= ~(ICANON | ECHO | ISIG);
     ts.c_cc[VTIME]  = 0;
     ts.c_cc[VMIN]   = 1;

     if (tcsetattr( vt->fd, TCSAFLUSH, &ts ) < 0) {
          D_PERROR( "FBDev/VT: tcsetattr() failed!\n" );
          ioctl( vt->fd, KDSKBMODE, K_XLATE );
          close( vt->fd );
          return DFB_INIT;
     }

     sz = write( vt->fd, cursoroff_str, sizeof(cursoroff_str) );

     if (vt->graphics) {
          if (ioctl( vt->fd, KDSETMODE, KD_GRAPHICS ) < 0) {
               D_PERROR( "FBDev/VT: KD_GRAPHICS failed!\n" );
               tcsetattr( vt->fd, TCSAFLUSH, &vt->old_ts );
               ioctl( vt->fd, KDSKBMODE, K_XLATE );
               close( vt->fd );
               return DFB_INIT;
          }
     }
     else {
          sz = write( vt->fd, blankoff_str, sizeof(blankoff_str) );
     }

     if (vt->switching) {
          struct vt_mode   vtm;
          struct sigaction sig_tty;

          memset( &sig_tty, 0, sizeof(sig_tty) );
          sig_tty.sa_handler = vt_switch_handler;
          sigfillset( &sig_tty.sa_mask );

          if (sigaction( SIG_SWITCH_FROM, &sig_tty, &vt->sig_usr1 ) ||
              sigaction( SIG_SWITCH_TO, &sig_tty, &vt->sig_usr2 )) {
               D_PERROR( "FBDev/VT: sigaction() failed!\n" );
               tcsetattr( vt->fd, TCSAFLUSH, &vt->old_ts );
               ioctl( vt->fd, KDSKBMODE, K_XLATE );
               close( vt->fd );
               return DFB_INIT;
          }

          if (ioctl( vt->fd, VT_GETMODE, &vt->old_vtm )) {
               D_PERROR( "FBDev/VT: VT_GETMODE failed!\n" );
               sigaction( SIG_SWITCH_FROM, &vt->sig_usr1, NULL );
               sigaction( SIG_SWITCH_TO, &vt->sig_usr2, NULL );
               tcsetattr( vt->fd, TCSAFLUSH, &vt->old_ts );
               ioctl( vt->fd, KDSKBMODE, K_XLATE );
               close( vt->fd );
               return DFB_INIT;
          }

          memset( &vtm, 0, sizeof(vtm) );
          vtm.mode   = VT_PROCESS;
          vtm.relsig = SIG_SWITCH_FROM;
          vtm.acqsig = SIG_SWITCH_TO;

          if (ioctl( vt->fd, VT_SETMODE, &vtm ) < 0) {
               D_PERROR( "FBDev/VT: VT_SETMODE failed!\n" );
               ioctl( vt->fd, VT_SETMODE, &vt->old_vtm );
               sigaction( SIG_SWITCH_FROM, &vt->sig_usr1, NULL );
               sigaction( SIG_SWITCH_TO, &vt->sig_usr2, NULL );
               tcsetattr( vt->fd, TCSAFLUSH, &vt->old_ts );
               ioctl( vt->fd, KDSKBMODE, K_XLATE );
               close( vt->fd );
               return DFB_INIT;
          }

          direct_recursive_mutex_init( &vt->lock );

          direct_waitqueue_init( &vt->wait );

          vt->sig = -1;

          vt->thread = direct_thread_create( DTT_CRITICAL, vt_switcher, NULL, "VT Switcher" );
     }

     return DFB_OK;
}

static void *
vt_flusher( DirectThread *thread,
            void         *arg )
{
     D_DEBUG_AT( VT, "%s( %p, %p )\n", __FUNCTION__, thread, arg );

     while (vt->flush) {
          int    err;
          fd_set set;

          FD_ZERO( &set );
          FD_SET( vt->fd, &set );

          err = select( vt->fd + 1, &set, NULL, NULL, NULL );

          if (err < 0 && errno == EINTR)
               continue;

          if (err < 0 || !vt->flush)
               break;

          tcflush( vt->fd, TCIFLUSH );
     }

     return NULL;
}

static void
vt_start_flushing()
{
     vt->flush        = true;
     vt->flush_thread = direct_thread_create( DTT_DEFAULT, vt_flusher, NULL, "VT Flusher" );
}

DFBResult
vt_initialize( CoreDFB *core,
               int      fbdev_fd )
{
     DFBResult       ret;
     struct vt_stat  vs;

     D_DEBUG_AT( VT, "%s()\n", __FUNCTION__ );

     vt = D_CALLOC( 1, sizeof(VirtualTerminal) );
     if (!vt)
          return D_OOM();

     vt->core = core;

     vt->num = direct_config_get_int_value_with_default( "vt-num", -1 );

     /* Check root privileges for the VT switch. */
     if (!direct_geteuid()) {
          if (direct_config_has_name( "no-vt-switch" ) && !direct_config_has_name( "vt-switch" ))
               D_INFO( "FBDev/VT: Don't switch to a new VT or to the given VT\n" );
          else
               vt->vt_switch = true;
     }

     /* Always put terminal into graphics mode if no VT switch. */
     if (!vt->vt_switch)
          vt->graphics = true;
     else {
          if (direct_config_has_name( "no-vt-graphics" ) && !direct_config_has_name( "vt-graphics" ))
               D_INFO( "FBDev/VT: Don't put terminal into graphics mode\n" );
          else
               vt->graphics = true;
     }

     /* Check if VT switching is allowed. */
     if (direct_config_has_name( "no-vt-switching" ) && !direct_config_has_name( "vt-switching" ))
          D_INFO( "FBDev/VT: Don't allow VT switching by pressing Ctrl+Alt+<F?>\n" );
     else
          vt->switching = true;

     setsid();

     vt->fd0 = open( "/dev/tty0", O_RDONLY | O_NOCTTY );
     if (vt->fd0 < 0) {
          if (errno == ENOENT) {
               vt->fd0 = open( "/dev/vc/0", O_RDONLY | O_NOCTTY );
               if (vt->fd0 < 0) {
                    if (errno == ENOENT) {
                         D_PERROR( "FBDev/VT: Couldn't open neither '/dev/tty0' nor '/dev/vc/0'!\n" );
                    }
                    else
                         D_PERROR( "FBDev/VT: Error opening '/dev/vc/0'!\n" );

                    D_FREE( vt );
                    vt = NULL;

                    return DFB_INIT;
               }
          }
          else {
               D_PERROR( "FBDev/VT: Error opening '/dev/tty0'!\n" );

               D_FREE( vt );
               vt = NULL;

               return DFB_INIT;
          }
     }

     if (ioctl( vt->fd0, VT_GETSTATE, &vs ) < 0) {
          D_PERROR( "FBDev/VT: VT_GETSTATE failed!\n" );
          close( vt->fd0 );
          D_FREE( vt );
          vt = NULL;
          return DFB_INIT;
     }

     vt->prev = vs.v_active;

     if (!vt->vt_switch) {
          vt->num = vt->prev;

          /* Move vt to framebuffer. */
          vt->old_fb = vt_get_fb( fbdev_fd, vt->num );
          vt_set_fb( fbdev_fd, vt->num, -1 );
     }
     else {
          if (vt->num == -1) {
               int err;

               err = ioctl( vt->fd0, VT_OPENQRY, &vt->num );
               if (err < 0 || vt->num == -1) {
                    D_PERROR( "FBDev/VT: Cannot allocate VT!\n" );
                    close( vt->fd0 );
                    D_FREE( vt );
                    vt = NULL;
                    return DFB_INIT;
               }
          }

          /* Move vt to framebuffer. */
          vt->old_fb = vt_get_fb( fbdev_fd, vt->num );
          vt_set_fb( fbdev_fd, vt->num, -1 );

          D_DEBUG_AT( VT, "  -> switching to vt %d\n", vt->num );

          while (ioctl( vt->fd0, VT_ACTIVATE, vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               D_PERROR( "FBDev/VT: VT_ACTIVATE failed!\n" );
               close( vt->fd0 );
               D_FREE( vt );
               vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( vt->fd0, VT_WAITACTIVE, vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               D_PERROR( "FBDev/VT: VT_WAITACTIVE failed!\n" );
               close( vt->fd0 );
               D_FREE( vt );
               vt = NULL;
               return DFB_INIT;
          }

          usleep( 40000 );
     }

     ret = vt_init_switching();
     if (ret) {
          if (vt->vt_switch) {
               D_DEBUG_AT( VT, "  -> switching back...\n" );
               ioctl( vt->fd0, VT_ACTIVATE, vt->prev );
               ioctl( vt->fd0, VT_WAITACTIVE, vt->prev );
               D_DEBUG_AT( VT, "  -> ...switched back\n" );
               ioctl( vt->fd0, VT_DISALLOCATE, vt->num );
          }

          close( vt->fd0 );
          D_FREE( vt );
          vt = NULL;
          return ret;
     }

     vt_start_flushing();

     return DFB_OK;
}

static void
vt_stop_flushing()
{
     vt->flush = false;
     direct_thread_cancel( vt->flush_thread );
     direct_thread_join( vt->flush_thread );
     direct_thread_destroy( vt->flush_thread );
     vt->flush_thread = NULL;
}

DFBResult
vt_shutdown( bool emergency,
             int  fbdev_fd )
{
     const char blankon_str[]  = "\033[9;10]";
     const char cursoron_str[] = "\033[?0;0;0c";
     ssize_t    sz;

     D_UNUSED_P( sz );

     D_DEBUG_AT( VT, "%s()\n", __FUNCTION__ );

     if (!vt)
          return DFB_OK;

     vt_stop_flushing();

     if (vt->switching) {
          if (ioctl( vt->fd, VT_SETMODE, &vt->old_vtm ) < 0)
               D_PERROR( "FBDev/VT: VT_SETMODE for original values failed!\n" );

          sigaction( SIG_SWITCH_FROM, &vt->sig_usr1, NULL );
          sigaction( SIG_SWITCH_TO, &vt->sig_usr2, NULL );

          direct_thread_cancel( vt->thread );
          direct_thread_join( vt->thread );
          direct_thread_destroy( vt->thread );

          direct_mutex_deinit( &vt->lock );
          direct_waitqueue_deinit( &vt->wait );
     }

     if (vt->graphics) {
          if (ioctl( vt->fd, KDSETMODE, KD_TEXT ) < 0)
               D_PERROR( "FBDev/VT: KD_TEXT failed!\n" );
     }
     else
          sz = write( vt->fd, blankon_str, sizeof(blankon_str) );

     sz = write( vt->fd, cursoron_str, sizeof(cursoron_str) );

     if (tcsetattr( vt->fd, TCSAFLUSH, &vt->old_ts ) < 0)
          D_PERROR( "FBDev/VT: tcsetattr() for original values failed!\n" );

     if (ioctl( vt->fd, KDSKBMODE, K_XLATE ) < 0)
          D_PERROR( "FBDev/VT: K_XLATE failed!\n" );

     if (vt->vt_switch) {
          D_DEBUG_AT( VT, "  -> switching back...\n" );

          if (ioctl( vt->fd0, VT_ACTIVATE, vt->prev ) < 0)
               D_PERROR( "FBDev/VT: VT_ACTIVATE failed!\n" );

          if (ioctl( vt->fd0, VT_WAITACTIVE, vt->prev ) < 0)
               D_PERROR( "FBDev/VT: VT_WAITACTIVE failed!\n" );

          D_DEBUG_AT( VT, "  -> ...switched back\n" );

          usleep( 40000 );

          /* Restore con2fbmap. */
          vt_set_fb( fbdev_fd, vt->num, vt->old_fb );

          if (close( vt->fd ) < 0)
               D_PERROR( "FBDev/VT: Unable to close file descriptor of allocated VT!\n" );

          ioctl( vt->fd0, VT_DISALLOCATE, vt->num );
     }
     else {
          /* Restore con2fbmap. */
          vt_set_fb( fbdev_fd, vt->num, vt->old_fb );

          if (close( vt->fd ) < 0)
               D_PERROR( "FBDev/VT: Unable to close file descriptor of current VT!\n" );
     }

     if (close( vt->fd0 ) < 0)
          D_PERROR( "FBDev/VT: Unable to close file descriptor of tty0!\n" );

     D_FREE( vt );
     vt = NULL;

     return DFB_OK;
}

bool
vt_switch_num( int  num,
               bool key_pressed )
{
     D_DEBUG_AT( VT, "%s( %d )\n", __FUNCTION__, num );

     if (!vt->switching)
          return false;

     if (!key_pressed)
          return true;

     D_DEBUG_AT( VT, "  -> switching to vt %d\n", num );

     if (ioctl( vt->fd0, VT_ACTIVATE, num ) < 0)
          D_PERROR( "FBDev/VT: VT_ACTIVATE failed!\n" );

     return true;
}

void
vt_set_graphics_mode( bool set )
{
     const char blankoff_str[]  = "\033[9;0]";
     const char cursoroff_str[] = "\033[?1;0;0c";
     ssize_t    sz;

     D_UNUSED_P( sz );

     if (vt->graphics)
          return;

     if (set)
          ioctl( vt->fd, KDSETMODE, KD_GRAPHICS );
     else {
          ioctl( vt->fd, KDSETMODE, KD_TEXT );
          sz = write( vt->fd, cursoroff_str, strlen( cursoroff_str ) );
          sz = write( vt->fd, blankoff_str,  strlen( blankoff_str ) );
     }
}
