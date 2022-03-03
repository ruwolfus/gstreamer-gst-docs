#include <gst/gst.h>
#include "rtp-test-empfaenger.h"
#include <gst/audio/audio.h>
#include <string.h>

//#define AUDIOSOURCE
//#define URISOURCE
#define UDPSOURCE

#if !defined(AUDISOURCE) && !defined(URISOURCE) && !defined(UDPSOURCE)
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
	GstElement *tee_audio_sink_visual;
	GstElement *audio_queue, *audio_convert, *volume, *autoaudiosink;
	GstElement *visual_queue, *visual_audio_convert, *visual, *video_convert, *video_sink;

	GMainLoop *main_loop;  /* GLib's Main Loop */
} CustomData;

/* Handler for the pad-added signal */
#ifdef URISOURCE
static void pad_added_handler_urisource(GstElement *src, GstPad *pad, CustomData *data);
#elif defined(UDPSOURCE)
static void pad_added_handler_rtpptdemux(GstElement *src, GstPad *pad, CustomData *data);
static void new_payload_found(GstElement * element, guint pt, GstPad * new_pad, CustomData *data);
#endif

/* This function is called when an error message is posted on the bus */
static void error_cb(GstBus *bus, GstMessage *msg, CustomData *data) {
	GError *err;
	gchar *debug_info;

	/* Print error details on the screen */
	gst_message_parse_error(msg, &err, &debug_info);
	g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
	g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
	g_clear_error(&err);
	g_free(debug_info);

	//g_main_loop_quit(data->main_loop);
}

