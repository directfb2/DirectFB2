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

#ifndef __CORE__GFXCARD_H__
#define __CORE__GFXCARD_H__

#include <core/coretypes.h>
#include <direct/modules.h>

DECLARE_MODULE_DIRECTORY( dfb_graphics_drivers );

/**********************************************************************************************************************/

#define DFB_GRAPHICS_DRIVER_ABI_VERSION          35

#define DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH     100
#define DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH  40

typedef struct {
     int major; /* major version */
     int minor; /* minor version */
} GraphicsDriverVersion;

struct __DFB_GraphicsDriverInfo {
     GraphicsDriverVersion version;

     char                  name[DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH];       /* Name of graphics driver */
     char                  vendor[DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH];   /* Vendor (or author) of the driver */
     char                  url[DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH];         /* URL for driver updates */
     char                  license[DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH]; /* License, e.g. 'LGPL' or 'proprietary' */

     unsigned int          driver_data_size;                                 /* Driver private data size to allocate */
     unsigned int          device_data_size;                                 /* Device private data size to allocate */
};

#define DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH     48
#define DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH   64

typedef enum {
     CCF_CLIPPING    = 0x00000001,
     CCF_NOTRIEMU    = 0x00000002,
     CCF_READSYSMEM  = 0x00000004,
     CCF_WRITESYSMEM = 0x00000008,

     CCF_RENDEROPTS  = 0x00000020
} CardCapabilitiesFlags;

typedef struct {
     CardCapabilitiesFlags   flags;

     DFBAccelerationMask     accel;
     DFBSurfaceBlittingFlags blitting;
     DFBSurfaceDrawingFlags  drawing;
     DFBAccelerationMask     clip;
} CardCapabilities;

typedef struct {
     unsigned int surface_byteoffset_alignment;
     unsigned int surface_pixelpitch_alignment;
     unsigned int surface_bytepitch_alignment;

     unsigned int surface_max_power_of_two_pixelpitch;
     unsigned int surface_max_power_of_two_bytepitch;
     unsigned int surface_max_power_of_two_height;

     DFBDimension dst_min;
     DFBDimension dst_max;
} CardLimitations;

struct __DFB_GraphicsDeviceInfo {
     char             name[DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH];     /* Device name. */
     char             vendor[DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH]; /* Vendor of the device. */

     CardCapabilities caps;                                           /* Hardware acceleration capabilities. */

     CardLimitations  limits;                                         /* Hardware limitations. */
};

