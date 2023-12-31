/* GStreamer
 * Copyright (C) 2023 Metrological
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include <gst/gst.h>
#include <gst/audio/gstaudiosrc.h>
#include "gstbluetoothaudiosrc.h"

#include <WPEFramework/bluetoothaudiosource/bluetoothaudiosource.h>


#define RECEIVE_BUFFER_SIZE (64 * 1024)

GST_DEBUG_CATEGORY_STATIC (gst_bluetoothaudiosrc_debug_category);
#define GST_CAT_DEFAULT gst_bluetoothaudiosrc_debug_category


/* implementation */

static void _exit_handler (void)
{
  bluetoothaudiosource_dispose ();
}

static void _audio_source_install_dispose_handler (void)
{
  static gboolean installed = FALSE;

  if (!installed) {
    atexit (_exit_handler);
  }
}

static uint32_t _audio_source_configure_sink (const bluetoothaudiosource_format_t *format, void *user_data)
{
  uint32_t result = BLUETOOTHAUDIOSOURCE_SUCCESS;

  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  return (result);
}

static uint32_t _audio_source_acquire_sink (void *user_data)
{
  uint32_t result = BLUETOOTHAUDIOSOURCE_SUCCESS;

  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  return (result);
}

static uint32_t _audio_source_relinquish_sink (void *user_data)
{
  uint32_t result = BLUETOOTHAUDIOSOURCE_SUCCESS;

  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  return (result);
}

static uint32_t _audio_source_set_sink_speed (const int8_t speed, void *user_data)
{
  uint32_t result = BLUETOOTHAUDIOSOURCE_SUCCESS;

  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  g_mutex_lock (&bluetoothaudiosrc->lock);

  if (speed == 0) {
    bluetoothaudiosrc->playing = FALSE;
  }
  else if (speed == 100) {
    bluetoothaudiosrc->reset = FALSE;
    bluetoothaudiosrc->playing = TRUE;
    bluetoothaudiosrc->buffering = TRUE;
  }

  g_mutex_unlock (&bluetoothaudiosrc->lock);

  return (result);
}

static uint32_t _audio_source_get_sink_time (uint32_t* time_ms, void *user_data)
{
  uint32_t result = BLUETOOTHAUDIOSOURCE_SUCCESS;

  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  return (result);
}

static uint32_t _audio_source_get_sink_delay(uint32_t* delay_samples, void *user_data)
{
  uint32_t result = BLUETOOTHAUDIOSOURCE_SUCCESS;

  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  return (result);
}

static void _audio_source_frame (const uint16_t length_bytes, const uint8_t frame[], void *user_data)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  gboolean empty = TRUE;

  g_mutex_lock (&bluetoothaudiosrc->lock);

  if (length_bytes < (bluetoothaudiosrc->buffer_size - bluetoothaudiosrc->buffer_write_offset)) {
    memcpy (bluetoothaudiosrc->buffer + bluetoothaudiosrc->buffer_write_offset, frame, length_bytes);
    bluetoothaudiosrc->buffer_write_offset += length_bytes;
  } else {
    // Wrap around...
    const guint16 head = (bluetoothaudiosrc->buffer_size - bluetoothaudiosrc->buffer_write_offset);
    memcpy ((bluetoothaudiosrc->buffer + bluetoothaudiosrc->buffer_write_offset), frame, head);
    memcpy (bluetoothaudiosrc->buffer, (frame + head), (length_bytes - head));
    bluetoothaudiosrc->buffer_write_offset = (length_bytes - head);

    GST_WARNING_OBJECT (bluetoothaudiosrc, "Buffer overflow");
  }

  g_mutex_unlock (&bluetoothaudiosrc->lock);
}

