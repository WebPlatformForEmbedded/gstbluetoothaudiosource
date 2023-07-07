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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_BLUETOOTHAUDIOSRC_H_
#define _GST_BLUETOOTHAUDIOSRC_H_

#include <gst/audio/gstaudiosrc.h>
#include <WPEFramework/bluetoothaudiosource/bluetoothaudiosource.h>

G_BEGIN_DECLS

#define GST_TYPE_BLUETOOTHAUDIOSRC   (gst_bluetoothaudiosrc_get_type())
#define GST_BLUETOOTHAUDIOSRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BLUETOOTHAUDIOSRC,GstBluetoothAudioSrc))
#define GST_BLUETOOTHAUDIOSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BLUETOOTHAUDIOSRC,GstBluetoothAudioSrcClass))
#define GST_IS_BLUETOOTHAUDIOSRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BLUETOOTHAUDIOSRC))
#define GST_IS_BLUETOOTHAUDIOSRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BLUETOOTHAUDIOSRC))

typedef struct _GstBluetoothAudioSrc GstBluetoothAudioSrc;
typedef struct _GstBluetoothAudioSrcClass GstBluetoothAudioSrcClass;

struct _GstBluetoothAudioSrc
{
  GstAudioSrc base_bluetoothaudiosrc;

  // private:
  bluetoothaudiosource_sink_t sink_callbacks;

  // private:
  guint8* buffer;
  guint32 buffer_size;
  guint32 buffer_write_offset;

  GMutex lock;
};

struct _GstBluetoothAudioSrcClass
{
  GstAudioSrcClass base_bluetoothaudiosrc_class;
};

GType gst_bluetoothaudiosrc_get_type (void);

G_END_DECLS

#endif // _GST_BLUETOOTHAUDIOSRC_H_
