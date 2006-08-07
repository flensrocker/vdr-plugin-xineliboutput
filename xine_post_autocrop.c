/*
 * Copyright (C) 2006 the xine project
 * 
 * This file is part of xine, a free video player.
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: 
 *
 * autocrop video filter by Petri Hintukainen 25/03/2006
 *
 * Automatically crop 4:3 letterbox frames to 16:9
 * 
 * based on expand.c
 *
 */

#include <xine/xine_internal.h>
#include <xine/post.h>


/*
 * Constants
 */

#define TRESHOLD       (0x1f)            /* "black" treshold value for Y-plane 
					    borders are not always black) */
#define TRESHOLD32     (0x1f1f1f1f)
#define TRESHOLD64     (UINT64_C(0x1f1f1f1f1f1f1f1f)) 
#define TRESHOLD_INV   (~(TRESHOLD))
#define TRESHOLD32_INV (~(TRESHOLD32))   
#define TRESHOLD64_INV (~(TRESHOLD64))
#define UVBLACK        (0x80)            /* "black" for U/V planes  */
#define UVBLACK32      (0x80808080)      /* (UVBLACK*0x01010101)    */
#define UVBLACK64      (UINT64_C(0x8080808080808080))


/*
 *    Open/close
 */

static void autocrop_dispose(post_plugin_t *this_gen)
{
  if (_x_post_dispose(this_gen)) 
    free(this_gen);
}

static post_plugin_t *autocrop_open_plugin(post_class_t *class_gen, 
					    int inputs,
					    xine_audio_port_t **audio_target,
					    xine_video_port_t **video_target)
{
  return NULL;
}


/*
 *    Plugin class
 */

static char *autocrop_get_identifier(post_class_t *class_gen)
{
  return "autocrop";
}

static char *autocrop_get_description(post_class_t *class_gen)
{
  return "Crop letterboxed 4:3 video to 16:9";
}

static void autocrop_class_dispose(post_class_t *class_gen)
{
  free(class_gen);
}

static void *autocrop_init_plugin(xine_t *xine, void *data)
{
  post_class_t *class = (post_class_t*)malloc(sizeof(post_class_t));
  
  if(class) {
    class->open_plugin     = autocrop_open_plugin;
    class->get_identifier  = autocrop_get_identifier;
    class->get_description = autocrop_get_description;
    class->dispose         = autocrop_class_dispose;
  }

  return class;
}


static post_info_t info = { XINE_POST_TYPE_VIDEO_FILTER };

const plugin_info_t xine_plugin_info[] __attribute__((visibility("default"))) =
{
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_POST, 9, "autocrop", XINE_VERSION_CODE, &info, &autocrop_init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
