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

static void rtpbin_pad_padded(GstElement *ele, GstPad *pad, gpointer userdata) {
    gchar *padname = gst_pad_get_name(pad);
    gchar *prefix = g_strdup_printf("recv_rtp_src_");
    // TODO what if multiple pads of recv_rtp_src qget added. need unique id
    if (g_str_has_prefix(padname, prefix)) {
        GstElement *downstream = (GstElement *)userdata;
        GstPad *sinkpad = gst_element_get_static_pad(downstream, "sink");
        gst_pad_link(pad, sinkpad);
        gst_object_unref(sinkpad);
    }
    g_free(prefix);
    g_free(padname);
}

media::Receiver::Receiver(const std::string &id, MediaType type,
                          const ReceiverInfo &receiver,
                          const PlaybackDevice &device, const ClockInfo &clock,
                          std::chrono::duration<int> delay)
    : thread(nullptr) {
    GstBus *bus;
    GstElement *rtpbin, *udpsrc, *depayloader, *decoder, *sink;
    gst_init(nullptr, nullptr);

    loop = g_main_loop_new(nullptr, FALSE);

    pipeline = gst_pipeline_new(id.c_str());

    MAKE_ELEMENT(udpsrc, "udpsrc", "receiver-udpsrc");
    g_object_set(udpsrc, "port", receiver.port, NULL);
    if (!receiver.address.empty())
        g_object_set(udpsrc, "address", receiver.address.c_str(), NULL);
    MAKE_ELEMENT(rtpbin, "rtpbin", "receiver-rtpbin");
    if (type == media::MediaType::Audio) {
        MAKE_ELEMENT(depayloader, "rtpopusdepay", "receiver-depay");
        MAKE_ELEMENT(decoder, "opusdec", "receiver-dec");
        MAKE_ELEMENT(sink, "autoaudiosink", "receiver-sink");
        g_object_set(
            udpsrc, "caps",
            gst_caps_from_string("application/"
                                 "x-rtp,media=(string)audio,clock-rate=(int)"
                                 "48000,encoding-name=(string)OPUS,payload=97"),
            nullptr);
    } else if (type == media::MediaType::Video) {
        MAKE_ELEMENT(depayloader, "rtpvp8depay", "receiver-depay");
        MAKE_ELEMENT(decoder, "vp8dec", "receiver-dec");
        MAKE_ELEMENT(sink, "autovideosink", "receiver-sink");
        g_object_set(
            udpsrc, "caps",
            gst_caps_from_string("application/"
                                 "x-rtp,media=(string)video,clock-rate=(int)"
                                 "90000,encoding-name=(string)VP8,payload=101"),
            nullptr);
    }
    g_signal_connect(rtpbin, "pad-added", G_CALLBACK(rtpbin_pad_padded),
                     depayloader);
    GstPad *src_pad = gst_element_get_static_pad(udpsrc, "src");
    GstPad *recv_rtp_sink_pad =
        gst_element_get_request_pad(rtpbin, "recv_rtp_sink_%u");

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus);

    gst_bin_add_many(GST_BIN(pipeline), udpsrc, rtpbin, depayloader, decoder,
                     sink, NULL);
    gst_pad_link(src_pad, recv_rtp_sink_pad);

    // GstPad *recv_rtp_src_pad =
    //     gst_element_request_pad_simple(rtpbin, "recv_rtp_src_%u_%u_%u");
    // GstPad *depay_sink = gst_element_get_static_pad(depayloader, "sink");
    // gst_pad_link(recv_rtp_src_pad, depay_sink);
    // // if (!gst_element_link(rtpbin, depayloader)) {
    //     g_printerr("Failed to link the rtpbin and depay\n");
    //     return;
    // }
    if (!gst_element_link_many(depayloader, decoder, sink, NULL)) {
        g_printerr("Failed to link the elements\n");
        return;
    }
}

media::Receiver::~Receiver() {
    gst_object_unref(GST_OBJECT(pipeline));
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);
}

void media::Receiver::start() {
    thread = std::make_unique<std::thread>([this] {
        g_print("Setting the pipeline to playing\n");
        gst_element_set_state(pipeline, GST_STATE_PLAYING);

        g_print("Running mainloop\n");

        GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL,
                                  "pipeline");
        g_main_loop_run(loop);
    });
}

void media::Receiver::stop() {
    g_main_loop_quit(loop);
    g_print("stopping playback\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);

    thread->join();
    thread.reset();
}
