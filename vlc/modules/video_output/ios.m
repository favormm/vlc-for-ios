/*****************************************************************************
 * voutgl.m: iOS X OpenGLES provider
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Romain Goyet <romain.goyet at likid dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import <UIKit/UIKit.h>
#import <OpenGLES/ES1/gl.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_vout_opengl.h>

#define USE_OPENGL_ES 1
#include "opengl.h"
#include <QuartzCore/QuartzCore.h>

/**
 * Forward declarations
 */
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

static picture_pool_t *Pool(vout_display_t *vd, unsigned requested_count);
static void PictureRender(vout_display_t *vd, picture_t *pic);
static void PictureDisplay(vout_display_t *vd, picture_t *pic);
static int Control (vout_display_t *vd, int query, va_list ap);

static int OpenglClean(vout_opengl_t *gl);
static void OpenglSwap(vout_opengl_t *gl);

/**
 * Module declaration
 */
vlc_module_begin ()
    /* Will be loaded even without interface module. see voutgl.m */
    set_shortname("iOS")
    set_description( N_("iOS OpenGL ES video output (requires UIView)"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT )
    set_capability("vout display", 300)
    set_callbacks(Open, Close)

    add_shortcut("ios", "vout_ios")
vlc_module_end ()

@interface VLCOpenGLESVideoView : UIView
{
    vout_display_t * _vd;
	EAGLContext * _context;
    GLuint _defaultFramebuffer;
	GLuint _colorRenderbuffer;
	BOOL _framebufferDirty;
}
- (id)initWithFrame:(CGRect)frame andVOutDisplay:(vout_display_t *)vd;
@property (readonly) EAGLContext * context;
@property (readonly) GLuint colorRenderbuffer;
- (void)setVoutDisplay:(vout_display_t *)vd;
- (void)cleanFramebuffer;
@end


struct vout_display_sys_t
{
    VLCOpenGLESVideoView *glView;
    UIView * container;

    vout_opengl_t gl;
    vout_display_opengl_t vgl;

    picture_pool_t *pool;
    picture_t *current;
    bool has_first_frame;
};

// Called from vout thread
static int Open(vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = calloc(1, sizeof(*sys));
    NSAutoreleasePool *nsPool = nil;

    if (!sys)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->pool = NULL;
    sys->gl.sys = NULL;

    /* Get the drawable object */
    UIView * container = (UIView *)var_CreateGetAddress(vd, "drawable-nsobject");

    if (![container isKindOfClass:[UIView class]]) {
        msg_Dbg(vd, "Container isn't an UIView, passing over.");
        goto error;
    }
    vout_display_DeleteWindow(vd, NULL);

    /* This will be released in Close(), on
     * main thread, after we are done using it. */
    sys->container = [container retain];

    /* Get our main view*/
    nsPool = [[NSAutoreleasePool alloc] init];

	msg_Dbg(vd, "Creating VLCOpenGLESVideoView");
	sys->glView = [[VLCOpenGLESVideoView alloc] initWithFrame:[container bounds] andVOutDisplay:vd];
    if (!sys->glView)
        goto error;

    /* We don't wait, that means that we'll have to be careful about releasing
     * container.
     * That's why we'll release on main thread in Close(). */
	[container performSelectorOnMainThread:@selector(addSubview:) withObject:sys->glView waitUntilDone:NO];

    [nsPool drain];
    nsPool = nil;

    /* Initialize common OpenGL video display */
    sys->gl.lock = OpenglClean; // We don't do locking, but sometimes we need to cleanup the framebuffer
    sys->gl.unlock = NULL;
    sys->gl.swap = OpenglSwap;
    sys->gl.sys = sys;

    if (vout_display_opengl_Init(&sys->vgl, &vd->fmt, &sys->gl))
    {
        sys->gl.sys = NULL;
        goto error;
    }

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = false;

    /* Setup vout_display_t once everything is fine */
    vd->info = info;

    vd->pool = Pool;
    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;

    /* */
    vout_display_SendEventFullscreen (vd, false);
    vout_display_SendEventDisplaySize (vd, vd->source.i_visible_width, vd->source.i_visible_height, false);

    return VLC_SUCCESS;

error:
    [nsPool release];
    Close(this);
    return VLC_EGENERIC;
}

// Called from vout thread as well
void Close(vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = vd->sys;

    [sys->glView setVoutDisplay:nil];

    var_Destroy(vd, "drawable-nsobject");
    /* release on main thread as explained in Open() */
    [(id)sys->container performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    [sys->glView performSelectorOnMainThread:@selector(removeFromSuperview) withObject:nil waitUntilDone:NO];

    [sys->glView release];

    if (sys->gl.sys != NULL)
        vout_display_opengl_Clean(&sys->vgl);

    free (sys);
}

/*****************************************************************************
 * vout display callbacks
 *****************************************************************************/

static picture_pool_t *Pool(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(requested_count);

    if (!sys->pool)
        sys->pool = vout_display_opengl_GetPool (&sys->vgl);
    assert(sys->pool);
    return sys->pool;
}

static void PictureRender(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Prepare( &sys->vgl, pic );
}

static void PictureDisplay(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    vout_display_opengl_Display(&sys->vgl, &vd->fmt );
    picture_Release (pic);
    sys->has_first_frame = true;
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
        case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        {
            return VLC_EGENERIC;
        }
        case VOUT_DISPLAY_HIDE_MOUSE:
            return VLC_SUCCESS;

        case VOUT_DISPLAY_GET_OPENGL:
        {
            vout_opengl_t **gl = va_arg (ap, vout_opengl_t **);
            *gl = &sys->gl;
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_RESET_PICTURES:
            assert (0);
        default:
            msg_Err (vd, "Unknown request in iOS vout display");
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/

static int OpenglClean(vout_opengl_t *gl) {
    vout_display_sys_t *sys = gl->sys;
	[sys->glView cleanFramebuffer];
	return 0;
}

static void OpenglSwap(vout_opengl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;
    EAGLContext *context = [sys->glView context];
	[context presentRenderbuffer:GL_RENDERBUFFER_OES];
}

/*****************************************************************************
 * Our UIView object
 * *ALL* OpenGL calls should happen in the render thread
 *****************************************************************************/

@interface VLCOpenGLESVideoView (Private)
- (void)_createFramebuffer;
- (void)_updateViewportWithBackingWitdh:(GLuint)backingWidth andBackingHeight:(GLuint)backingHeight;
- (void)_destroyFramebuffer;
@end

@implementation VLCOpenGLESVideoView
@synthesize context=_context, colorRenderbuffer=_colorRenderbuffer;
#define VLCAssertMainThread() assert([[NSThread currentThread] isMainThread])

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

/**
 * Gets called by the Open() method.
 */

- (id)initWithFrame:(CGRect)frame andVOutDisplay:(vout_display_t *)vd {
	if (self = [super initWithFrame:frame]) {
		_vd = vd;
		CAEAGLLayer * eaglLayer = (CAEAGLLayer *)self.layer;

        eaglLayer.opaque = TRUE;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
//										[NSNumber numberWithBool:FALSE], kEAGLDrawablePropertyRetainedBacking,
										kEAGLColorFormatRGB565, kEAGLDrawablePropertyColorFormat,
										nil];

		_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
		NSAssert(_context && [EAGLContext setCurrentContext:_context], @"Creating context");

		[self _createFramebuffer];
		_framebufferDirty = NO;
		
		[self setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
	}
    return self;
}

- (void) dealloc
{
    [_context release];
    [super dealloc];
}

/**
 * Gets called by the Close and Open methods.
 * (Non main thread).
 */
- (void)setVoutDisplay:(vout_display_t *)aVd
{
    @synchronized(self) {
        _vd = aVd;
    }
}


/**
 * Method called by UIKit when we have been resized
 */
- (void)layoutSubviews {
	// CAUTION : This is called from the main thread
	_framebufferDirty = YES;
}

- (void)cleanFramebuffer {
	if (_framebufferDirty) {
		[self _destroyFramebuffer];
		[self _createFramebuffer];
		_framebufferDirty = NO;
	}
}

@end

@implementation VLCOpenGLESVideoView (Private)
- (void)_createFramebuffer {
	msg_Dbg(_vd, "Creating framebuffer for layer %p with bounds (%.1f,%.1f,%.1f,%.1f)", self.layer, self.layer.bounds.origin.x, self.layer.bounds.origin.y, self.layer.bounds.size.width, self.layer.bounds.size.height);
	[EAGLContext setCurrentContext:_context];
	// Create default framebuffer object. The backing will be allocated for the current layer in -resizeFromLayer
	glGenFramebuffersOES(1, &_defaultFramebuffer); // Generate one framebuffer, store it in _defaultFrameBuffer
	glGenRenderbuffersOES(1, &_colorRenderbuffer);
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, _defaultFramebuffer);
	glBindRenderbufferOES(GL_RENDERBUFFER_OES, _colorRenderbuffer);

	// This call associates the storage for the current render buffer with the EAGLDrawable (our CAEAGLLayer)
	// allowing us to draw into a buffer that will later be rendered to screen wherever the layer is (which corresponds with our view).
	[_context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(id<EAGLDrawable>)self.layer];
	glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, _colorRenderbuffer);

	GLuint backingWidth, backingHeight;
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);

	if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
		msg_Err(_vd, "Failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
	}
	[self _updateViewportWithBackingWitdh:backingWidth andBackingHeight:backingHeight];
}

- (void)_updateViewportWithBackingWitdh:(GLuint)backingWidth andBackingHeight:(GLuint)backingHeight {
	msg_Dbg(_vd, "Reshaping to %dx%d", backingWidth, backingHeight);
	
	CGFloat width = (CGFloat)backingWidth;
	CGFloat height = (CGFloat)backingHeight;
	
    GLint x = width, y = height;
	
	if (_vd) {
		CGFloat videoHeight = _vd->source.i_visible_height;
		CGFloat videoWidth = _vd->source.i_visible_width;
		
		GLint sarNum = _vd->source.i_sar_num;
		GLint sarDen = _vd->source.i_sar_den;
		
		if (height * videoWidth * sarNum < width * videoHeight * sarDen)
		{
			x = (height * videoWidth * sarNum) / (videoHeight * sarDen);
			y = height;
		}
		else
		{
			x = width;
			y = (width * videoHeight * sarDen) / (videoWidth * sarNum);
		}
	}
	
	[EAGLContext setCurrentContext:_context];
	glViewport((width - x) / 2, (height - y) / 2, x, y);
}

- (void)_destroyFramebuffer {
	[EAGLContext setCurrentContext:_context];
	glDeleteFramebuffersOES(1, &_defaultFramebuffer);
	_defaultFramebuffer = 0;
	glDeleteRenderbuffersOES(1, &_colorRenderbuffer);
	_colorRenderbuffer = 0;
}
@end