static void _audio_source_callback_state_changed (const bluetoothaudiosource_state_t state, void *user_data)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  switch (state) {
  case BLUETOOTHAUDIOSOURCE_STATE_CONNECTED:
    GST_INFO_OBJECT (bluetoothaudiosrc, "Bluetooth audio source is now connected!");
    break;
  case BLUETOOTHAUDIOSOURCE_STATE_CONNECTED_BAD:
    GST_ERROR_OBJECT (bluetoothaudiosrc, "Invalid device connected - cant't play!");
    break;
  case BLUETOOTHAUDIOSOURCE_STATE_DISCONNECTED:
    GST_WARNING_OBJECT (bluetoothaudiosrc, "Bluetooth Audio source is now disconnected!");
    break;
  case BLUETOOTHAUDIOSOURCE_STATE_READY:
    GST_INFO_OBJECT (bluetoothaudiosrc, "Bluetooth Audio source now ready!");
    break;
  case BLUETOOTHAUDIOSOURCE_STATE_STREAMING:
    GST_INFO_OBJECT (bluetoothaudiosrc, "Bluetooth Audio source is now streaming!");
    break;
  default:
    break;
  }
}

static void _audio_source_callback_operational_state_updated (const uint8_t running, void *user_data)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (user_data);

  g_assert (bluetoothaudiosrc != NULL);

  if (running) {

    GST_INFO_OBJECT (bluetoothaudiosrc, "Bluetooth Audio Source service now available");

    /* Register for the source updates... */
    if (bluetoothaudiosource_register_state_changed_callback (&_audio_source_callback_state_changed, bluetoothaudiosrc) != BLUETOOTHAUDIOSOURCE_SUCCESS) {
      GST_ERROR_OBJECT (bluetoothaudiosrc, "bluetoothaudiosource_register_state_changed_callback() failed");
    } else {
      GST_INFO_OBJECT (bluetoothaudiosrc, "Successfully registered to Bluetooth Audio Source status update callback");

      if (bluetoothaudiosource_set_sink(&bluetoothaudiosrc->sink_callbacks, bluetoothaudiosrc) != BLUETOOTHAUDIOSOURCE_SUCCESS) {
        GST_ERROR_OBJECT (bluetoothaudiosrc, "bluetoothaudiosource_set_sink() failed");
      }
    }
  } else {
    GST_INFO_OBJECT (bluetoothaudiosrc, "Bluetooth Audio Source service is now unvailable");
  }
}

static void _audio_source_initialize (GstBluetoothAudioSrc *bluetoothaudiosrc)
{
  g_mutex_init (&bluetoothaudiosrc->lock);

  g_assert (bluetoothaudiosrc != NULL);

  bluetoothaudiosrc->sink_callbacks.configure_cb = _audio_source_configure_sink;
  bluetoothaudiosrc->sink_callbacks.acquire_cb = _audio_source_acquire_sink;
  bluetoothaudiosrc->sink_callbacks.relinquish_cb = _audio_source_relinquish_sink;
  bluetoothaudiosrc->sink_callbacks.set_speed_cb = _audio_source_set_sink_speed;
  bluetoothaudiosrc->sink_callbacks.get_time_cb = _audio_source_get_sink_time;
  bluetoothaudiosrc->sink_callbacks.get_delay_cb = _audio_source_get_sink_delay;
  bluetoothaudiosrc->sink_callbacks.frame_cb = _audio_source_frame;

  bluetoothaudiosrc->buffer_size = RECEIVE_BUFFER_SIZE;
  bluetoothaudiosrc->buffer_write_offset = 0;
  bluetoothaudiosrc->buffer = malloc(bluetoothaudiosrc->buffer_size);
  g_assert(bluetoothaudiosrc->buffer != NULL);

  bluetoothaudiosrc->playing = FALSE;
  bluetoothaudiosrc->buffering = FALSE;
  bluetoothaudiosrc->reset = FALSE;

  bluetoothaudiosrc->clock = 0;
  bluetoothaudiosrc->clock_base = 0;

  /* Register for the Bluetooth Audio Source service updates... */
  if (bluetoothaudiosource_register_operational_state_update_callback (&_audio_source_callback_operational_state_updated, bluetoothaudiosrc) != BLUETOOTHAUDIOSOURCE_SUCCESS) {
    GST_ERROR_OBJECT (bluetoothaudiosrc, "bluetoothaudiosource_register_operational_state_update_callback() failed");
  } else {
    GST_INFO_OBJECT (bluetoothaudiosrc, "Successfully registered to Bluetooth Audio Source service operational callback");
  }

  _audio_source_install_dispose_handler ();
}

