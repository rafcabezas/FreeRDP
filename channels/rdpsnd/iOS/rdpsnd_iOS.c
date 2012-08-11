/*
   FreeRDP: A Remote Desktop Protocol client.

   Copyright 2011 Vic Lee

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rdpdr_types.h"
#include "rdpsnd_main.h"
#include <freerdp/utils/stream.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/dsp.h>
#include <freerdp/utils/svc_plugin.h>

#define IMPORTING_FROM_C_FILE
#import "iOSSound.h"
#import "ConnectionConfig.h"

#define LOG_LEVEL 0
#define LLOG(_level, _args) \
  do { if (_level < LOG_LEVEL) { printf _args ; } } while (0)
#define LLOGLN(_level, _args) \
  do { if (_level < LOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

RDPAudioMode getRDPAudioMode(void *cdvc); // In RDPWrapper_Callbacks
double       getAudioBufferInSec(void *cdvc); // In RDPWrapper_Callbacks

typedef struct iOS_device_data rdpsndiOSPlugin;
struct iOS_device_data
{
    rdpsndDevicePlugin device;

	char* device_name;
	int format;
	int block_size;

    ADPCM adpcm;
    
    //ios
    int   IOS_RATE_MAX;
    int   IOS_CHANNELS_MAX;
    void *context;
    IOSSoundFormat soundFormat;
    void *cdvc;
    
    FREERDP_DSP_CONTEXT* dsp_context;
};

static void rdpsnd_ios_play(rdpsndDevicePlugin* device, uint8* data, int size)
{
    rdpsndiOSPlugin* iOS_data;
    //int rc;
    
	iOS_data = (rdpsndiOSPlugin*) device;
	if (!iOS_data->context)
		return;
    
	uint8* src;

	if (iOS_data->format == 2)
	{
		iOS_data->dsp_context->decode_ms_adpcm(iOS_data->dsp_context,
                                               data, size, iOS_data->soundFormat.channels,
                                               iOS_data->block_size);
		size = iOS_data->dsp_context->adpcm_size;
		src = iOS_data->dsp_context->adpcm_buffer;
	}
	else if (iOS_data->format == 0x11)
	{
		iOS_data->dsp_context->decode_ima_adpcm(iOS_data->dsp_context,
                                                data, size, iOS_data->soundFormat.channels,
                                                iOS_data->block_size);
		size = iOS_data->dsp_context->adpcm_size;
		src = iOS_data->dsp_context->adpcm_buffer;
	}
	else
	{
		src = data;
	}

	LLOGLN(10, ("rdpsnd_iOS_play: size %d", size));
    
    iOSSoundPlay(iOS_data->context, (char *)src, size);
    
    LLOGLN(0, ("rdpsnd_iOS_play: (end)"));

	return;
}


static void rdpsnd_ios_set_volume(rdpsndDevicePlugin * devplugin, uint32 value)
{
	LLOGLN(0, ("rdpsnd_iOS_set_volume: %8.8x", value));
	return;
}

static void rdpsnd_ios_set_format(rdpsndDevicePlugin* device, rdpsndFormat* format, int latency)
{
	rdpsndiOSPlugin* iOS_data;
	int nChannels;
	int wBitsPerSample;
	int nSamplesPerSec;
	int wFormatTag;
	int nBlockAlign;
    
	iOS_data = (rdpsndiOSPlugin*) device;
	if (!iOS_data->context)
		return;
	wFormatTag = format->wFormatTag;
	nChannels = format->nChannels;
	nSamplesPerSec = format->nSamplesPerSec;
	nBlockAlign = format->nBlockAlign;
	wBitsPerSample = format->wBitsPerSample;
	LLOGLN(0, ("rdpsnd_iOS_set_format: wFormatTag=%d nChannels=%d "
               "nSamplesPerSec=%d wBitsPerSample=%d nBlockAlign=%d",
               wFormatTag, nChannels, nSamplesPerSec, wBitsPerSample, nBlockAlign));
    
    iOS_data->soundFormat.channels = nChannels;
    iOS_data->soundFormat.formatFlags = kAudioFormatFlagIsSignedInteger;
	switch (wFormatTag)
	{
		case 1: /* PCM */
			switch (wBitsPerSample)
            {
                case 8:
                    iOS_data->soundFormat.formatID    = kAudioFormatLinearPCM;
                    break;
                case 16:
                    iOS_data->soundFormat.formatID    = kAudioFormatLinearPCM;
                    break;
            }
			break;
            
		case 6: /* A-LAW */
            iOS_data->soundFormat.formatID    = kAudioFormatALaw;
			break;
            
		case 7: /* U-LAW */
            iOS_data->soundFormat.formatID    = kAudioFormatULaw;
			break;
            
        case 2: /* MS ADPCM */
		case 0x11: /* IMA ADPCM */
            iOS_data->soundFormat.formatID    = kAudioFormatLinearPCM;
            wBitsPerSample = 16;
			break;
	}
    
	iOS_data->format = wFormatTag;
	iOS_data->block_size = nBlockAlign;
    
    iOS_data->soundFormat.bitsPerSample = wBitsPerSample;
    iOS_data->soundFormat.sampleRate = nSamplesPerSec;
    
    iOSSetFormat(iOS_data->context, &iOS_data->soundFormat);
    iOSSetBufferInSec(iOS_data->context, getAudioBufferInSec(iOS_data->cdvc));
    iOSEnableDelay(iOS_data->context, 0);

    //rdpsnd_iOS_open(devplugin);
    LLOGLN(0, ("rdpsnd_iOS_set_format: (end)"));

	return;
}

