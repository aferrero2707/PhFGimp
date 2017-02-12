/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * file-photoflow.c -- raw file format plug-in that uses photoflow
 * Copyright (C) 2012 Simon Budig <simon@gimp.org>
 * Copyright (C) 2016 Tobias Ellinghaus <me@houz.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fstream>
#include <sstream>

#include <glib/gstdio.h>
#include <glib/gi18n.h>


#define INIT_I18N()

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

/*#include "libgimp/stdplugins-intl.h"*/

#include "file-formats.h"


#define LOAD_THUMB_PROC "file-raw-load-thumb"

static std::string phf_binary;

static void     query                (void);
static void     run                  (const gchar      *name,
                                      gint              nparams,
                                      const GimpParam  *param,
                                      gint             *nreturn_vals,
                                      GimpParam       **return_vals);
static gint32   load_image           (const gchar      *filename,
                                      GimpRunMode       run_mode,
                                      GError          **error);

static gint32   load_thumbnail_image (const gchar      *filename,
                                      gint             thumb_size,
                                      gint             *width,
                                      gint             *height,
                                      GError          **error);

const GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query proc */
  run,   /* run_proc */
};

MAIN ()


void
query (void)
{
  phf_binary = "photoflow";
  char* phf_path = getenv("PHOTOFLOW_PATH");
  if( phf_path ) phf_binary = phf_path;

  printf("file-photoflow query() called, phf_binary=%s\n",phf_binary.c_str());

  static const GimpParamDef load_args[] =
  {
    { GIMP_PDB_INT32,  "run-mode",     "The run mode { RUN-INTERACTIVE (0) }" },
    { GIMP_PDB_STRING, "filename",     "The name of the file to load." },
    { GIMP_PDB_STRING, "raw-filename", "The name entered" },
  };

  static const GimpParamDef load_return_vals[] =
  {
    { GIMP_PDB_IMAGE,  "image",        "Output image" }
  };

  static const GimpParamDef thumb_args[] =
  {
    { GIMP_PDB_STRING, "filename",     "The name of the file to load"  },
    { GIMP_PDB_INT32,  "thumb-size",   "Preferred thumbnail size"      }
  };

  static const GimpParamDef thumb_return_vals[] =
  {
    { GIMP_PDB_IMAGE,  "image",        "Thumbnail image"               },
    { GIMP_PDB_INT32,  "image-width",  "Width of full-sized image"     },
    { GIMP_PDB_INT32,  "image-height", "Height of full-sized image"    }
  };

  /* check if photoflow is installed
   * TODO: allow setting the location of the executable in preferences
   */
  gchar    *argv[]           = { phf_binary.c_str(), "--version", NULL };
  gchar    *photoflow_stdout = NULL;
  gboolean  have_photoflow   = FALSE;
  gint      i;

  if (g_spawn_sync (NULL,
      argv,
      NULL,
      G_SPAWN_STDERR_TO_DEV_NULL |
      G_SPAWN_SEARCH_PATH,
      NULL,
      NULL,
      &photoflow_stdout,
      NULL,
      NULL,
      NULL)) {
    gint major, minor, patch;

    if (sscanf (photoflow_stdout,
        "this is photoflow %d.%d.%d",
        &major, &minor, &patch) == 3) {
      if( major >= 0 && minor >= 2 && patch >= 8 ) {
        have_photoflow = TRUE;
      }
    }

    g_free (photoflow_stdout);
  }

  if (! have_photoflow)
    return;

  /*
  gimp_install_procedure (LOAD_THUMB_PROC,
                          "Load thumbnail from a raw image via photoflow",
                          "This plug-in loads a thumbnail from a raw image by calling photoflow-cli.",
                          "Tobias Ellinghaus",
                          "Tobias Ellinghaus",
                          "2016",
                          NULL,
                          NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (thumb_args),
                          G_N_ELEMENTS (thumb_return_vals),
                          thumb_args, thumb_return_vals);
  */
  for (i = 0; i < G_N_ELEMENTS (file_formats); i++)
    {
      const FileFormat *format = &file_formats[i];

      gimp_install_procedure (format->load_proc,
                              format->load_blurb,
                              format->load_help,
                              "Tobias Ellinghaus",
                              "Tobias Ellinghaus",
                              "2016",
                              format->file_type,
                              NULL,
                              GIMP_PLUGIN,
                              G_N_ELEMENTS (load_args),
                              G_N_ELEMENTS (load_return_vals),
                              load_args, load_return_vals);

      gimp_register_file_handler_mime (format->load_proc,
                                       format->mime_type);
      gimp_register_magic_load_handler (format->load_proc,
                                        format->extensions,
                                        "",
                                        format->magic);

      gimp_register_thumbnail_loader (format->load_proc, LOAD_THUMB_PROC);
    }
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[6];
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  GimpRunMode        run_mode;
  gint               image_ID;
  GError            *error = NULL;
  gint               i;

  INIT_I18N ();

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

  phf_binary = "photoflow";
  char* phf_path = getenv("PHOTOFLOW_PATH");
  if( phf_path ) phf_binary = phf_path;

  printf("file-photoflow: run() procedure called, name=%s, phf_binary=%s\n", name, phf_binary.c_str());

  /* check if the format passed is actually supported & load */
  for (i = 0; i < G_N_ELEMENTS (file_formats); i++)
    {
      const FileFormat *format = &file_formats[i];

      if (format->load_proc && ! strcmp (name, format->load_proc))
        {
          image_ID = load_image (param[1].data.d_string, run_mode, &error);

          if (image_ID != -1)
            {
              *nreturn_vals = 2;
              values[1].type         = GIMP_PDB_IMAGE;
              values[1].data.d_image = image_ID;
            }
          else
            {
              status = GIMP_PDB_EXECUTION_ERROR;
            }

          break;
        }
      else if (! strcmp (name, LOAD_THUMB_PROC))
        {
          gint width  = 0;
          gint height = 0;

          image_ID = load_thumbnail_image (param[0].data.d_string,
                                           param[1].data.d_int32,
                                           &width,
                                           &height,
                                           &error);

          if (image_ID != -1)
            {
              *nreturn_vals = 6;
              values[1].type         = GIMP_PDB_IMAGE;
              values[1].data.d_image = image_ID;
              values[2].type         = GIMP_PDB_INT32;
              values[2].data.d_int32 = width;
              values[3].type         = GIMP_PDB_INT32;
              values[3].data.d_int32 = height;
              values[4].type         = GIMP_PDB_INT32;
              values[4].data.d_int32 = GIMP_RGB_IMAGE;
              values[5].type         = GIMP_PDB_INT32;
              values[5].data.d_int32 = 1; /* num_layers */
            }
          else
            {
              status = GIMP_PDB_EXECUTION_ERROR;
            }

          break;
        }
    }

  if (i == G_N_ELEMENTS (file_formats))
    status = GIMP_PDB_CALLING_ERROR;

  if (status != GIMP_PDB_SUCCESS && error)
    {
      *nreturn_vals = 2;
      values[1].type           = GIMP_PDB_STRING;
      values[1].data.d_string  = error->message;
    }

  values[0].data.d_status = status;
}