static void _audio_source_deinitialize (GstBluetoothAudioSrc *bluetoothaudiosrc)
{
  g_assert (bluetoothaudiosrc != NULL);

  GST_INFO_OBJECT (bluetoothaudiosrc, "Deinitializing...");

  bluetoothaudiosource_relinquish ();
  bluetoothaudiosource_set_sink (NULL, NULL);
  bluetoothaudiosource_unregister_state_changed_callback (&_audio_source_callback_state_changed);
  bluetoothaudiosource_unregister_operational_state_update_callback (&_audio_source_callback_operational_state_updated);

  g_mutex_lock (&bluetoothaudiosrc->lock);

  free (bluetoothaudiosrc->buffer);

  g_mutex_unlock (&bluetoothaudiosrc->lock);

  g_mutex_clear (&bluetoothaudiosrc->lock);
}


/* prototypes */

static void gst_bluetoothaudiosrc_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_bluetoothaudiosrc_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_bluetoothaudiosrc_dispose (GObject *object);
static void gst_bluetoothaudiosrc_finalize (GObject *object);

static GstStateChangeReturn gst_bluetoothaudiosrc_change_state (GstElement *element, GstStateChange transition);

static gboolean gst_bluetoothaudiosrc_open (GstAudioSrc *src);
static gboolean gst_bluetoothaudiosrc_prepare (GstAudioSrc *src, GstAudioRingBufferSpec *spec);
static gboolean gst_bluetoothaudiosrc_unprepare (GstAudioSrc *src);
static gboolean gst_bluetoothaudiosrc_close (GstAudioSrc *src);
static guint gst_bluetoothaudiosrc_read (GstAudioSrc *src, gpointer data, guint length, GstClockTime *timestamp);
static guint gst_bluetoothaudiosrc_delay (GstAudioSrc *src);
static void gst_bluetoothaudiosrc_reset (GstAudioSrc *src);


enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_bluetoothaudiosrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,"
      "format=S16LE,"
      "rate={32000,44100,48000}," /* Standard sample rates required to be supported by all source devices.*/
      "channels=[1,2],"
      "layout=interleaved")
    );

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstBluetoothAudioSrc, gst_bluetoothaudiosrc, GST_TYPE_AUDIO_SRC,
  GST_DEBUG_CATEGORY_INIT (gst_bluetoothaudiosrc_debug_category, "bluetoothaudiosrc", 0, "debug category for bluetoothaudiosrc element"));

static void gst_bluetoothaudiosrc_class_init (GstBluetoothAudioSrcClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioSrcClass *audio_src_class = GST_AUDIO_SRC_CLASS (klass);

  g_assert (klass != NULL);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass), &gst_bluetoothaudiosrc_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Audio Source (Bluetooth)", "Source/Audio", "Input from Bluetooth audio device", "Metrological");

  gobject_class->set_property = gst_bluetoothaudiosrc_set_property;
  gobject_class->get_property = gst_bluetoothaudiosrc_get_property;
  gobject_class->dispose = gst_bluetoothaudiosrc_dispose;
  gobject_class->finalize = gst_bluetoothaudiosrc_finalize;

  audio_src_class->open = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosrc_open);
  audio_src_class->prepare = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosrc_prepare);
  audio_src_class->unprepare = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosrc_unprepare);
  audio_src_class->close = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosrc_close);
  audio_src_class->read = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosrc_read);
  audio_src_class->delay = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosrc_delay);
  audio_src_class->reset = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosrc_reset);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosrc_change_state);
}

/* implementation */

static void gst_bluetoothaudiosrc_init (GstBluetoothAudioSrc *bluetoothaudiosrc)
{
  g_assert (bluetoothaudiosrc != NULL);

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "init");

  _audio_source_initialize (bluetoothaudiosrc);
}

