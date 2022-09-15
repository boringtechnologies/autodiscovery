#include <iterator>
#include <memory>
#include <thread>

#include <sync.hpp>
#include "internal.hpp"

static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    GMainLoop *loop = (GMainLoop *)data;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(loop);
            break;

        case GST_MESSAGE_ERROR: {
            gchar *debug;
            GError *error;

            gst_message_parse_error(msg, &error, &debug);
            g_free(debug);

            g_printerr("Error: %s\n", error->message);
            g_error_free(error);

            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }

    return TRUE;
}

static GstElement *get_audio_device(const media::CaptureDevice &device) {
    GstElement *source;

#if defined(__linux__)
    MAKE_ELEMENT(source, "pulsesrc", "publisher-audiosrc");
#elif defined(__APPLE__)
    MAKE_ELEMENT(source, "osxaudiosrc", "publisher-audiosrc");
#endif

    return source;
}

static GstElement *get_video_device(const media::CaptureDevice &device) {
    GstElement *source;

#if defined(__linux__)
    MAKE_ELEMENT(source, "v4l2src", "publisher-videosrc");
    if (!device.name.empty()) {
        g_object_set(G_OBJECT(source), "device", device.name.c_str(), nullptr);
    }

#elif defined(__APPLE__)
    MAKE_ELEMENT(source, "avfvideosrc", "publisher-audiosrc");
#endif

    return source;
}

media::Sender::Sender(const std::string &id, media::MediaType type,
                      const media::CaptureDevice &device,
                      const media::ClockInfo &clock)
    : thread(nullptr) {
    GstBus *bus;
    GstElement *source, *conv, *capsf, *encoder, *payloader, *queue, *fakesink;
    gst_init(nullptr, nullptr);

    loop = g_main_loop_new(nullptr, FALSE);

    pipeline = gst_pipeline_new(id.c_str());

    if (type == media::MediaType::Audio) {
        source = get_audio_device(device);
        MAKE_ELEMENT(conv, "audioconvert", "conv");
        MAKE_ELEMENT(capsf, "capsfilter", "capsf");
        g_object_set(capsf, "caps",
                     gst_caps_from_string("audio/x-raw,rate=48000"), nullptr);
        MAKE_ELEMENT(encoder, "opusenc", "publisher-audioenc");
        payloader =
            gst_element_factory_make("rtpopuspay", "publisher-audiopayload");
        g_object_set(payloader, "pt", 97, NULL);
    } else if (type == media::MediaType::Video) {
        source = get_video_device(device);
        MAKE_ELEMENT(conv, "videoconvert", "conv");
        MAKE_ELEMENT(capsf, "capsfilter", "capsf");
        g_object_set(capsf, "caps",
                     gst_caps_from_string("video/x-raw,framerate=30/1"),
                     nullptr);
        MAKE_ELEMENT(encoder, "vp8enc", "publisher-videoenc");
        // Generates a keyframe every 10s, we can drop that part when we wire
        // up RTCP feedback
        g_object_set(encoder, "deadline", 1, "keyframe-max-dist", 300, nullptr);
        payloader =
            gst_element_factory_make("rtpvp8pay", "publisher-videopayload");
        g_object_set(payloader, "pt", 101, NULL);
    }
    // TODO: Add a muxer when we want both audio and video from a single
    // publisher
    MAKE_ELEMENT(tee, "tee", "publisher-tee");
    MAKE_ELEMENT(queue, "queue", "publisher-queue");
    MAKE_ELEMENT(fakesink, "fakesink", "fakesink");

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus);

    gst_bin_add_many(GST_BIN(pipeline), source, conv, capsf, encoder, payloader,
                     tee, queue, fakesink, nullptr);

    if (!gst_element_link_many(source, conv, capsf, encoder, payloader, tee,
                               queue, fakesink, nullptr)) {
        g_printerr("failed to link the elements\n");
        return;
    }
    g_print("Elements linked successfully\n");
}

media::Sender::~Sender() {
    gst_object_unref(GST_OBJECT(pipeline));
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);
}

