/*****************************************************************************
 * audioqueue.c : AudioQueue audio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2002 the VideoLAN team
 *
 * Authors: Romain Goyet <romain.goyet@likid.org>
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
#include <unistd.h>                                      /* write(), close() */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include <AudioToolBox/AudioToolBox.h>

#define FRAME_SIZE 2048
#define NUMBER_OF_BUFFERS 3

/*****************************************************************************
 * aout_sys_t: SDL audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
	AudioQueueRef audioQueue;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );
static void Play        ( aout_instance_t * );
static void AudioQueueCallback (void *, AudioQueueRef, AudioQueueBufferRef);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( "AudioQueue" )
    set_description( N_("AudioQueue (iOS / Mac OS) audio output") )
    set_capability( "audio output", 40 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_shortcut( "audioqueue" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/

static int Open ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
	struct aout_sys_t *p_sys = malloc(sizeof(aout_sys_t));
    p_aout->output.p_sys = p_sys;
	
	OSStatus status = 0;
    // Setup the audio device.
    AudioStreamBasicDescription deviceFormat;
    deviceFormat.mSampleRate = 44100;
    deviceFormat.mFormatID = kAudioFormatLinearPCM;
    deviceFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger; // Signed integer, little endian
    deviceFormat.mBytesPerPacket = 4;
    deviceFormat.mFramesPerPacket = 1;
    deviceFormat.mBytesPerFrame = 4;
    deviceFormat.mChannelsPerFrame = 2;
    deviceFormat.mBitsPerChannel = 16;
    deviceFormat.mReserved = 0;
    // Create a new output AudioQueue for the device.
    // printf("AUDIOQUEUE NEW OUTPUT !!\n");
    status = AudioQueueNewOutput(&deviceFormat,      // Format
						AudioQueueCallback, // Callback
						p_aout,               // User data, passed to the callback
						CFRunLoopGetMain(), // RunLoop
						kCFRunLoopDefaultMode, // RunLoop mode
						0,                     // Flags ; must be zero...
						&(p_sys->audioQueue));          // Output
//	AudioQueueSetParameter(p_sys->audioQueue, kAudioQueueParam_Volume, 12.0);

    // printf("AUDIOQUEUE NEW OUTPUT CREATED %d\n", status);

	
    // Allocate buffers for the AudioQueue, and pre-fill them.
    for (int i = 0; i < NUMBER_OF_BUFFERS; ++i) {
		AudioQueueBufferRef buffer = NULL;
		status = AudioQueueAllocateBuffer(p_sys->audioQueue, FRAME_SIZE * 4, &buffer);
		AudioQueueCallback(NULL, p_sys->audioQueue, buffer);
	}

	/* Volume is entirely done in software. */
    aout_VolumeSoftInit( p_aout );


	p_aout->output.output.i_format = VLC_CODEC_S16L;
	p_aout->output.output.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    p_aout->output.output.i_rate = 44100;
    p_aout->output.i_nb_samples = FRAME_SIZE;
    p_aout->output.pf_play = Play;

	status = AudioQueueStart(p_sys->audioQueue, NULL);
    // printf("AUDIOQUEUE START %d\n", status);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
    VLC_UNUSED(p_aout);
	
//	AudioQueueCallback(p_aout, audioQueue, mBuffer);
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
	
	AudioQueueStop(p_sys->audioQueue, false);
	AudioQueueDispose(p_sys->audioQueue, false);
	free (p_sys);
}

void AudioQueueCallback(void * inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    // printf("AUDIOQUEUE CALLBACK !!\n");
	aout_instance_t * p_aout = (aout_instance_t *)inUserData;
	aout_buffer_t *   p_buffer = NULL;

    // printf("AUDIOQUEUE CALLBACK -> Poping FIFO\n");
	if (p_aout) {
		vlc_mutex_lock( &p_aout->output_fifo_lock );
		p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
		vlc_mutex_unlock( &p_aout->output_fifo_lock );
	}

    // printf("AUDIOQUEUE CALLBACK -> Checking if buffer is NULL\n");
	if ( p_buffer != NULL ) {
		vlc_memcpy( inBuffer->mAudioData, p_buffer->p_buffer, p_buffer->i_buffer );
		inBuffer->mAudioDataByteSize = p_buffer->i_buffer;
		aout_BufferFree( p_buffer );
	} else {
		vlc_memset( inBuffer->mAudioData, 0, inBuffer->mAudioDataBytesCapacity );
		inBuffer->mAudioDataByteSize = inBuffer->mAudioDataBytesCapacity;
	}
    // printf("p_buffer = %p\n", p_buffer);
    // printf("AudioCapacity = %d\n", inBuffer->mAudioDataBytesCapacity);
    // printf("AudioDataByteSize = %d\n", inBuffer->mAudioDataByteSize);
    OSStatus err = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    // printf("AUDIOQUEUE ENQUEUED %d\n", err);

}