static void gst_bluetoothaudiosrc_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (object);

  g_assert (bluetoothaudiosrc != NULL);

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void gst_bluetoothaudiosrc_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (object);

  g_assert (bluetoothaudiosrc != NULL);

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void gst_bluetoothaudiosrc_dispose (GObject *object)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (object);

  g_assert (bluetoothaudiosrc != NULL);

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "dispose");

  G_OBJECT_CLASS (gst_bluetoothaudiosrc_parent_class)->dispose (object);
}

void gst_bluetoothaudiosrc_finalize (GObject *object)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (object);

  g_assert (bluetoothaudiosrc != NULL);

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "finalize");

  _audio_source_deinitialize (bluetoothaudiosrc);

  G_OBJECT_CLASS (gst_bluetoothaudiosrc_parent_class)->finalize (object);
}

static GstStateChangeReturn gst_bluetoothaudiosrc_change_state (GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (element);

  g_assert (bluetoothaudiosrc != NULL);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    case GST_STATE_CHANGE_READY_TO_NULL:
    case GST_STATE_CHANGE_NULL_TO_NULL:
    case GST_STATE_CHANGE_READY_TO_READY:
    case GST_STATE_CHANGE_PAUSED_TO_PAUSED:
    case GST_STATE_CHANGE_PLAYING_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING: {
      GST_INFO_OBJECT (bluetoothaudiosrc, "state changed to playing!");

      struct timespec time_now;
      timespec_get (&time_now, TIME_UTC);
      bluetoothaudiosrc->clock_base = GST_TIMESPEC_TO_TIME (time_now);
      break;
    }
  }

  ret = GST_ELEMENT_CLASS (gst_bluetoothaudiosrc_parent_class)->change_state (element, transition);

  return ret;
}

/* open the device with given specs */
static gboolean gst_bluetoothaudiosrc_open (GstAudioSrc *src)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (src);

  g_assert (bluetoothaudiosrc != NULL);

  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "open");

  return result;
}

/* prepare resources and state to operate with the given specs */
static gboolean gst_bluetoothaudiosrc_prepare (GstAudioSrc *src, GstAudioRingBufferSpec *spec)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (src);

  g_assert (bluetoothaudiosrc != NULL);

  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "prepare");

  bluetoothaudiosrc->channels = 2;
  bluetoothaudiosrc->bps = 16;
  bluetoothaudiosrc->frame_rate = 44100;
  bluetoothaudiosrc->bitrate = (bluetoothaudiosrc->channels * (bluetoothaudiosrc->bps / 8) * bluetoothaudiosrc->frame_rate * 8);

  return result;
}

/* undo anything that was done in prepare() */
static gboolean gst_bluetoothaudiosrc_unprepare (GstAudioSrc *src)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (src);

  g_assert (bluetoothaudiosrc != NULL);

  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "unprepare");

  return result;
}

/* close the device */
static gboolean gst_bluetoothaudiosrc_close (GstAudioSrc *src)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (src);

  g_assert (bluetoothaudiosrc != NULL);

  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "close");

  return result;
}

static guint64 advance_clock(GstBluetoothAudioSrc *bluetoothaudiosrc, guint length)
{
  g_assert (bluetoothaudiosrc != NULL);

  bluetoothaudiosrc->clock += ((1000000000ULL * length) / (bluetoothaudiosrc->bitrate / 8));

  return bluetoothaudiosrc->clock;
}

