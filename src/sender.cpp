#include "gst/gstbin.h"
#include "gst/gstelement.h"
#include "gst/gstelementfactory.h"
#include "gst/gstutils.h"
#include <iterator>
#include <sync.hpp>

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

synch::Sender::Sender (const std::string & id, synch::MediaType type,
    const synch::CaptureDevice & device, const synch::ClockInfo & clock)
{
  GstBus *
      bus;
  gst_init (nullptr, nullptr);

  loop = g_main_loop_new (nullptr, FALSE);

  pipeline = gst_pipeline_new (id.c_str ());
  if (type == synch::MediaType::Audio) {
    source = gst_element_factory_make ("pulsesrc", "publisher-audiosrc");
    conv = gst_element_factory_make ("audioconvert", "conv");
    encoder = gst_element_factory_make ("opusenc", "publisher-audioenc");
    payloader =
        gst_element_factory_make ("rtpopuspay", "publisher-audiopayload");
  } else if (type == synch::MediaType::Video) {
    source = gst_element_factory_make ("v4l2src", "publisher-videosrc");
    conv = gst_element_factory_make ("videoconvert", "conv");
    encoder = gst_element_factory_make ("vp8enc", "publisher-videoenc");
    payloader =
        gst_element_factory_make ("rtpvp8pay", "publisher-videopayload");
  }
  // TODO: Add a muxer when we want both audio and video from a single publisher
  tee = gst_element_factory_make ("tee", "publisher-tee");
  fakesink = gst_element_factory_make("fakesink", "fakesink");


  g_object_set (G_OBJECT (source), "device", device.device.c_str(), nullptr);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_callback, loop);
  gst_object_unref (bus);

  gst_bin_add_many (GST_BIN (pipeline),
      source, conv, encoder, payloader, tee, fakesink, nullptr);

  if (!gst_element_link_many (source, conv, encoder, payloader, tee, fakesink, nullptr)) {
    g_printerr ("failed to link the elements");
    return;
  }
  g_print ("Elements linked successfully\n");
}

synch::Sender::~Sender ()
{

  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

void
synch::Sender::start ()
{

  g_print ("Setting the pipeline to playing\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_print ("Running mainloop\n");
  g_main_loop_run (loop);
}

void
synch::Sender::stop ()
{
  g_print ("stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
}

void
synch::Sender::addReceiver (const std::string & id,
    const ReceiverInfo & receiver)
{

  auto p = recvr_ids.insert (id);
  if (p.second == false) {
    g_printerr ("failed to insert %s. Can't add receiver", id.c_str ());
    return;
  }
  auto idx = std::distance (recvr_ids.begin (), p.first) + 1;

  gchar *
      tee_src_pad = g_strdup_printf ("src_%u", (guint) idx);

  GstElement *
      bin = gst_bin_new (id.c_str ());
  GstElement *
      rtpbin = gst_element_factory_make ("rtpbin", "publisher-rtpbin");
  GstElement *
      queue = gst_element_factory_make ("queue", "publisher-q");
  GstElement *
      udpsink = gst_element_factory_make ("udpsink", "publisher-udpsink");
  
  g_object_set (G_OBJECT (udpsink), "host", receiver.address.c_str(),
      "port", receiver.port, nullptr);
  
  gst_bin_add (GST_BIN (pipeline), bin);
  gst_element_sync_state_with_parent (bin);

  gst_bin_add_many (GST_BIN (bin), queue, rtpbin, udpsink, nullptr);
  gst_element_sync_state_with_parent (queue);
  gst_element_sync_state_with_parent (rtpbin);
  gst_element_sync_state_with_parent (udpsink);
 
  if (!gst_element_link_pads (queue, "src", rtpbin, "send_rtp_sink_0")) {
    g_printerr ("Failed to link queue and rtpbin");
    return;
  }

  if (!gst_element_link_pads (rtpbin, "send_rtp_src_0", udpsink, "sink")) {
    g_printerr ("Failed to link rtpbin and udpsink");
    return;
  }

  if (!gst_element_link_pads (tee, tee_src_pad, queue, "sink")) {
    g_printerr ("Failed to link tee and queue");
  }
}

GstPadProbeReturn probe_call_back (GstPad *pad, GstPadProbeInfo *info,
    gpointer user_data) {

  GstPad *q_sink_pad = (GstPad *) user_data;
  if(!gst_pad_unlink(pad, q_sink_pad)) {
    g_printerr("failed to unlink pads in %s", __func__);
  }

  // signal to set reciever bin to NULL state
  return GST_PAD_PROBE_REMOVE;
}

void
synch::Sender::removeReceiver (const std::string & id)
{

  GstElement *bin = gst_bin_get_by_name(GST_BIN (pipeline), id.c_str());
  if (bin == NULL) {
    g_printerr("No such receiver in the current pipeline %s", id.c_str());
    return;
  }
  GstElement *queue = gst_bin_get_by_name (GST_BIN(bin), "publisher-q");
  GstPad *q_sink_pad = gst_element_get_static_pad (queue, "sink");
  GstPad *tee_src_pad = gst_pad_get_peer(q_sink_pad);
  gst_pad_add_probe (tee_src_pad, GST_PAD_PROBE_TYPE_IDLE, probe_call_back, q_sink_pad, nullptr);
  // wait for the signal - to set reciever bin to NULL state
}