static void rdpsnd_ios_open(rdpsndDevicePlugin* device, rdpsndFormat* format, int latency)
{
	rdpsndiOSPlugin* ios = (rdpsndiOSPlugin*)device;
	if (!ios->context)
		return;
    
	DEBUG_SVC("rdpsnd_ios_open");
    
    memset(&ios->adpcm, 0, sizeof(ADPCM));
    rdpsnd_ios_set_format(device, format, latency);
}

static boolean rdpsnd_ios_format_supported(rdpsndDevicePlugin* device, rdpsndFormat* format)
{
	rdpsndiOSPlugin* iOS_data;
	int nChannels;
	int wBitsPerSample;
	int nSamplesPerSec;
	int cbSize;
	int wFormatTag;
    
	iOS_data = (rdpsndiOSPlugin*) device;
	if (!iOS_data->context)
		return false;
	wFormatTag = format->wFormatTag;
	nChannels = format->nChannels;
	nSamplesPerSec = format->nSamplesPerSec;
	wBitsPerSample = format->wBitsPerSample;
	cbSize = format->cbSize;
	LLOGLN(0, ("rdpsnd_iOS_format_supported: wFormatTag=%d "
                "nChannels=%d nSamplesPerSec=%d wBitsPerSample=%d cbSize=%d",
                wFormatTag, nChannels, nSamplesPerSec, wBitsPerSample, cbSize));
    
    RDPAudioMode userSelectedAudioMode = getRDPAudioMode(iOS_data->cdvc);
    int minSampleRate = 0;
    if (userSelectedAudioMode == RDP_AUDIO_ADAPTIVE_HIGH) {
        minSampleRate = 22050;
    }
    
    /*
    if ((nSamplesPerSec % 11025) != 0) {
        LLOGLN(0, ("Invalid Sample Rate"));
        return 0;
    }
    */

	switch (wFormatTag)
	{
		case 1: /* PCM */
			if (cbSize == 0 &&
				(nSamplesPerSec <= iOS_data->IOS_RATE_MAX) &&
				(wBitsPerSample == 8 || wBitsPerSample == 16) &&
				(nChannels >= 1 && nChannels <= iOS_data->IOS_CHANNELS_MAX))
			{
				LLOGLN(0, ("rdpsnd_iOS_format_supported: PCM ok"));
				return true;
			}
			break;
            
		case 6: /* A-LAW */
		case 7: /* U-LAW */
            if (userSelectedAudioMode == RDP_AUDIO_ADAPTIVE_HIGH
                || userSelectedAudioMode == RDP_AUDIO_ADAPTIVE_LOW) {
                
                if (cbSize == 0 &&
                    (nSamplesPerSec >= minSampleRate) &&
                    (nSamplesPerSec <= iOS_data->IOS_RATE_MAX) &&
                    (wBitsPerSample == 8) &&
                    (nChannels >= 1 && nChannels <= iOS_data->IOS_CHANNELS_MAX))
                {
                    LLOGLN(0, ("rdpsnd_iOS_format_supported:ALAW/ULAW ok"));
                    return true;
                }
            }
			break;
            
		case 2: /* MS ADPCM */
		case 0x11: /* IMA ADPCM */
            if (userSelectedAudioMode == RDP_AUDIO_ADAPTIVE_HIGH
                || userSelectedAudioMode == RDP_AUDIO_ADAPTIVE_LOW) {

                if ((nSamplesPerSec <= iOS_data->IOS_RATE_MAX) &&
                    (nSamplesPerSec >= minSampleRate) &&
                    (wBitsPerSample == 4) &&
                    (nChannels == 1 || nChannels == 2))
                {
                    LLOGLN(0, ("rdpsnd_iOS_format_supported:ADPCM ok"));
                    return true;
                }
            }
			break;
	}
	return false;
}

