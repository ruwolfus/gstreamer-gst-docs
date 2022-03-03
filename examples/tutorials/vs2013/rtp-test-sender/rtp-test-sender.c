#include <gst/gst.h>
//#include "../../../../../../../gstreamer/gst-plugins-base/gst/audiotestsrc/gstaudiotestsrc.h"

#define RTPAUDIOSINK

#ifdef RTPAUDIOSINK
int configlocalbindport;
int configextport;
char *configexthost;
#endif

int main(int argc, char *argv[]) {
	GstElement *pipeline, *audio_source, *tee, *audio_queue, *audio_convert, *audio_resample;
	GstElement *video_queue, *visual, *video_convert, *video_sink;
#ifdef RTPAUDIOSINK
	GstElement *alawenc, *rtppcmapay, *udpsink;
#else
	GstElement *audio_sink;
#endif
	GstBus *bus;
	GstMessage *msg;
	GstPadTemplate *tee_src_pad_template;
	GstPad *tee_audio_pad, *tee_video_pad;
	GstPad *queue_audio_pad, *queue_video_pad;

#ifdef RTPAUDIOSINK
	configlocalbindport = 16000;
	configextport = 2042;
	configexthost = "192.168.252.12";
#endif

	/* Initialize GStreamer */
	gst_init(&argc, &argv);

	/* Create the elements */
	audio_source = gst_element_factory_make("audiotestsrc", "audio_source");
	tee = gst_element_factory_make("tee", "tee");
	audio_queue = gst_element_factory_make("queue", "audio_queue");
	audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
	video_queue = gst_element_factory_make("queue", "video_queue");
	visual = gst_element_factory_make("wavescope", "visual");
	//visual = gst_element_factory_make("spectrascope", "visual");
	//visual = gst_element_factory_make("synaescope", "visual");  
	video_convert = gst_element_factory_make("videoconvert", "video_convert");
	video_sink = gst_element_factory_make("autovideosink", "video_sink");
	audio_resample = gst_element_factory_make("audioresample", "audio_resample");
#ifdef RTPAUDIOSINK
	alawenc = gst_element_factory_make("alawenc", "alawenc");
	rtppcmapay = gst_element_factory_make("rtppcmapay", "rtppcmapay");
	udpsink = gst_element_factory_make("udpsink", "udpsink");
#else
	audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");
#endif

	/* Create the empty pipeline */
	pipeline = gst_pipeline_new("test-pipeline");

#ifdef RTPAUDIOSINK
	if (!pipeline || !audio_source || !tee || !audio_queue || !audio_convert || !audio_resample ||
		!video_queue || !visual || !video_convert || !video_sink ||
		!alawenc || !rtppcmapay || !udpsink) {
		g_printerr("Not all elements could be created.\n");
		return -1;
	}
#else
	if (!pipeline || !audio_source || !tee || !audio_queue || !audio_convert || !audio_resample || !audio_sink ||
		!video_queue || !visual || !video_convert || !video_sink) {
		g_printerr("Not all elements could be created.\n");
		return -1;
	}
#endif

	/* Configure elements */
	//g_object_set (audio_source, "freq", 215.0f, NULL);
	//g_object_set(audio_source, "wave", GST_AUDIO_TEST_SRC_WAVE_TICKS, NULL);
	g_object_set(audio_source, "freq", 220.0f, NULL);
	g_object_set(visual, "shader", 0, "style", 3, NULL);
#ifdef RTPAUDIOSINK
	g_object_set(rtppcmapay, "ssrc", 1, NULL);
	g_object_set(udpsink, "host", configexthost, "bind-port", configlocalbindport, "port", configextport, NULL);
#endif

	/* Link all elements that can be automatically linked because they have "Always" pads */
#ifdef RTPAUDIOSINK
	gst_bin_add_many(GST_BIN(pipeline), audio_source, tee, audio_queue, audio_convert, audio_resample, alawenc, rtppcmapay, udpsink,
		video_queue, visual, video_convert, video_sink, NULL);
	if (gst_element_link_many(audio_source, tee, NULL) != TRUE ||
		gst_element_link_many(audio_queue, audio_convert, audio_resample, alawenc, rtppcmapay, udpsink, NULL) != TRUE ||
		gst_element_link_many(video_queue, visual, video_convert, video_sink, NULL) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(pipeline);
		return -1;
	}
#else
	gst_bin_add_many(GST_BIN(pipeline), audio_source, tee, audio_queue, audio_convert, audio_resample, audio_sink,
		video_queue, visual, video_convert, video_sink, NULL);
	if (gst_element_link_many(audio_source, tee, NULL) != TRUE ||
		gst_element_link_many(audio_queue, audio_convert, audio_resample, audio_sink, NULL) != TRUE ||
		gst_element_link_many(video_queue, visual, video_convert, video_sink, NULL) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(pipeline);
		return -1;
	}
#endif

	/* Manually link the Tee, which has "Request" pads */
	tee_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(tee), "src_%u");
	tee_audio_pad = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);
	g_print("Obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
	queue_audio_pad = gst_element_get_static_pad(audio_queue, "sink");
	tee_video_pad = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);
	g_print("Obtained request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
	queue_video_pad = gst_element_get_static_pad(video_queue, "sink");
	if (gst_pad_link(tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
		gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK) {
		g_printerr("Tee could not be linked.\n");
		gst_object_unref(pipeline);
		return -1;
	}
	gst_object_unref(queue_audio_pad);
	gst_object_unref(queue_video_pad);

	/* Start playing the pipeline */
	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	/* Wait until error or EOS */
	bus = gst_element_get_bus(pipeline);
#if 1
	for (int i = 0; i < 100; i++)
	{
		g_print("i:%d\n", i);
		msg = gst_bus_timed_pop_filtered(bus, (800 * GST_MSECOND), GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		g_object_set(audio_source, "freq", 240.0f+(i*5), NULL);
		gst_element_set_state(pipeline, GST_STATE_NULL);
		gst_element_set_state(pipeline, GST_STATE_PLAYING);	
	}
#else
	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
#endif


	/* Release the request pads from the Tee, and unref them */
	gst_element_release_request_pad(tee, tee_audio_pad);
	gst_element_release_request_pad(tee, tee_video_pad);
	gst_object_unref(tee_audio_pad);
	gst_object_unref(tee_video_pad);

	/* Free resources */
	if (msg != NULL)
		gst_message_unref(msg);
	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);

	gst_object_unref(pipeline);
	return 0;
}

#if 0
gst-launch-1.0 -v uridecodebin uri="http://stream.schwarzwaldradio.com/schwarzwaldradio"  !audioconvert !audioresample !alawenc !rtppcmapay ssrc=10 !application/x-rtp, payload=8, rate=8000 !udpsink host=192.168.252.12 bind-port=%localport% port=%extport%
gst-launch-1.0 -v uridecodebin audiotestsrc ! tee name=t-audio-wave ! queue ! audioconvert ! wavescope ! videoconvert ! autovideosink t-audio-wave. ! queue ! audioconvert ! autoaudiosink

#endif


