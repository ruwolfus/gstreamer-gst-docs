#include <gst/gst.h>

/* GLOBAL SETTINGS */
//#define INSERTBREAKS //pause the pipline frequently and rise the frequence in steps
#define RTPAUDIOSINK //output is RTP otherwise local speaker
//#define AUDIOSOURCE //input is audiotestsrc
#define URISOURCE //input is uridecodebin
//#define UDPSOURCE //input is a RTP source

#define OUTVOLUME 1.0 //100%
//#define OUTVOLUME 2.0
//#define OUTVOLUME 0.04 //low
#define STARTFREQUENCY 220.0f
#ifdef RTPAUDIOSINK
#define CONFIGLOCALBINDPORT 16000
#define CONFIGEXTPORT 2042
#define CONFIGEXTHOST "192.168.252.12"
#define SSRC 1
#endif
#ifdef URISOURCE
//#define URISOURCEURL "http://swr-swr1-bw.cast.addradio.de/swr/swr1/bw/mp3/128/stream.mp3"
#define URISOURCEURL "http://stream.schwarzwaldradio.com/schwarzwaldradio"
#endif
#ifdef INSERTBREAKS
#define INSERTBREAKSCOUNT 100
#define INSERTBREAKSTIMEMS 10000 //milliseconds
#endif

#ifdef RTPAUDIOSINK
int configlocalbindport;
int configextport;
char *configexthost;
#endif

#if !defined(AUDIOSOURCE) && !defined(URISOURCE) && !defined(UDPSOURCE)
Mist gebaut.
#endif

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
	GstElement *pipeline;
#ifdef AUDIOSOURCE
	GstElement *audiosource;
#elif defined(URISOURCE)
	GstElement *urisource;
#elif defined(UDPSOURCE)
	GstElement *udpsource, *rtpptdemux, *rtppcmadepay, *alawdec;
	GstElement *rtpdtmfdepay;
	GstElement *fakesink;
#endif
	GstElement *tee;
	GstElement *audioqueue, *audioconvert, *audioresample, *volume;
	GstElement *videoqueue, *visual, *videoconvert, *videosink;
#ifdef RTPAUDIOSINK
	GstElement *alawenc, *rtppcmapay, *udpsink;
#else
	GstElement *autoaudiosink;
#endif

	GMainLoop *main_loop;  /* GLib's Main Loop */
} CustomData;

/* Handler for the pad-added signal */
#ifdef URISOURCE
static void pad_added_handler_urisource(GstElement *src, GstPad *pad, CustomData *data);
#elif defined(UDPSOURCE)
static void pad_added_handler_rtpptdemux(GstElement *src, GstPad *pad, CustomData *data);
static void new_payload_found(GstElement * element, guint pt, GstPad * new_pad, CustomData *data);
#endif

