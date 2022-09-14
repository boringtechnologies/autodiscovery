#include <sync.hpp>

#include <iterator>
#include <memory>
#include <thread>

static gboolean
bus_callback (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

media::Sender::Sender (const std::string & id, media::MediaType type,
    const media::CaptureDevice & device, const media::ClockInfo & clock) : thread(nullptr)
{
  GstBus *
      bus;
  gst_init (nullptr, nullptr);

  loop = g_main_loop_new (nullptr, FALSE);

  pipeline = gst_pipeline_new (id.c_str ());
  if (type == media::MediaType::Audio) {
    source = gst_element_factory_make ("pulsesrc", "publisher-audiosrc");
    conv = gst_element_factory_make ("audioconvert", "conv");
    encoder = gst_element_factory_make ("opusenc", "publisher-audioenc");
    payloader =
        gst_element_factory_make ("rtpopuspay", "publisher-audiopayload");
  } else if (type == media::MediaType::Video) {
    source = gst_element_factory_make ("v4l2src", "publisher-videosrc");
    conv = gst_element_factory_make ("videoconvert", "conv");
    encoder = gst_element_factory_make ("vp8enc", "publisher-videoenc");
    payloader =
        gst_element_factory_make ("rtpvp8pay", "publisher-videopayload");
  }
  // TODO: Add a muxer when we want both audio and video from a single publisher
  tee = gst_element_factory_make ("tee", "publisher-tee");
  queue = gst_element_factory_make ("queue", "publisher-queue");
  fakesink = gst_element_factory_make ("fakesink", "fakesink");


  g_object_set (G_OBJECT (source), "device", device.device.c_str (), nullptr);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_callback, loop);
  gst_object_unref (bus);

  gst_bin_add_many (GST_BIN (pipeline),
      source, conv, encoder, payloader, tee, queue, fakesink, nullptr);

  if (!gst_element_link_many (source, conv, encoder, payloader, tee, queue,
          fakesink, nullptr)) {
    g_printerr ("failed to link the elements\n");
    return;
  }
  g_print ("Elements linked successfully\n");
}

media::Sender::~Sender ()
{

  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

void
media::Sender::start ()
{
  thread = std::make_unique<std::thread>([this] {
    g_print ("Setting the pipeline to playing\n");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    g_print ("Running mainloop\n");
    g_main_loop_run (loop);
  });
}

void
media::Sender::stop ()
{
  g_main_loop_quit (loop);
  g_print ("stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  thread->join();
  thread.reset();
}

void
media::Sender::addReceiver (const std::string & id,
    const ReceiverInfo & receiver)
{

  GstElement *
      bin = gst_bin_get_by_name (GST_BIN (pipeline), id.c_str ());
  if (bin != NULL) {
    g_printerr("Receiver id %s already exists\n", id.c_str());
    return;
  }
  bin = gst_bin_new (id.c_str ());
  g_assert (bin != NULL);
  GstElement *
      rtpbin = gst_element_factory_make ("rtpbin", "publisher-rtpbin");
  GstElement *
      queue = gst_element_factory_make ("queue", "publisher-q");
  GstElement *
      udpsink = gst_element_factory_make ("udpsink", "publisher-udpsink");

  if (!bin || !queue || !rtpbin || !udpsink) {
    g_printerr("failed to create elements in addReceiver");
    return;
  }

  g_object_set (G_OBJECT (udpsink), "host", receiver.address.c_str (),
      "port", receiver.port, nullptr);

  gst_bin_add_many (GST_BIN (bin), queue, rtpbin, udpsink, nullptr);

  GstPad *
      rtp_send_sinkpad =
      gst_element_request_pad_simple (rtpbin, "send_rtp_sink_%u");
  GstPad *
      queue_src_pad = gst_element_get_static_pad (queue, "src");

  if (GST_PAD_LINK_OK != gst_pad_link (queue_src_pad, rtp_send_sinkpad))
    g_print ("failed to link queue_src_pad and rtp_send_sinkpad");

  if (! gst_element_link_many(rtpbin, udpsink, NULL))
      g_print ("failed to link rtpbin and udpsink");

  gst_bin_add (GST_BIN (pipeline), bin);
  gst_element_sync_state_with_parent (bin);


  GstPad *
      queue_sink_pad = gst_element_get_static_pad (queue, "sink");
  GstPad *
      bin_ghost_pad = gst_ghost_pad_new ("sink", queue_sink_pad);

  gst_element_add_pad (bin, bin_ghost_pad);
  if (!gst_element_link (tee, bin))
    g_printerr ("failed to link tee and bin");

  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_ALL,
      "pipeline");
}

gboolean
set_to_null (gpointer data)
{
  GstElement *
      bin = (GstElement *) data;
  GstStateChangeReturn ret = gst_element_set_state (bin, GST_STATE_NULL);
  g_print ("state change return %d\n", ret);
  GstElement * pipeline = (GstElement *) gst_element_get_parent(bin);
  gst_bin_remove (GST_BIN (pipeline), bin);
  //TODO unref bin
  return FALSE;
}

GstPadProbeReturn
probe_call_back (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  g_print ("%s \n", __func__);

  GstElement *
      bin = (GstElement *) user_data;
  GstPad *
      peer_pad = gst_pad_get_peer (pad);
  if (!gst_pad_unlink (pad, peer_pad)) {
    g_printerr ("failed to unlink pads in %s\n", __func__);
  }
  gst_object_unref (peer_pad);
  g_idle_add (set_to_null, bin);
  return GST_PAD_PROBE_REMOVE;
}

void
media::Sender::removeReceiver (const std::string & id)
{

  GstElement *
      bin = gst_bin_get_by_name (GST_BIN (pipeline), id.c_str ());
  if (bin == NULL) {
    g_printerr ("No such receiver in the current pipeline %s\n", id.c_str ());
    return;
  }

  GstPad *
      bin_ghost_pad = gst_element_get_static_pad (bin, "sink");

  GstPad *
      tee_src_pad = gst_pad_get_peer (bin_ghost_pad);

  // TODO bitwise OR works only we do typecast. Know why
  gst_pad_add_probe (tee_src_pad,
      GST_PAD_PROBE_TYPE_IDLE, probe_call_back, bin, nullptr);
  gst_object_unref (bin_ghost_pad);
  gst_object_unref (tee_src_pad);
}