void media::Sender::start() {
    thread = std::make_unique<std::thread>([this] {
        g_print("Setting the pipeline to playing\n");
        gst_element_set_state(pipeline, GST_STATE_PLAYING);

        g_print("Running mainloop\n");
        g_main_loop_run(loop);
    });
}

void media::Sender::stop() {
    g_main_loop_quit(loop);
    g_print("stopping playback\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);

    thread->join();
    thread.reset();
}

void media::Sender::addReceiver(const std::string &id,
                                const ReceiverInfo &receiver) {
    GstElement *udpsink, *rtpbin, *queue;
    GstElement *bin = gst_bin_get_by_name(GST_BIN(pipeline), id.c_str());

    if (bin != nullptr) {
        g_printerr("Receiver id %s already exists\n", id.c_str());
        return;
    }

    bin = gst_bin_new(id.c_str());
    g_assert(bin != nullptr);

    MAKE_ELEMENT(rtpbin, "rtpbin", "publisher-rtpbin");
    MAKE_ELEMENT(queue, "queue", "publisher-q");
    MAKE_ELEMENT(udpsink, "udpsink", "publisher-udpsink");

    if (!bin || !queue || !rtpbin || !udpsink) {
        g_printerr("failed to create elements in addReceiver");
        return;
    }

    g_object_set(G_OBJECT(udpsink), "host", receiver.address.c_str(), "port",
                 receiver.port, nullptr);

    gst_bin_add_many(GST_BIN(bin), queue, rtpbin, udpsink, nullptr);

    GstPad *rtp_send_sinkpad =
        gst_element_get_request_pad(rtpbin, "send_rtp_sink_%u");
    GstPad *queue_src_pad = gst_element_get_static_pad(queue, "src");

    if (GST_PAD_LINK_OK != gst_pad_link(queue_src_pad, rtp_send_sinkpad))
        g_print("failed to link queue_src_pad and rtp_send_sinkpad");

    if (!gst_element_link_many(rtpbin, udpsink, nullptr))
        g_print("failed to link rtpbin and udpsink");

    gst_bin_add(GST_BIN(pipeline), bin);
    gst_element_sync_state_with_parent(bin);

    GstPad *queue_sink_pad = gst_element_get_static_pad(queue, "sink");
    GstPad *bin_ghost_pad = gst_ghost_pad_new("sink", queue_sink_pad);

    gst_element_add_pad(bin, bin_ghost_pad);
    if (!gst_element_link(tee, bin)) g_printerr("failed to link tee and bin");

    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL,
                              "pipeline");
}

gboolean set_to_null(gpointer data) {
    GstElement *bin = (GstElement *)data;

    GstStateChangeReturn ret = gst_element_set_state(bin, GST_STATE_NULL);
    g_print("state change return %d\n", ret);

    GstElement *pipeline = (GstElement *)gst_element_get_parent(bin);
    gst_bin_remove(GST_BIN(pipeline), bin);

    gst_object_unref(bin);
    return FALSE;
}

GstPadProbeReturn probe_call_back(GstPad *pad, GstPadProbeInfo *info,
                                  gpointer user_data) {
    GstElement *bin = (GstElement *)user_data;
    GstPad *peer_pad = gst_pad_get_peer(pad);

    if (!gst_pad_unlink(pad, peer_pad)) {
        g_printerr("failed to unlink pads in %s\n", __func__);
    }
    gst_object_unref(peer_pad);

    g_idle_add(set_to_null, bin);

    return GST_PAD_PROBE_REMOVE;
}

void media::Sender::removeReceiver(const std::string &id) {
    GstElement *bin = gst_bin_get_by_name(GST_BIN(pipeline), id.c_str());
    if (bin == nullptr) {
        g_printerr("No such receiver in the current pipeline %s\n", id.c_str());
        return;
    }

    GstPad *bin_ghost_pad = gst_element_get_static_pad(bin, "sink");

    GstPad *tee_src_pad = gst_pad_get_peer(bin_ghost_pad);

    gst_pad_add_probe(tee_src_pad, GST_PAD_PROBE_TYPE_IDLE, probe_call_back,
                      bin, nullptr);

    gst_object_unref(bin_ghost_pad);
    gst_object_unref(tee_src_pad);
}