int main(int argc, char *argv[]) {
	CustomData data;
	GstBus *bus;

	//tee_audio_sink_visual
	GstPadTemplate *tee_audio_sink_visual_src_pad_template;
	GstPad *tee_audio_sink_visual_audio_pad, *tee_audio_sink_visual_visual_pad;
	GstPad *queue_audio_pad, *queue_visual_pad;

	/* Initialize cumstom data structure */
	memset(&data, 0, sizeof(data));

	/* Initialize GStreamer */
	gst_init(&argc, &argv);

	/* Create the empty pipeline */
	data.pipeline = gst_pipeline_new("rtp-rx-pipeline");

	/* Create the elements */
#ifdef AUDIOSOURCE
	data.audiosource = gst_element_factory_make("audiotestsrc", "audio_source");
#elif defined(URISOURCE)
	data.urisource = gst_element_factory_make("uridecodebin", "urisource");
#elif defined(UDPSOURCE)
	data.udpsource = gst_element_factory_make("udpsrc", "udpsource");
	data.rtpptdemux = gst_element_factory_make("rtpptdemux", "rtpptdemux");
	data.rtppcmadepay = gst_element_factory_make("rtppcmadepay", "rtppcmadepay");
	data.alawdec = gst_element_factory_make("alawdec", "alawdec");
	data.rtpdtmfdepay = gst_element_factory_make("rtpdtmfdepay", "rtpdtmfdepay");
	data.fakesink = gst_element_factory_make("fakesink", "fakesink");
#endif
	data.tee_audio_sink_visual = gst_element_factory_make("tee", "tee_audio_speaker_visual");

	data.audio_queue = gst_element_factory_make("queue", "audio_queue");
	data.audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
	data.volume = gst_element_factory_make("volume", "audio_volume");
	data.autoaudiosink = gst_element_factory_make("autoaudiosink", "autoaudiosink");

	data.visual_queue = gst_element_factory_make("queue", "visual_queue");
	data.visual_audio_convert = gst_element_factory_make("audioconvert", "visual_audio_convert");
	data.visual = gst_element_factory_make("wavescope", "visual");
	data.video_convert = gst_element_factory_make("videoconvert", "video_convert");
	data.video_sink = gst_element_factory_make("autovideosink", "video_sink");

	if (!data.pipeline ||
#ifdef AUDIOSOURCE
		!data.audiosource ||
#elif defined(URISOURCE)
		!data.urisource ||
#elif defined(UDPSOURCE)
		!data.udpsource ||
		!data.rtpptdemux ||
		!data.rtppcmadepay ||
		!data.alawdec ||
		!data.rtpdtmfdepay ||
		!data.fakesink ||
#endif
		!data.tee_audio_sink_visual ||
		!data.audio_queue || !data.audio_convert || !data.volume || !data.autoaudiosink ||
		!data.visual_queue || !data.visual_audio_convert || !data.visual || !data.video_convert || !data.video_sink) {
		g_printerr("Not all elements could be created.\n");
		return -1;
	}

	/* Configure wavescope */
	g_object_set(data.visual, "shader", 0, "style", 0, NULL);
#ifdef URISOURCE
	/* Set the URI to play */
	g_object_set(data.urisource, "uri", "http://swr-swr1-bw.cast.addradio.de/swr/swr1/bw/mp3/128/stream.mp3", NULL);
#endif
#ifdef UDPSOURCE
	g_object_set(data.udpsource, "port", 9001, NULL);
	{
		GstCaps *caps = gst_caps_new_simple("application/x-rtp",
			"media", G_TYPE_STRING, "audio",
			"payload", G_TYPE_INT, 8,
			"payload", G_TYPE_INT, 101,
			"clock-rate", G_TYPE_INT, 8000,
			NULL);
		g_object_set(data.udpsource, "caps", caps, NULL);
		gst_caps_unref(caps);
	}
#endif
	/* misc config */
	g_object_set(data.volume, "volume", 0.3, NULL);

	/* Link all elements that can be automatically linked because they have "Always" pads */
#ifdef AUDIOSOURCE
	gst_bin_add_many(GST_BIN(data.pipeline),
		data.audiosource,
		data.tee_audio_sink_visual,
		data.audio_queue, data.audio_convert, data.volume, data.autoaudiosink,
		data.visual_queue, data.visual_audio_convert, data.visual, data.video_convert, data.video_sink, NULL);
	if (gst_element_link_many(data.audiosource, data.tee_audio_sink_visual, NULL) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}
#elif defined(URISOURCE)
	gst_bin_add_many(GST_BIN(data.pipeline),
		data.urisource,
		data.tee_audio_sink_visual,
		data.audio_queue, data.audio_convert, data.volume, data.autoaudiosink,
		data.visual_queue, data.visual_audio_convert, data.visual, data.video_convert, data.video_sink, NULL);
#elif defined(UDPSOURCE)
	gst_bin_add_many(GST_BIN(data.pipeline),
		data.udpsource, data.rtpptdemux, data.rtppcmadepay, data.alawdec, data.rtpdtmfdepay,
		data.tee_audio_sink_visual,
		data.audio_queue, data.audio_convert, data.volume, data.autoaudiosink,
		data.visual_queue, data.visual_audio_convert, data.visual, data.video_convert, data.video_sink, NULL);
	if (gst_element_link_many(data.udpsource, data.rtpptdemux, NULL) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}
	if (gst_element_link_many(data.rtppcmadepay, data.alawdec,
		data.tee_audio_sink_visual, NULL) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}
#endif
	if (gst_element_link_many(data.audio_queue, data.audio_convert, data.volume, data.autoaudiosink, NULL) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}
	if (gst_element_link_many (data.visual_queue, data.visual_audio_convert, data.visual, data.video_convert, data.video_sink, NULL) != TRUE) {
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (data.pipeline);
		return -1;
	}

	/* Manually link the tee_audio_sink_visual, which has "Request" pads */
	tee_audio_sink_visual_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(data.tee_audio_sink_visual), "src_%u");
	tee_audio_sink_visual_audio_pad = gst_element_request_pad(data.tee_audio_sink_visual, tee_audio_sink_visual_src_pad_template, NULL, NULL);
	g_print("Obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_sink_visual_audio_pad));
	queue_audio_pad = gst_element_get_static_pad(data.audio_queue, "sink");
	tee_audio_sink_visual_visual_pad = gst_element_request_pad(data.tee_audio_sink_visual, tee_audio_sink_visual_src_pad_template, NULL, NULL);
	g_print("Obtained request pad %s for video branch.\n", gst_pad_get_name(tee_audio_sink_visual_visual_pad));
	queue_visual_pad = gst_element_get_static_pad(data.visual_queue, "sink");
	if (gst_pad_link(tee_audio_sink_visual_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
		gst_pad_link(tee_audio_sink_visual_visual_pad, queue_visual_pad) != GST_PAD_LINK_OK) {
		g_printerr("Tee could not be linked.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}
	gst_object_unref(queue_audio_pad);
	gst_object_unref(queue_visual_pad);

	/* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
	bus = gst_element_get_bus(data.pipeline);
	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, &data);
	gst_object_unref(bus);

#ifdef URISOURCE
	/* Connect to the pad-added signal */
	g_signal_connect(data.urisource, "pad-added", G_CALLBACK(pad_added_handler_urisource), &data);
#endif
#ifdef UDPSOURCE
	/* Connect to the pad-added signal */
	//g_signal_connect(data.rtpptdemux, "pad-added", G_CALLBACK(pad_added_handler_rtpptdemux), &data);
	g_signal_connect(data.rtpptdemux, "new-payload-type", G_CALLBACK(new_payload_found), &data);
#endif

	/* Start playing the pipeline */
	gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

	/* Create a GLib Main Loop and set it to run */
	data.main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(data.main_loop);

	/* Release the request pads from the tee_audio_sink_visual, and unref them */
	gst_element_release_request_pad(data.tee_audio_sink_visual, tee_audio_sink_visual_audio_pad);
	gst_element_release_request_pad(data.tee_audio_sink_visual, tee_audio_sink_visual_visual_pad);
	gst_object_unref(tee_audio_sink_visual_audio_pad);
	gst_object_unref(tee_audio_sink_visual_visual_pad);

	/* Free resources */
	gst_element_set_state(data.pipeline, GST_STATE_NULL);
	gst_object_unref(data.pipeline);
	return 0;
}

#ifdef URISOURCE
/* This function will be called by the pad-added signal */
static void pad_added_handler_urisource(GstElement *src, GstPad *new_pad, CustomData *data) {
	GstPad *sink_pad = gst_element_get_static_pad (data->tee_audio_sink_visual, "sink");
	GstPadLinkReturn ret;
	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;

	g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

	/* If our converter is already linked, we have nothing to do here */
	if (gst_pad_is_linked (sink_pad)) {
		g_print ("  We are already linked. Ignoring.\n");
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
#ifdef UDPSOURCE
/* This function will be called by the pad-added signal */
static void pad_added_handler_rtpptdemux(GstElement *src, GstPad *new_pad, CustomData *data) {
	GstPad *sink_pad = gst_element_get_static_pad (data->rtppcmadepay, "sink");
	GstPadLinkReturn ret;
	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;

	g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

	/* Check the new pad's type */
	new_pad_caps = gst_pad_query_caps(new_pad, NULL);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	new_pad_type = gst_structure_get_name(new_pad_struct);
	if (!g_str_has_prefix(new_pad_type, "application/x-rtp")) {
		g_print("  It has type '%s' which is not raw rtp. Ignoring.\n", new_pad_type);
		if (new_pad_caps != NULL) gst_caps_unref(new_pad_caps);
		gst_object_unref(sink_pad);
		return;
	}
	if (new_pad_caps != NULL) gst_caps_unref(new_pad_caps);

	/* If our converter is already linked, we have nothing to do here */
	if (gst_pad_is_linked (sink_pad)) {
		g_print ("  We are already linked. Ignoring.\n");
		gst_object_unref(sink_pad);
		return;
	}

	/* Attempt the link */
	ret = gst_pad_link(new_pad, sink_pad);
	if (GST_PAD_LINK_FAILED(ret)) {
		g_print("  Type is '%s' but link failed.\n", new_pad_type);
	}
	else {
		g_print("  Link succeeded (type '%s').\n", new_pad_type);
	}

	/* Unreference the sink pad */
	gst_object_unref(sink_pad);
}

static void new_payload_found(GstElement * element, guint pt, GstPad * new_pad, CustomData *data)
{
	GstPad *sink_pad = NULL;
	GstPadLinkReturn ret;

	g_print("Received new pad '%s' with pt:%d\n", GST_PAD_NAME(new_pad), pt);
	switch (pt)
	{
	case 8: //pcma
		sink_pad = gst_element_get_static_pad(data->rtppcmadepay, "sink");
		break;
	case 101: //dtmf
#if 1
		sink_pad = gst_element_get_static_pad(data->rtpdtmfdepay, "sink");
#else
		sink_pad = gst_element_get_static_pad(data->fakesink, "sink");
		g_object_set(data->fakesink, "async", FALSE, "sync", FALSE, NULL);
		gst_pad_link(new_pad, sink_pad);
		gst_element_sync_state_with_parent(data->fakesink);
		gst_object_unref(sink_pad);
		sink_pad = NULL;
#endif
		break;
	default:
		break;
	}

	if (sink_pad == NULL)
	{
		g_print("  sink_pad is NULL\n");
		return;
	}

	/* If our converter is already linked, we have nothing to do here */
	if (gst_pad_is_linked (sink_pad)) {
		g_print ("  We are already linked. Ignoring.\n");
		gst_object_unref(sink_pad);
		return;
	}

	switch (pt)
	{
	case 8: //pcma
		break;
	case 101: //dtmf
		{
			GstPad *sink_pad = gst_element_get_static_pad(data->fakesink, "sink");
			GstPad *src_pad = gst_element_get_static_pad(data->rtpdtmfdepay, "src");

			g_object_set(data->fakesink, "async", FALSE, "sync", FALSE, NULL);
			gst_bin_add(GST_BIN(data->pipeline), data->fakesink);
			gst_pad_link(src_pad, sink_pad);
			//gst_element_link_pads(data->rtpdtmfdepay, "src", data->fakesink, "sink");
			gst_element_sync_state_with_parent(data->fakesink);
			gst_object_unref(sink_pad);
			gst_object_unref(src_pad);
		}
		gst_pad_link(new_pad, sink_pad);
		gst_element_sync_state_with_parent(data->rtpdtmfdepay);
		gst_object_unref(sink_pad);
		return;
		break;
	default:
		break;
	}

	/* Attempt the link */
	ret = gst_pad_link(new_pad, sink_pad);
	if (GST_PAD_LINK_FAILED(ret)) {
		g_print("  link failed.\n");
	}
	else {
		g_print("  Link succeeded.\n");
	}

	/* Unreference the sink pad */
	gst_object_unref(sink_pad);
}
#endif

#if 0
#empfaenger:
gst-launch-1.0 -v uridecodebin uri="http://swr-swr1-bw.cast.addradio.de/swr/swr1/bw/mp3/128/stream.mp3" ! queue ! audioconvert ! autoaudiosink
"http://swr-swr1-bw.cast.addradio.de/swr/swr1/bw/mp3/128/stream.mp3"
"http://swr-swr1-bw.cast.addradio.de/swr/swr1/bw/mp3/64/stream.mp3"
#ifdef AUDIOSOURCE
gst-launch-1.0 -v audiotestsrc ! tee name=t-audio-wave ! queue ! audioconvert ! wavescope ! videoconvert ! autovideosink t-audio-wave. ! queue ! audioconvert ! volume volume=0.3 ! autoaudiosink
#endif
#ifdef URISOURCE
gst-launch-1.0 -v uridecodebin uri="http://swr-swr1-bw.cast.addradio.de/swr/swr1/bw/mp3/64/stream.mp3" ! tee name=t-audio-wave ! queue ! audioconvert ! wavescope ! videoconvert ! autovideosink t-audio-wave. ! queue ! audioconvert ! volume volume=0.3 ! autoaudiosink
#endif

gst-launch-1.0 -v udpsrc port=9001 caps="application/x-rtp, media=(string)audio, clock-rate=(int)8000" ! rtpptdemux ! rtppcmadepay ! alawdec ! queue ! audioconvert ! autoaudiosink
gst-launch-1.0 -v udpsrc port=9001 caps="application/x-rtp, media=(string)audio, clock-rate=(int)8000" ! rtpptdemux ! rtppcmadepay ! alawdec ! tee name=t-audio-wave ! queue ! audioconvert ! wavescope ! videoconvert ! autovideosink t-audio-wave. ! queue ! audioconvert ! volume volume=0.3 ! autoaudiosink


#sender:
gst-launch-1.0 -v autoaudiosrc ! audioconvert ! alawenc ! rtppcmapay ! udpsink host=10.21.3.32 port=8000

linked udpsrc0:src and rtpbin1:recv_rtp_sink_0
linked recv_rtp_sink_0:proxypad16 and rtpsession1:recv_rtp_sink
linked rtpbin1:send_rtp_src_0 and multiudpsink0:sink
linked sink_0:proxypad11 and rtprtxqueue1:sink
linked rtprtxqueue1:src and src_0:proxypad12
linked bin1:src_0 and rtpsession1:send_rtp_sink
linked rtpsession1:send_rtp_src and send_rtp_src_0:proxypad13
linked rtpsession1:recv_rtp_src and rtpssrcdemux2:sink
linked rtpssrcdemux2:src_633726822 and rtpjitterbuffer0:sink
linked rtpjitterbuffer0:src and rtpptdemux0:sink
linked rtpptdemux0:src_8 and recv_rtp_src_0_633726822_8:proxypad19
linked rtpbin1:recv_rtp_src_0_633726822_8 and rtppcmadepay0:sink
linked rtppcmadepay0:src and kmsagnosticbin2 - 0:sink
#endif