static void
rdpsnd_ios_close(rdpsndDevicePlugin * devplugin)
{
	rdpsndiOSPlugin* iOS_data;
    
	iOS_data = (rdpsndiOSPlugin*) devplugin;
	if (!iOS_data->context)
		return;
	LLOGLN(0, ("rdpsnd_iOS_close: (start)"));
    if (iOS_data->context)
    {
        //while (!isDonePlayingBuffer(iOS_data->context)) { usleep(1000); };
        iOSSoundStop(iOS_data->context);
    }
	LLOGLN(0, ("rdpsnd_iOS_close: (end)"));

	return;
}

static void
rdpsnd_ios_free(rdpsndDevicePlugin * devplugin)
{
	rdpsndiOSPlugin* iOS_data;
    
	iOS_data = (rdpsndiOSPlugin*) devplugin;
	LLOGLN(10, ("rdpsnd_iOS_free:"));
	if (!iOS_data)
		return;
	rdpsnd_ios_close(devplugin);

    if (iOS_data->context)
    {
        iOSSoundDealloc(iOS_data->context);
        iOS_data->context = NULL;
    }
    
	free(iOS_data);
	devplugin = NULL;
}

static void rdpsnd_ios_start(rdpsndDevicePlugin* device)
{
    /*
	rdpsndiOSPlugin* ios = (rdpsndiOSPlugin*)device;
    
    ios->context = iOSSoundAllocAndInit();
    if (!ios->context)
    {
        LLOGLN(0, ("rdpsnd_iOS: iossound context alloc/init failed"));
        rdpsnd_ios_free(device);
    }
     */
}

int FreeRDPRdpsndDeviceEntry_iOS(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
    rdpsndiOSPlugin *ios;
    RDP_PLUGIN_DATA *data;
    
    ios = xnew(rdpsndiOSPlugin);
    
    ios->device.Open = rdpsnd_ios_open;
    ios->device.FormatSupported = rdpsnd_ios_format_supported;
    ios->device.SetFormat = rdpsnd_ios_set_format;
    ios->device.SetVolume = rdpsnd_ios_set_volume;
    ios->device.Play = rdpsnd_ios_play;
    ios->device.Start = rdpsnd_ios_start;
    ios->device.Close = rdpsnd_ios_close;
    ios->device.Free = rdpsnd_ios_free;
    
    data = pEntryPoints->plugin_data;
    if (data && strcmp((char*)data->data[0], "iOS") == 0)
    {
        ios->device_name = xstrdup("default");//xstrdup((char*)data->data[1]);
    }
    if (ios->device_name == NULL)
    {
        ios->device_name = xstrdup("default");
    }
    ios->IOS_RATE_MAX = 44100;
    ios->IOS_CHANNELS_MAX = 2;
    //ios->out_handle = 0;
    //ios->source_rate = 22050;
    //ios->actual_rate = 22050;
    //ios->format = SND_PCM_FORMAT_S16_LE;
    //ios->source_channels = 2;
    //ios->actual_channels = 2;
    //ios->bytes_per_channel = 2;
    
    ios->dsp_context = freerdp_dsp_context_new();
    
    pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)ios);
    ios->cdvc = ((RDP_PLUGIN_DATA *)pEntryPoints->plugin_data)[0].data[1];
    
    ios->context = iOSSoundAllocAndInit();
    if (!ios->context)
    {
        LLOGLN(0, ("rdpsnd_iOS: iossound context alloc/init failed"));
        rdpsnd_ios_free(&ios->device);
    }

	return 0;
}