/* read samples from the device */
static guint gst_bluetoothaudiosrc_read (GstAudioSrc *src, gpointer data, guint length, GstClockTime *timestamp)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (src);

  guint result = length;

  g_assert (bluetoothaudiosrc != NULL);
  g_assert (length != 0);

  // GST_DEBUG_OBJECT (bluetoothaudiosrc, "read");

  /* this is a blocking call! */

  guint64 clock_played = 0;

  while (result != 0) {

    g_mutex_lock (&bluetoothaudiosrc->lock);

    if (bluetoothaudiosrc->reset) {
      // Break issued!
      g_mutex_unlock (&bluetoothaudiosrc->lock);
      break;
    }

    guint32 size = length;

    if (bluetoothaudiosrc->buffering) {
      size = (bluetoothaudiosrc->buffer_size / 4);
      GST_DEBUG_OBJECT (bluetoothaudiosrc, "buffering... (%lu/%lu)", bluetoothaudiosrc->buffer_write_offset, size);
    }

    if (((bluetoothaudiosrc->playing) && (bluetoothaudiosrc->buffer_write_offset >= size))
          || ((!bluetoothaudiosrc->playing) && (bluetoothaudiosrc->buffer_write_offset != 0)))  {
      // if the device is playing and we have  buffered already
      // or the device is not playing anymore but there is still data available to play out...

      const guint32 available = (result < bluetoothaudiosrc->buffer_write_offset ? result : bluetoothaudiosrc->buffer_write_offset);
      g_assert (available);

      // GST_DEBUG_OBJECT (bluetoothaudiosrc, "buffer health (%lu/%lu)", bluetoothaudiosrc->buffer_write_offset, size);

      memcpy ((data + (length - result)), bluetoothaudiosrc->buffer, available);
      memmove (bluetoothaudiosrc->buffer, (bluetoothaudiosrc->buffer + available), (bluetoothaudiosrc->buffer_write_offset - available));
      bluetoothaudiosrc->buffer_write_offset -= available;
      result -= available;

      clock_played = advance_clock (bluetoothaudiosrc, available);
      bluetoothaudiosrc->buffering = FALSE;
    }
    else {
      if (bluetoothaudiosrc->playing && !bluetoothaudiosrc->buffering)
        GST_WARNING_OBJECT (bluetoothaudiosrc, "buffer underflow (%lu/%lu)", bluetoothaudiosrc->buffer_write_offset, size);

      // Not playing currently, but since this is live playback, stuff it.
      memset (data + (length - result), 0, result);
      clock_played = advance_clock (bluetoothaudiosrc, result);
      result = 0;
    }

    g_mutex_unlock (&bluetoothaudiosrc->lock);
  }

  if (clock_played) {
    // GstAudioSrc is a live source, so ensure the data is served at a roughly real time pace.

    struct timespec time_now;
    timespec_get (&time_now, TIME_UTC);

    const GstClockTime clock_now = GST_TIMESPEC_TO_TIME (time_now);
    const GstClockTime clock_elapsed = (clock_now - bluetoothaudiosrc->clock_base);
    const GstClockTime clock_sleep = (clock_played > clock_elapsed ? (clock_played - clock_elapsed) : 0);

    if (clock_sleep)
      g_usleep (clock_sleep / 1000);
  }

  return length;
}

/* get number of samples queued in the device */
static guint gst_bluetoothaudiosrc_delay (GstAudioSrc *src)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (src);
  guint delay = 0;

  g_assert (bluetoothaudiosrc != NULL);

  // GST_DEBUG_OBJECT (bluetoothaudiosrc, "delay");

  return delay;
}

/* reset the audio device, unblock from a read */
static void gst_bluetoothaudiosrc_reset (GstAudioSrc *src)
{
  GstBluetoothAudioSrc *bluetoothaudiosrc = GST_BLUETOOTHAUDIOSRC (src);

  g_assert (bluetoothaudiosrc != NULL);

  GST_DEBUG_OBJECT (bluetoothaudiosrc, "reset");

  g_mutex_lock (&bluetoothaudiosrc->lock);

  bluetoothaudiosrc->reset = TRUE;

  g_mutex_unlock (&bluetoothaudiosrc->lock);
}

static gboolean plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, "bluetoothaudiosrc", GST_RANK_PRIMARY, GST_TYPE_BLUETOOTHAUDIOSRC);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "gstbluetoothaudiosrc"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gstbluetoothaudiosrc"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://github.com/WebPlatformForEmbedded/gstbluetoothaudiosource"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bluetoothaudiosrc,
    "bluetoothaudisrc plugin",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