int main(int argc, char *argv[]) {
	CustomData data;
	GstBus *bus;
	GstMessage *msg;
	GstPadTemplate *tee_src_pad_template;
	GstPad *tee_audio_pad, *tee_video_pad;
	GstPad *queue_audio_pad, *queue_video_pad;

#ifdef RTPAUDIOSINK
	configlocalbindport = CONFIGLOCALBINDPORT;
	configextport = CONFIGEXTPORT;
	configexthost = CONFIGEXTHOST;
#endif

	/* Initialize GStreamer */
	gst_init(&argc, &argv);

	/* Create the elements */
#ifdef AUDIOSOURCE
	data.audiosource = gst_element_factory_make("audiotestsrc", "audiosource");
#elif defined(URISOURCE)
	data.urisource = gst_element_factory_make("uridecodebin", "urisource");
#endif
	data.tee = gst_element_factory_make("tee", "tee");
	data.audioqueue = gst_element_factory_make("queue", "audioqueue");
	data.audioconvert = gst_element_factory_make("audioconvert", "audioconvert");
	data.videoqueue = gst_element_factory_make("queue", "videoqueue");
	data.visual = gst_element_factory_make("wavescope", "visual");
	//data.visual = gst_element_factory_make("spacescope", "visual");
	//data.visual = gst_element_factory_make("spectrascope", "visual");
	//data.visual = gst_element_factory_make("synaescope", "visual");
	data.videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
	data.videosink = gst_element_factory_make("autovideosink", "videosink");
	data.audioresample = gst_element_factory_make("audioresample", "audioresample");
	data.volume = gst_element_factory_make("volume", "volume");
#ifdef RTPAUDIOSINK
	data.alawenc = gst_element_factory_make("alawenc", "alawenc");
	data.rtppcmapay = gst_element_factory_make("rtppcmapay", "rtppcmapay");
	data.udpsink = gst_element_factory_make("udpsink", "udpsink");
#else
	data.autoaudiosink = gst_element_factory_make("autoaudiosink", "autoaudiosink");
#endif

	/* Create the empty pipeline */
	data.pipeline = gst_pipeline_new("test-pipeline");

#ifdef RTPAUDIOSINK
	if (!data.pipeline ||
#ifdef AUDIOSOURCE
		!data.audiosource ||
#elif defined(URISOURCE)
		!data.urisource ||
#endif
		!data.tee || !data.audioqueue || !data.audioconvert || !data.audioresample || !data.volume ||
		!data.videoqueue || !data.visual || !data.videoconvert || !data.videosink ||
		!data.alawenc || !data.rtppcmapay || !data.udpsink) {
		g_printerr("Not all elements could be created.\n");
		return -1;
	}
#else
	if (!data.pipeline ||
#ifdef AUDIOSOURCE
		!data.audiosource ||
#elif defined(URISOURCE)
		!data.urisource ||
#endif
		!data.tee || !data.audioqueue || !data.audioconvert || !data.audioresample || !data.volume || !data.autoaudiosink ||
		!data.videoqueue || !data.visual || !data.videoconvert || !data.videosink) {
		g_printerr("Not all elements could be created.\n");
		return -1;
	}
#endif

	/* Configure elements */
#ifdef AUDIOSOURCE
	//g_object_set(audiosource, "wave", GST_AUDIO_TEST_SRC_WAVE_TICKS, NULL);
	g_object_set(data.audiosource, "freq", STARTFREQUENCY, NULL);
#endif
	/*
	wavescope+spacescope: "style": STYLE_DOTS=0,STYLE_LINES,STYLE_COLOR_DOTS,STYLE_COLOR_LINES
	all:
	"shader":
	typedef enum {
	GST_AUDIO_VISUALIZER_SHADER_NONE=0,
	GST_AUDIO_VISUALIZER_SHADER_FADE, //DEFAULT_SHADER
	GST_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_UP,
	GST_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_DOWN,
	GST_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_LEFT,
	GST_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_RIGHT,
	GST_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_HORIZ_OUT,
	GST_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_HORIZ_IN,
	GST_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_VERT_OUT,
	GST_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_VERT_IN
	} GstAudioVisualizerShader;
	"shade-amount": DEFAULT_SHADE_AMOUNT 0x000a0a0a other:0x00040302 0x00080402
	*/
	g_object_set(data.visual, "style", 3, NULL);
	g_object_set(data.visual, "shader", 6, NULL);
	g_object_set(data.visual, "shade-amount", 0x00080402, NULL);
	g_object_set(data.volume, "volume", OUTVOLUME, NULL);
#ifdef URISOURCE
	/* Set the URI to play */
	g_object_set(data.urisource, "uri", URISOURCEURL, NULL);
#endif
#ifdef RTPAUDIOSINK
	g_object_set(data.rtppcmapay, "ssrc", SSRC, NULL);
	g_object_set(data.udpsink, "host", configexthost, "bind-port", configlocalbindport, "port", configextport, NULL);
#endif

	/* Link all elements that can be automatically linked because they have "Always" pads */
#ifdef RTPAUDIOSINK
	gst_bin_add_many(GST_BIN(data.pipeline),
#ifdef AUDIOSOURCE
		data.audiosource,
#elif defined(URISOURCE)
		data.urisource,
#endif
		data.tee, data.audioqueue, data.audioconvert, data.audioresample, data.volume, data.alawenc, data.rtppcmapay, data.udpsink,
		data.videoqueue, data.visual, data.videoconvert, data.videosink, NULL);
	if (
#ifdef AUDIOSOURCE
		gst_element_link_many(data.audiosource, data.tee, NULL) != TRUE ||
#endif
		gst_element_link_many(data.audioqueue, data.audioconvert, data.audioresample, data.volume, data.alawenc, data.rtppcmapay, data.udpsink, NULL) != TRUE ||
		gst_element_link_many(data.videoqueue, data.visual, data.videoconvert, data.videosink, NULL) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}
#else
	gst_bin_add_many(GST_BIN(data.pipeline),
#ifdef AUDIOSOURCE
		data.audiosource,
#elif defined(URISOURCE)
		data.urisource,
#endif
		data.tee, data.audioqueue, data.audioconvert, data.audioresample, data.volume, data.autoaudiosink,
		data.videoqueue, data.visual, data.videoconvert, data.videosink, NULL);
	if (
#ifdef AUDIOSOURCE
		gst_element_link_many(data.audiosource, data.tee, NULL) != TRUE ||
#endif
		gst_element_link_many(data.audioqueue, data.audioconvert, data.audioresample, data.volume, data.autoaudiosink, NULL) != TRUE ||
		gst_element_link_many(data.videoqueue, data.visual, data.videoconvert, data.videosink, NULL) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}
#endif

	/* Manually link the Tee, which has "Request" pads */
	tee_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(data.tee), "src_%u");
	tee_audio_pad = gst_element_request_pad(data.tee, tee_src_pad_template, NULL, NULL);
	g_print("Obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
	queue_audio_pad = gst_element_get_static_pad(data.audioqueue, "sink");
	tee_video_pad = gst_element_request_pad(data.tee, tee_src_pad_template, NULL, NULL);
	g_print("Obtained request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
	queue_video_pad = gst_element_get_static_pad(data.videoqueue, "sink");
	if (gst_pad_link(tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
		gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK) {
		g_printerr("Tee could not be linked.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}
	gst_object_unref(queue_audio_pad);
	gst_object_unref(queue_video_pad);

#ifdef URISOURCE
	/* Connect to the pad-added signal */
	g_signal_connect(data.urisource, "pad-added", G_CALLBACK(pad_added_handler_urisource), &data);
#endif

	/* Start playing the pipeline */
	gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

	/* Wait until error or EOS */
	bus = gst_element_get_bus(data.pipeline);
#ifdef INSERTBREAKS
	for (int i = 0; i < INSERTBREAKSCOUNT; i++)
	{
		g_print("i:%d\n", i);
		msg = gst_bus_timed_pop_filtered(bus, (INSERTBREAKSTIMEMS * GST_MSECOND), GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
#ifdef AUDIOSOURCE
		g_object_set(data.audiosource, "freq", STARTFREQUENCY + ((i+1) * 5), NULL);
#endif
		gst_element_set_state(data.pipeline, GST_STATE_NULL);
		gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
	}
#else
	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
#endif


	/* Release the request pads from the Tee, and unref them */
	gst_element_release_request_pad(data.tee, tee_audio_pad);
	gst_element_release_request_pad(data.tee, tee_video_pad);
	gst_object_unref(tee_audio_pad);
	gst_object_unref(tee_video_pad);

	/* Free resources */
	if (msg != NULL)
		gst_message_unref(msg);
	gst_object_unref(bus);
	gst_element_set_state(data.pipeline, GST_STATE_NULL);

	gst_object_unref(data.pipeline);
	return 0;
}

#ifdef URISOURCE
/* This function will be called by the pad-added signal */
static void pad_added_handler_urisource(GstElement *src, GstPad *new_pad, CustomData *data) {
	GstPad *sink_pad = gst_element_get_static_pad (data->tee, "sink");
	GstPadLinkReturn ret;
	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;

	g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

	/* If our converter is already linked, we have nothing to do here */
	if (gst_pad_is_linked(sink_pad)) {
		g_print("  We are already linked. Ignoring.\n");
		goto exit;
	}

	/* Check the new pad's type */
	new_pad_caps = gst_pad_query_caps(new_pad, NULL);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	new_pad_type = gst_structure_get_name(new_pad_struct);
	if (!g_str_has_prefix(new_pad_type, "audio/x-raw")) {
		g_print("  It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
		goto exit;
	}

	/* Attempt the link */
	ret = gst_pad_link(new_pad, sink_pad);
	if (GST_PAD_LINK_FAILED(ret)) {
		g_print("  Type is '%s' but link failed.\n", new_pad_type);
	}
	else {
		g_print("  Link succeeded (type '%s').\n", new_pad_type);
	}

exit:
	/* Unreference the new pad's caps, if we got them */
	if (new_pad_caps != NULL)
		gst_caps_unref(new_pad_caps);

	/* Unreference the sink pad */
	gst_object_unref(sink_pad);
}
#endif

#if 0
gst-launch-1.0 -v uridecodebin uri="http://stream.schwarzwaldradio.com/schwarzwaldradio"  !audioconvert !audioresample !alawenc !rtppcmapay ssrc=10 !application/x-rtp, payload=8, rate=8000 !udpsink host=192.168.252.12 bind-port=%localport% port=%extport%
gst-launch-1.0 -v uridecodebin audiotestsrc ! tee name=t-audio-wave ! queue ! audioconvert ! wavescope ! videoconvert ! autovideosink t-audio-wave. ! queue ! audioconvert ! autoaudiosink

#endif