static gint32
load_image (const gchar  *filename,
            GimpRunMode   run_mode,
            GError      **error)
{
  gint32  image_ID        = -1;
  gchar  *filename_out    = gimp_temp_name ("tif");
  gchar  *pfiname         = gimp_temp_name ("pfi");

  gchar *photoflow_stdout = NULL;

  /* linear sRGB for now as GIMP uses that internally in many places anyway */
  gchar *argv[] =
    {
        phf_binary.c_str(),
      "--plugin",
      (gchar *) filename,
      (gchar *) filename_out,
      (gchar *) pfiname,
      NULL
    };

  gimp_progress_init_printf (_("Opening '%s'"),
                             gimp_filename_to_utf8 (filename));

  printf ("Starting photoflow...\n");
  //system("photoflow");
/**/
  if (g_spawn_sync (NULL,
                    argv,
                    NULL,
//                     G_SPAWN_STDOUT_TO_DEV_NULL |
                    G_SPAWN_STDERR_TO_DEV_NULL |
                    G_SPAWN_SEARCH_PATH,
                    NULL,
                    NULL,
                    &photoflow_stdout,
                    NULL,
                    NULL,
                    error))
    {
      image_ID = gimp_file_load (run_mode, filename_out, filename_out);
      if (image_ID != -1) {
        gimp_image_set_filename (image_ID, filename);
        gint nlayers;
        gint* layers = gimp_image_get_layers(image_ID, &nlayers);
        gint layer = layers[nlayers-1];
        std::ifstream t;
        std::stringstream strstr;
        t.open( pfiname );
        strstr << t.rdbuf();
        char* buffer = strdup( strstr.str().c_str() );
        t.close();

        GimpParasite *cfg_parasite;
        cfg_parasite = gimp_parasite_new("phf-config",
            GIMP_PARASITE_PERSISTENT, strlen(buffer), buffer);
        gimp_item_attach_parasite(layer, cfg_parasite);
        gimp_parasite_free(cfg_parasite);
      }
    }
/**/
  printf ("photoflow_stdout: %p\n", (void*)photoflow_stdout);
  if (photoflow_stdout) printf ("%s\n", photoflow_stdout);
  g_free(photoflow_stdout);

  printf ("filename_out: %s\n", filename_out);
  printf ("pfiname: %s\n", pfiname);

  g_unlink (filename_out);
  g_unlink (pfiname);
  g_free (filename_out);

  gimp_progress_update (1.0);

  return image_ID;
}