typedef struct _GraphicsDeviceFuncs {
     /*
      * Called after screen information is changed.
      */
     void      (*AfterSetVar)      ( void                        *driver_data,
                                     void                        *device_data );

     /*
      * The driver should do the one time initialization of the engine, e.g. writing some registers that are supposed to
      * have a fixed value.
      */
     void      (*EngineReset)      ( void                        *driver_data,
                                     void                        *device_data );

     /*
      * Make sure that graphics hardware has finished all operations.
      */
     DFBResult (*EngineSync)       ( void                        *driver_data,
                                     void                        *device_data );

     /*
      * Called during dfb_gfxcard_lock() to notify the driver that the current rendering state is no longer valid.
      */
     void      (*InvalidateState)  ( void                        *driver_data,
                                     void                        *device_data );

     /*
      * After the video memory has been written to by the CPU (e.g. modification of a texture) make sure the accelerator
      * won't use cached texture data.
      */
     void      (*FlushTextureCache)( void                        *driver_data,
                                     void                        *device_data );

     /*
      * After the video memory has been written to by the accelerator make sure the CPU won't read back cached data.
      */
     void      (*FlushReadCache)   ( void                        *driver_data,
                                     void                        *device_data );

     /*
      * Return the serial of the last (queued) operation.
      * The serial is used to wait for finishing a specific graphics operation instead of the whole engine being idle.
      */
     void      (*GetSerial)        ( void                        *driver_data,
                                     void                        *device_data,
                                     CoreGraphicsSerial          *serial );

     /*
      * Make sure that graphics hardware has finished the specified operation.
      */
     DFBResult (*WaitSerial)       ( void                        *driver_data,
                                     void                        *device_data,
                                     const CoreGraphicsSerial    *serial );

     /*
      * Emit any buffered commands, i.e. trigger processing.
      */
     void      (*EmitCommands)     ( void                        *driver_data,
                                     void                        *device_data );

     /*
      * Check if the function 'accel' can be accelerated with the 'state'.
      * If that's true, the function sets the 'accel' bit in 'state->accel'.
      */
     void      (*CheckState)       ( void                        *driver_data,
                                     void                        *device_data,
                                     CardState                   *state,
                                     DFBAccelerationMask          accel );

     /*
      * Program card for execution of the function 'accel' with the 'state'.
      * 'state->modified' contains information about changed entries, at least 'accel' is set in 'state->set'.
      * The driver may modify 'funcs' depending on 'state' settings.
      */
     void      (*SetState)         ( void                        *driver_data,
                                     void                        *device_data,
                                     struct _GraphicsDeviceFuncs *funcs,
                                     CardState                   *state,
                                     DFBAccelerationMask          accel );

     /*
      * Drawing functions.
      */

     bool      (*FillRectangle)    ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBRectangle                *rect );

     bool      (*DrawRectangle)    ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBRectangle                *rect );

     bool      (*DrawLine)         ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBRegion                   *line );

     bool      (*FillTriangle)     ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBTriangle                 *tri );

     bool      (*FillTrapezoid)    ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBTrapezoid                *trap );

     bool      (*FillQuadrangles)  ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBPoint                    *points,
                                     int                          num );

     bool      (*DrawMonoGlyph)    ( void                        *driver_data,
                                     void                        *device_data,
                                     const void                  *glyph,
                                     int                          glyph_width,
                                     int                          glyph_height,
                                     int                          glyph_rowbyte,
                                     int                          glyph_offset,
                                     int                          dx,
                                     int                          dy,
                                     int                          fg_color,
                                     int                          bg_color,
                                     int                          hzoom,
                                     int                          vzoom );

     /*
      * Blitting functions.
      */

     bool      (*Blit)             ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBRectangle                *rect,
                                     int                          dx,
                                     int                          dy );

     bool      (*Blit2)            ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBRectangle                *rect,
                                     int                          dx,
                                     int                          dy,
                                     int                          sx2,
                                     int                          sy2 );

     bool      (*StretchBlit)      ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBRectangle                *srect,
                                     DFBRectangle                *drect );

     bool      (*TextureTriangles) ( void                        *driver_data,
                                     void                        *device_data,
                                     DFBVertex                   *vertices,
                                     int                          num,
                                     DFBTriangleFormation         formation );

     /*
      * Signal beginning of a sequence of operations using this 'state'.
      */
     void      (*StartDrawing)     ( void                        *driver_data,
                                     void                        *device_data,
                                     CardState                   *state );

     /*
      * Signal end of sequence.
      */
     void      (*StopDrawing)      ( void                        *driver_data,
                                     void                        *device_data,
                                     CardState                   *state );

     /*
      * BatchBlit: when driver returns false (late fallback), it may set 'ret_num' to the number of successful blits in
      * case of partial execution.
      */
     bool      (*BatchBlit)        ( void                        *driver_data,
                                     void                        *device_data,
                                     const DFBRectangle          *rects,
                                     const DFBPoint              *points,
                                     unsigned int                 num,
                                     unsigned int                *ret_num );

     /*
      * BatchFill: when driver returns false (late fallback), it may set 'ret_num' to the number of successful fills in
      * case of partial execution.
      */
     bool      (*BatchFill)        ( void                        *driver_data,
                                     void                        *device_data,
                                     const DFBRectangle          *rects,
                                     unsigned int                 num,
                                     unsigned int                *ret_num );

     /*
      * Callbacks when a state is created or destroyed. This allows a graphics driver to hold additional state.
      */

     void      (*StateInit)        ( void                        *driver_data,
                                     void                        *device_data,
                                     CardState                   *state );

     void      (*StateDestroy)     ( void                        *driver_data,
                                     void                        *device_data,
                                     CardState                   *state );

     /*
      * Calculate the amount of memory and pitch for the specified surface buffer.
      */
     DFBResult (*CalcBufferSize)   ( void                        *driver_data,
                                     void                        *device_data,
                                     CoreSurfaceBuffer           *buffer,
                                     int                         *ret_pitch,
                                     int                         *ret_length );
} GraphicsDeviceFuncs;

