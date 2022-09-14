#ifndef SYNC_H_
#define SYNC_H_

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <gst/gst.h>

// Synced Groups API
//
// The key considerations for the moment are:
//
//  * Receivers will receiver from a single Sender (but clients can
//    instantiate as many Receivers as they want)
//
//  * Each audio and video stream will constitute an indivial Sender (this can
//    make things a little less efficient in number of ports consumed, but we
//    can worry about that when we start doing ICE)
//
//  * We will start an internal event loop thread for each library instance,
//    from which error and other callbacks will be emitted. This does not
//    include other threads created by GStreamer itself.
//

namespace media {
enum MediaType {
  Audio,
  Video,
};

// Common clock information for all receivers
struct ClockInfo {
  // XXX: What clock type should we use? GstNetClock or NTP? Do we want to
  // support more than one?
  std::string address;
  uint16_t port;
};

struct SenderInfo {
  // XXX: To be fleshed out, but this will contain any information we want to
  // publish to receivers (such as format information) so they can initialise
  // themselves for the data they will need to decode/render.
};

struct ReceiverInfo {
  std::string address;
  uint16_t port;
};

struct CaptureDevice {
  // XXX: This can be extended for other source types
  std::string device;
};

class Sender {
private:
  std::unique_ptr<std::thread> thread;
  GMainLoop *loop;
  guint bus_watch_id;
  GstElement *pipeline, *tee;
public:
  // The constructor takes:
  //   * An instance id, for easier debugging
  //   * A media type (audio/video)
  //   * A device that we can use as the source
  //   * A clock to sync to
  //
  // When constructed, the required resources are allocated, but nothing
  // happens yet. The ::start() method must be called.
  //
  // XXX: Do we want audio and video senders to be completely independent? If
  // not, we can add an audio and video device here, and the type might be
  // redundant
  Sender(const std::string &id, MediaType type, const CaptureDevice &device,
         const ClockInfo &clock);

  // Just tear everything down
  ~Sender();

  // Start streaming media to any connected receivers.
  //
  // XXX: How should we handle errors? We could return an error here, but an
  // error callback might be better.
  void start();

  // Shut down capturing media and streaming to any connected receivers.
  void stop();

  // Have the sender stream to the receiver at the specified address/port
  // pair. The id is handy as a handle to remove the receiver later.
  //
  // XXX: We might choose to mux RTCP from the get-go so we don't have to
  // reserve more than one port at a time, which should make ICE easier.
  void addReceiver(const std::string &id, const ReceiverInfo &receiver);

  void removeReceiver(const std::string &id);
};

struct PlaybackDevice {
  // XXX: We can render to an audio device or a separate video window easily
  // enough, but we need a good way to provide a platform-specific surface to
  // render within the application itself.
};

class Receiver {
public:
  // The constructor takes:
  //   * An instance id, for easier debugging
  //   * A media type (audio/video)
  //   * A device to play to
  //   * A clock to sync to
  //   * The total delay to playback with for sync
  //
  // When constructed, the required resources are allocated, but nothing
  // happens yet. The ::start() method must be called.
  //
  // XXX: If a Sender sends audio and video, this API will need to change too.
  Receiver(const std::string &id, MediaType type, const ReceiverInfo &receiver,
           const PlaybackDevice &device, const ClockInfo &clock,
           std::chrono::duration<int> delay);

  // Just tear everything down
  ~Receiver();

  // Start streaming media from any connected senders.
  //
  // XXX: How should we handle errors? We could return an error here, but an
  // error callback might be better.
  void start();

  // Shut down reading media from senders.
  void stop();
};
} // namespace sync

#endif // SYNC_H_