static gint32
load_thumbnail_image (const gchar   *filename,
                      gint           thumb_size,
                      gint          *width,
                      gint          *height,
                      GError       **error)
{
  gint32  image_ID         = -1;
  gchar  *filename_out     = gimp_temp_name ("jpg");
  gchar  *size             = g_strdup_printf ("%d", thumb_size);
  GFile  *lua_file         = gimp_data_directory_file ("file-photoflow",
                                                       "get-size.lua",
                                                       NULL);
  gchar  *lua_script       = g_file_get_path (lua_file);
  gchar  *lua_quoted       = g_shell_quote (lua_script);
  gchar  *lua_cmd          = g_strdup_printf ("dofile(%s)", lua_quoted);
  gchar  *photoflow_stdout = NULL;

  gchar *argv[] =
    {
      "photoflow-cli",
      (gchar *) filename, filename_out,
      "--width",          size,
      "--height",         size,
      "--hq",             "false",
      "--core",
      "--conf",           "plugins/lighttable/export/icctype=3",
      "--luacmd",         lua_cmd,
      NULL
    };

  g_object_unref (lua_file);
  g_free (lua_script);
  g_free (lua_quoted);

  gimp_progress_init_printf (_("Opening thumbnail for '%s'"),
                             gimp_filename_to_utf8 (filename));

  *width = *height = thumb_size;

  if (g_spawn_sync (NULL,
                    argv,
                    NULL,
                    G_SPAWN_STDERR_TO_DEV_NULL |
                    G_SPAWN_SEARCH_PATH,
                    NULL,
                    NULL,
                    &photoflow_stdout,
                    NULL,
                    NULL,
                    error))
    {
      gimp_progress_update (0.5);

      image_ID = gimp_file_load (GIMP_RUN_NONINTERACTIVE,
                                 filename_out,
                                 filename_out);
      if (image_ID != -1)
        {
          /* the size reported by raw files isn't precise,
           * but it should be close enough to get an idea.
           */
          gchar *start_of_size = g_strstr_len (photoflow_stdout,
                                               -1,
                                               "[dt4gimp]");
          if (start_of_size)
            sscanf (start_of_size, "[dt4gimp] %d %d", width, height);

          /* is this needed for thumbnails? */
          gimp_image_set_filename (image_ID, filename);
        }
    }

  gimp_progress_update (1.0);

  g_unlink (filename_out);
  g_free (filename_out);
  g_free (size);
  g_free (lua_cmd);
  g_free (photoflow_stdout);

  return image_ID;
}