typedef struct {
     int       (*Probe)        ( void );

     void      (*GetDriverInfo)( GraphicsDriverInfo  *driver_info );

     DFBResult (*InitDriver)   ( GraphicsDeviceFuncs *funcs,
                                 void                *driver_data,
                                 void                *device_data,
                                 CoreDFB             *core );

     DFBResult (*InitDevice)   ( GraphicsDeviceInfo  *device_info,
                                 void                *driver_data,
                                 void                *device_data );

     void      (*CloseDevice)  ( void                *driver_data,
                                 void                *device_data );

     void      (*CloseDriver)  ( void                *driver_data );
} GraphicsDriverFuncs;

typedef enum {
     GDLF_NONE       = 0x00000000,

     GDLF_WAIT       = 0x00000001,
     GDLF_SYNC       = 0x00000002,
     GDLF_INVALIDATE = 0x00000004,
     GDLF_RESET      = 0x00000008
} GraphicsDeviceLockFlags;

/**********************************************************************************************************************/

DFBResult      dfb_gfxcard_lock                  ( GraphicsDeviceLockFlags        flags );

void           dfb_gfxcard_unlock                ( void );

DFBResult      dfb_gfxcard_flush                 ( void );

/*
 * Signal beginning of a sequence of operations using this state. Any number of states can be 'drawing'.
 */
void           dfb_gfxcard_start_drawing         ( CardState                     *state );

/*
 * Signal end of sequence, i.e. destination surface is consistent again.
 */
void           dfb_gfxcard_stop_drawing          ( CardState                     *state );

/*
 * This function returns non zero if acceleration is available for the specific function using the given state.
 */
bool           dfb_gfxcard_state_check           ( CardState                     *state,
                                                   DFBAccelerationMask            accel );

void           dfb_gfxcard_state_init            ( CardState                     *state );

void           dfb_gfxcard_state_destroy         ( CardState                     *state );

/*
 * Drawing functions, lock source and destination surfaces, handle clipping and drawing method (hardware/software).
 */

void           dfb_gfxcard_fillrectangles        ( DFBRectangle                  *rects,
                                                   int                            num,
                                                   CardState                     *state );

void           dfb_gfxcard_drawrectangle         ( DFBRectangle                  *rect,
                                                   CardState                     *state );

void           dfb_gfxcard_drawlines             ( DFBRegion                     *lines,
                                                   int                            num_lines,
                                                   CardState                     *state );

void           dfb_gfxcard_filltriangles         ( DFBTriangle                   *tris,
                                                   int                            num,
                                                   CardState                     *state );

void           dfb_gfxcard_filltrapezoids        ( DFBTrapezoid                  *traps,
                                                   int                            num,
                                                   CardState                     *state );

void           dfb_gfxcard_fillquadrangles       ( DFBPoint                      *points,
                                                   int                            num,
                                                   CardState                     *state );

void           dfb_gfxcard_fillspans             ( int                            y,
                                                   DFBSpan                       *spans,
                                                   int                            num_spans,
                                                   CardState                     *state );

void           dfb_gfxcard_draw_mono_glyphs      ( const void                    *glyph[],
                                                   const DFBMonoGlyphAttributes  *attributes,
                                                   const DFBPoint                *points,
                                                   unsigned int                   num,
                                                   CardState                     *state );

