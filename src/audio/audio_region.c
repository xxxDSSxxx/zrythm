/*
 * Copyright (C) 2018-2020 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/channel.h"
#include "audio/audio_region.h"
#include "audio/clip.h"
#include "audio/pool.h"
#include "audio/track.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/region.h"
#include "project.h"
#include "utils/audio.h"
#include "utils/dsp.h"
#include "utils/flags.h"
#include "utils/io.h"
#include "zrythm_app.h"

/**
 * Creates a ZRegion for audio data.
 *
 * FIXME First create the
 * audio on the pool and then pass the pool id here.
 *
 * @param pool_id The pool ID. This is used when
 *   creating clone regions (non-main) and must be
 *   -1 when creating a new clip.
 * @param filename Filename, if loading from
 *   file, otherwise NULL.
 * @param frames Float array, if loading from
 *   float array, otherwise NULL.
 * @param nframes Number of frames per channel.
 * @param clip_name Name of audio clip, if not
 *   loading from file.
 */
ZRegion *
audio_region_new (
  const int        pool_id,
  const char *     filename,
  const float *    frames,
  const long       nframes,
  const char *     clip_name,
  const channels_t channels,
  const Position * start_pos,
  int              track_pos,
  int              lane_pos,
  int              idx_inside_lane)
{
  ZRegion * self =
    calloc (1, sizeof (AudioRegion));
  ArrangerObject * obj =
    (ArrangerObject *) self;

  g_return_val_if_fail (
    start_pos && start_pos->bars > 0, NULL);

  self->id.type = REGION_TYPE_AUDIO;
  self->pool_id = -1;

  AudioClip * clip = NULL;
  int recording = 0;
  if (pool_id == -1)
    {
      if (filename)
        {
          clip =
            audio_clip_new_from_file (filename);
        }
      else if (frames)
        {
          g_return_val_if_fail (clip_name, NULL);
          clip =
            audio_clip_new_from_float_array (
              frames, nframes, channels, clip_name);
        }
      else
        {
          clip =
            audio_clip_new_recording (
              2, nframes, clip_name);
          recording = 1;
        }
      g_return_val_if_fail (clip, NULL);

      self->pool_id =
        audio_pool_add_clip (
          AUDIO_POOL, clip);
      g_warn_if_fail (self->pool_id > -1);
    }
  else
    {
      self->pool_id = pool_id;
      clip = AUDIO_POOL->clips[pool_id];
      g_warn_if_fail (clip && clip->frames);
    }

  /* set end pos to sample end */
  position_set_to_pos (
    &obj->end_pos, start_pos);
  position_add_frames (
    &obj->end_pos, clip->num_frames);

  /* init split points */
  self->split_points_size = 1;
  self->split_points =
    calloc (
      self->split_points_size, sizeof (Position));
  self->num_split_points = 0;

  /* init APs */
  self->aps_size = 2;
  self->aps =
    malloc (
      self->aps_size * sizeof (AutomationPoint *));

  /* init */
  region_init (
    self, start_pos, &obj->end_pos, track_pos,
    lane_pos, idx_inside_lane);

  (void) recording;
  g_warn_if_fail (audio_region_get_clip (self));
  /*if (!recording)*/
    /*audio_clip_write_to_pool (clip);*/

  return self;
}

#if 0
/**
 * Allocates the frame caches from the frames in
 * the clip.
 */
void
audio_region_init_frame_caches (
  AudioRegion * self,
  AudioClip *   clip)
{
#if 0
  /* copy the clip frames to the cache. */
  self->frames =
    realloc (
      self->frames,
      sizeof (float) *
        (size_t) clip->num_frames * clip->channels);
  self->num_frames = (size_t) clip->num_frames;
  dsp_copy (
    &self->frames[0], &clip->frames[0],
    (size_t) clip->num_frames * clip->channels);
#endif

  audio_region_update_channel_caches (self, clip);
}

/**
 * Updates the region's channel caches from the
 * region's frames.
 */
void
audio_region_update_channel_caches (
  ZRegion *   self,
  AudioClip * clip)
{
  /* copy the frames to the channel caches */
  for (unsigned int i = 0; i < clip->channels; i++)
    {
      self->ch_frames[i] =
        realloc (
          self->ch_frames[i],
          sizeof (float) *
            (size_t) clip->num_frames);
      for (size_t j = 0;
           j < (size_t) clip->num_frames; j++)
        {
          self->ch_frames[i][j] =
            clip->frames[j * clip->channels + i];
        }
    }
}
#endif

/**
 * Returns the audio clip associated with the
 * Region.
 */
AudioClip *
audio_region_get_clip (
  const ZRegion * self)
{
  g_return_val_if_fail (
    self->pool_id >= 0 &&
    self->id.type == REGION_TYPE_AUDIO,
    NULL);

  AudioClip * clip =
    AUDIO_POOL->clips[self->pool_id];

  g_return_val_if_fail (
    clip && clip->frames && clip->num_frames > 0,
    NULL);

  return clip;
}

/**
 * Sets the clip ID on the region and updates any
 * references.
 */
void
audio_region_set_clip_id (
  ZRegion * self,
  int       clip_id)
{
  self->pool_id = clip_id;

  /* TODO update identifier - needed? */
}

/**
 * Replaces the region's frames from \ref
 * start_frames with \ref frames.
 *
 * @param duplicate_clip Whether to duplicate the
 *   clip (eg, when other regions refer to it).
 * @param frames Frames, interleaved.
 */
void
audio_region_replace_frames (
  ZRegion * self,
  float *   frames,
  size_t    start_frame,
  size_t    num_frames,
  bool      duplicate_clip)
{
  AudioClip * clip = audio_region_get_clip (self);

  if (duplicate_clip)
    {
      g_warn_if_reached ();

      /* TODO delete */
      int prev_id = clip->pool_id;
      int id =
        audio_pool_duplicate_clip (
          AUDIO_POOL, clip->pool_id,
          F_NO_WRITE_FILE);
      g_return_if_fail (id != prev_id);
      clip = audio_pool_get_clip (AUDIO_POOL, id);
      g_return_if_fail (clip);

      self->pool_id = clip->pool_id;
    }

  dsp_copy (
    &clip->frames[start_frame * clip->channels],
    frames, num_frames * clip->channels);

  audio_clip_write_to_pool (clip, false);
}

/**
 * Frees members only but not the audio region itself.
 *
 * Regions should be free'd using region_free.
 */
void
audio_region_free_members (ZRegion * self)
{
}
