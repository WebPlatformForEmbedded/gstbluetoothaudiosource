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
#include "gstbluetoothaudiosource.h"

#include <WPEFramework/bluetoothaudiosource/bluetoothaudiosource.h>


GST_DEBUG_CATEGORY_STATIC (gst_bluetoothaudiosource_debug_category);
#define GST_CAT_DEFAULT gst_bluetoothaudiosource_debug_category


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

static void _audio_source_callback_state_changed (const bluetoothaudiosource_state_t state, void *user_data)
{
  GstBluetoothAudioSource *bluetoothaudiosource = (GstBluetoothAudioSource*)user_data;

  g_assert (bluetoothaudiosource != NULL);

  switch (state) {
  case BLUETOOTHAUDIOSOURCE_STATE_CONNECTED:
    GST_INFO_OBJECT (bluetoothaudiosource, "Bluetooth audio source is now connected!");
    break;
  case BLUETOOTHAUDIOSOURCE_STATE_CONNECTED_BAD:
    GST_ERROR_OBJECT (bluetoothaudiosource, "Invalid device connected - cant't play!");
    break;
  case BLUETOOTHAUDIOSOURCE_STATE_DISCONNECTED:
    GST_WARNING_OBJECT (bluetoothaudiosource, "Bluetooth Audio source is now disconnected!");
    break;
  case BLUETOOTHAUDIOSOURCE_STATE_READY:
    GST_INFO_OBJECT (bluetoothaudiosource, "Bluetooth Audio source now ready!");
    break;
  case BLUETOOTHAUDIOSOURCE_STATE_STREAMING:
    GST_INFO_OBJECT (bluetoothaudiosource, "Bluetooth Audio source is now streaming!");
    break;
  default:
    break;
  }
}

static void _audio_source_callback_operational_state_updated (const uint8_t running, void *user_data)
{
  GstBluetoothAudioSource *bluetoothaudiosource = (GstBluetoothAudioSource*)user_data;

  g_assert (bluetoothaudiosource != NULL);

  if (running) {

    GST_INFO_OBJECT (bluetoothaudiosource, "Bluetooth Audio Source service now available");

    /* Register for the source updates... */
    if (bluetoothaudiosource_register_state_changed_callback (&_audio_source_callback_state_changed, bluetoothaudiosource) != 0) {
      GST_ERROR_OBJECT (bluetoothaudiosource, "bluetoothaudiosource_register_state_changed_callback() failed");
    } else {
      GST_INFO_OBJECT (bluetoothaudiosource, "Successfully registered to Bluetooth Audio Source status update callback");
    }
  } else {
    GST_INFO_OBJECT (bluetoothaudiosource, "Bluetooth Audio Source service is now unvailable");
  }
}

static void _audio_source_initialize (GstBluetoothAudioSource *bluetoothaudiosource)
{
  g_mutex_init (&bluetoothaudiosource->lock);

  /* Register for the Bluetooth Audio Source service updates... */
  if (bluetoothaudiosource_register_operational_state_update_callback (&_audio_source_callback_operational_state_updated, bluetoothaudiosource) != 0) {
    GST_ERROR_OBJECT (bluetoothaudiosource, "bluetoothaudiosource_register_operational_state_update_callback() failed");
  } else {
    GST_INFO_OBJECT (bluetoothaudiosource, "Successfully registered to Bluetooth Audio Source service operational callback");
  }

  _audio_source_install_dispose_handler ();
}

static void _audio_source_dispose (GstBluetoothAudioSource *bluetoothaudiosource)
{
  bluetoothaudiosource_unregister_state_changed_callback (&_audio_source_callback_state_changed);
  bluetoothaudiosource_unregister_operational_state_update_callback (&_audio_source_callback_operational_state_updated);
}


/* prototypes */

static void gst_bluetoothaudiosource_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_bluetoothaudiosource_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_bluetoothaudiosource_dispose (GObject *object);
static void gst_bluetoothaudiosource_finalize (GObject *object);

static gboolean gst_bluetoothaudiosource_open (GstAudioSrc *source);
static gboolean gst_bluetoothaudiosource_prepare (GstAudioSrc *source, GstAudioRingBufferSpec *spec);
static gboolean gst_bluetoothaudiosource_unprepare (GstAudioSrc *source);
static gboolean gst_bluetoothaudiosource_close (GstAudioSrc *source);
static guint gst_bluetoothaudiosource_read (GstAudioSrc *src, gpointer data, guint length, GstClockTime *timestamp);
static guint gst_bluetoothaudiosource_delay (GstAudioSrc *source);
static void gst_bluetoothaudiosource_reset (GstAudioSrc *source);


enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_bluetoothaudiosource_src_template =
GST_STATIC_PAD_TEMPLATE ("source",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,"
      "format=S16LE,"
      "rate={32000,44100,48000}," /* Standard sample rates required to be supported by all source devices.*/
      "channels=[1,2],"
      "layout=interleaved")
    );

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstBluetoothAudioSource, gst_bluetoothaudiosource, GST_TYPE_AUDIO_SRC,
  GST_DEBUG_CATEGORY_INIT (gst_bluetoothaudiosource_debug_category, "bluetoothaudiosource", 0, "debug category for bluetoothaudiosource element"));

static void gst_bluetoothaudiosource_class_init (GstBluetoothAudioSourceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAudioSrcClass *audio_source_class = GST_AUDIO_SRC_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
      &gst_bluetoothaudiosource_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Audio Source (Bluetooth)", "Source/Audio", "Input from Bluetooth audio device", "Metrological");

  gobject_class->set_property = gst_bluetoothaudiosource_set_property;
  gobject_class->get_property = gst_bluetoothaudiosource_get_property;
  gobject_class->dispose = gst_bluetoothaudiosource_dispose;
  gobject_class->finalize = gst_bluetoothaudiosource_finalize;

  audio_source_class->open = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosource_open);
  audio_source_class->prepare = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosource_prepare);
  audio_source_class->unprepare = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosource_unprepare);
  audio_source_class->close = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosource_close);
  audio_source_class->read = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosource_read);
  audio_source_class->delay = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosource_delay);
  audio_source_class->reset = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosource_reset);
}

/* implementation */

static void gst_bluetoothaudiosource_init (GstBluetoothAudioSource *bluetoothaudiosource)
{
  GST_DEBUG_OBJECT (bluetoothaudiosource, "init");

  _audio_source_initialize (bluetoothaudiosource);
}

void gst_bluetoothaudiosource_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (object);

  GST_DEBUG_OBJECT (bluetoothaudiosource, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void gst_bluetoothaudiosource_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (object);

  GST_DEBUG_OBJECT (bluetoothaudiosource, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void gst_bluetoothaudiosource_dispose (GObject *object)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (object);

  GST_DEBUG_OBJECT (bluetoothaudiosource, "dispose");

  G_OBJECT_CLASS (gst_bluetoothaudiosource_parent_class)->dispose (object);
}

void gst_bluetoothaudiosource_finalize (GObject *object)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (object);

  GST_DEBUG_OBJECT (bluetoothaudiosource, "finalize");

  g_mutex_clear (&bluetoothaudiosource->lock);

  G_OBJECT_CLASS (gst_bluetoothaudiosource_parent_class)->finalize (object);
}

/* open the device with given specs */
static gboolean gst_bluetoothaudiosource_open (GstAudioSrc *source)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (source);
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosource, "open");

  return result;
}

/* prepare resources and state to operate with the given specs */
static gboolean gst_bluetoothaudiosource_prepare (GstAudioSrc *source, GstAudioRingBufferSpec *spec)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (source);
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosource, "prepare");

  return result;
}

/* undo anything that was done in prepare() */
static gboolean gst_bluetoothaudiosource_unprepare (GstAudioSrc *source)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (source);
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosource, "unprepare");

  return result;
}

/* close the device */
static gboolean gst_bluetoothaudiosource_close (GstAudioSrc *source)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (source);
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosource, "close");

  return result;
}

/* write samples to the device */
static guint gst_bluetoothaudiosource_read (GstAudioSrc *source, gpointer data, guint length, GstClockTime *timestamp)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (source);

  GST_DEBUG_OBJECT (bluetoothaudiosource, "read");

  return 0;
}

/* get number of samples queued in the device */
static guint gst_bluetoothaudiosource_delay (GstAudioSrc *source)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (source);

  // GST_DEBUG_OBJECT (bluetoothaudiosource, "delay");

  return 0;
}

/* reset the audio device, unblock from a write */
static void gst_bluetoothaudiosource_reset (GstAudioSrc *source)
{
  GstBluetoothAudioSource *bluetoothaudiosource = GST_BLUETOOTHAUDIOSOURCE (source);

  GST_DEBUG_OBJECT (bluetoothaudiosource, "reset");
}

static gboolean plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, "bluetoothaudiosource", GST_RANK_PRIMARY, GST_TYPE_BLUETOOTHAUDIOSOURCE);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "gstbluetoothaudiosource"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gstbluetoothaudiosource"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://github.com/WebPlatformForEmbedded/gstbluetoothaudiosource"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bluetoothaudiosource,
    "bluetoothaudisource plugin",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