void           dfb_gfxcard_blit                  ( DFBRectangle                  *rect,
                                                   int                            dx,
                                                   int                            dy,
                                                   CardState                     *state );

void           dfb_gfxcard_batchblit             ( DFBRectangle                  *rects,
                                                   DFBPoint                      *points,
                                                   int                            num,
                                                   CardState                     *state );

void           dfb_gfxcard_batchblit2            ( DFBRectangle                  *rects,
                                                   DFBPoint                      *points,
                                                   DFBPoint                      *points2,
                                                   int                            num,
                                                   CardState                     *state );

void           dfb_gfxcard_stretchblit           ( DFBRectangle                  *srect,
                                                   DFBRectangle                  *drect,
                                                   CardState                     *state );

void           dfb_gfxcard_batchstretchblit      ( DFBRectangle                  *srects,
                                                   DFBRectangle                  *drects,
                                                   unsigned int                   num,
                                                   CardState                     *state );

void           dfb_gfxcard_tileblit              ( DFBRectangle                  *rect,
                                                   int                            dx1,
                                                   int                            dy1,
                                                   int                            dx2,
                                                   int                            dy2,
                                                   CardState                     *state );


void           dfb_gfxcard_texture_triangles     ( DFBVertex                     *vertices,
                                                   int                            num,
                                                   DFBTriangleFormation           formation,
                                                   CardState                     *state );

void           dfb_gfxcard_drawstring            ( const u8                      *text,
                                                   int                            bytes,
                                                   DFBTextEncodingID              encoding,
                                                   int                            x,
                                                   int                            y,
                                                   CoreFont                      *font,
                                                   unsigned int                   layers,
                                                   CoreGraphicsStateClient       *client,
                                                   DFBSurfaceTextFlags            flags );

void           dfb_gfxcard_drawglyph             ( CoreGlyphData                **glyph,
                                                   int                            x,
                                                   int                            y,
                                                   CoreFont                      *font,
                                                   unsigned int                   layers,
                                                   CoreGraphicsStateClient       *client,
                                                   DFBSurfaceTextFlags            flags );

/*
 * Check text rendering function.
 */
bool           dfb_gfxcard_drawstring_check_state( CoreFont                      *font,
                                                   CardState                     *state,
                                                   CoreGraphicsStateClient       *client,
                                                   DFBSurfaceTextFlags            flags );

DFBResult      dfb_gfxcard_sync                  ( void );

DFBResult      dfb_gfxcard_wait_serial           ( const CoreGraphicsSerial      *serial );

void           dfb_gfxcard_flush_texture_cache   ( void );

void           dfb_gfxcard_flush_read_cache      ( void );

void           dfb_gfxcard_after_set_var         ( void );

void           dfb_gfxcard_get_capabilities      ( CardCapabilities              *ret_caps );

void           dfb_gfxcard_get_device_info       ( GraphicsDeviceInfo            *ret_device_info );

void           dfb_gfxcard_get_driver_info       ( GraphicsDriverInfo            *ret_driver_info );

int            dfb_gfxcard_reserve_memory        ( unsigned int                   size );

unsigned int   dfb_gfxcard_memory_length         ( void );

volatile void *dfb_gfxcard_map_mmio              ( unsigned int                   offset,
                                                   int                            length );

void           dfb_gfxcard_unmap_mmio            ( volatile void                 *addr,
                                                   int                            length );

int            dfb_gfxcard_get_accelerator       ( void );

void           dfb_gfxcard_calc_buffer_size      ( CoreSurfaceBuffer             *buffer,
                                                   int                           *ret_pitch,
                                                   int                           *ret_length );

unsigned long  dfb_gfxcard_memory_physical       ( unsigned int                   offset );

void          *dfb_gfxcard_memory_virtual        ( unsigned int                   offset );

void          *dfb_gfxcard_get_device_data       ( void );

void          *dfb_gfxcard_get_driver_data       ( void );

#endif
