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

#include <libgen.h>
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

static void     init                 (void);
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
    init,  /* init_proc */
    NULL,  /* quit_proc */
    query, /* query proc */
    run,   /* run_proc */
  };

MAIN ()


static void
init (void)
{
  phf_binary = "photoflow";
#if defined(__APPLE__) && defined (__MACH__)
  phf_binary = "/Applications/photoflow.app/Contents/MacOS/photoflow";
  //phf_binary = "open -W /Applications/photoflow.app --args";
#endif
#ifdef WIN32
  char* user_path = getenv("ProgramFiles");
  if( user_path ) phf_binary = std::string(user_path) + "\\photoflow\\bin\\photoflow.exe";
#endif
  char* phf_path = getenv("PHOTOFLOW_PATH");
  if( phf_path ) phf_binary = phf_path;

  gchar* exec_path = g_strdup(phf_binary.c_str());

  printf("file-photoflow::init() called, exec_path=%s\n",exec_path);

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
  gchar    *argv[]           = { exec_path, "--version", NULL };
  gchar    *photoflow_stdout = NULL;
  gboolean  have_photoflow   = FALSE;
  gint      i;

  if (g_spawn_sync (NULL,
		    argv,
		    NULL,
		    (GSpawnFlags)(G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH),
		    NULL,
		    NULL,
		    &photoflow_stdout,
		    NULL,
		    NULL,
		    NULL)) {
    gint major, minor, patch;

    printf("stdout:\n%s\n",photoflow_stdout);
    gchar* version_str = strstr(photoflow_stdout, "this is photoflow");
    if(version_str) {
      int nread = sscanf (version_str,
			  "this is photoflow %d.%d.%d",
			  &major, &minor, &patch);
      printf("nread: %d\n",nread);
      if ( nread == 3) {
        printf("Photoflow version: %d.%d.%d\n", major, minor, patch);
        if( major >= 0 && minor >= 2 && patch >= 8 ) {
          have_photoflow = TRUE;
        }
      }
    }

    g_free (photoflow_stdout);
  } else {
    printf("file-photoflow::init(): failed to run photoflow (%s)\n",phf_binary.c_str());
  }

  g_free (exec_path);

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
      //#ifdef HAVE_GIMP_REGISTER_FILE_HANDLER_RAW_FUNCTION
      printf("gimp_register_file_handler_raw (%s)\n",
	     format->load_proc);
      gimp_register_file_handler_raw (format->load_proc);
      //#endif
      gimp_register_magic_load_handler (format->load_proc,
                                        format->extensions,
                                        "",
                                        format->magic);

      //gimp_register_thumbnail_loader (format->load_proc, LOAD_THUMB_PROC);
    }
}

static void
query (void)
{
  /* query() is run only the first time for efficiency. Yet this plugin
   * is dependent on the presence of darktable which may be installed
   * or uninstalled between GIMP startups. Therefore we should move the
   * usual gimp_install_procedure() to init() so that the check is done
   * at every startup instead.
   */
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

  run_mode = (GimpRunMode)param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

  phf_binary = "photoflow";
#if defined(__APPLE__) && defined (__MACH__)
  //phf_binary = "/Applications/photoflow.app/Contents/MacOS/photoflow";
  phf_binary = "open -W /Applications/photoflow.app --args";
#endif
#ifdef WIN32
  char* user_path = getenv("ProgramFiles");
  if( user_path ) phf_binary = std::string(user_path) + "\\photoflow\\bin\\photoflow.exe";
#endif
  char* phf_path = getenv("PHOTOFLOW_PATH");
  if( phf_path ) phf_binary = phf_path;

  printf("file-photoflow: run() procedure called, name=%s, phf_binary=%s\n", name, phf_binary.c_str());

  /* check if the format passed is actually supported & load */
  for (i = 0; i < G_N_ELEMENTS (file_formats); i++)
    {
      const FileFormat *format = &file_formats[i];

      if (format->load_proc && ! strcmp (name, format->load_proc))
        {
	  printf("format: \"%s\", load_image (%s, run_mode, &error)\n", format->file_type, param[1].data.d_string);
          image_ID = load_image (param[1].data.d_string, run_mode, &error);

          if (image_ID != -1)
            {
              *nreturn_vals = 2;
              values[1].type         = GIMP_PDB_IMAGE;
              values[1].data.d_image = image_ID;
            }
          else
            {
	      printf("file-photoflow::run(): image_ID = -1\n");
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

  if (i == G_N_ELEMENTS (file_formats)) {
    printf("file-photoflow::run(): i == G_N_ELEMENTS (file_formats)\n");
    status = GIMP_PDB_CALLING_ERROR;
  }

  printf("Execution of file-photoflow plug-in finished, status=%d (success=%d)\n",(int)status, (int)GIMP_PDB_SUCCESS);

  if (status != GIMP_PDB_SUCCESS && error)
    {
      *nreturn_vals = 2;
      values[1].type           = GIMP_PDB_STRING;
      values[1].data.d_string  = error->message;
      printf("Execution of file-photoflow plug-in failed\n");
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

  printf("file-photoflow: load_image() called\n");

  gimp_progress_init_printf (_("Opening '%s'"),
                             gimp_filename_to_utf8 (filename));

  gchar* tmp_path = g_strdup(filename_out);
  char* tmpdir = g_path_get_dirname( tmp_path );
  g_mkdir( tmpdir, 0700 );
  g_free(tmp_path);
  g_free(tmpdir);

#if defined(__APPLE__) && defined (__MACH__)
  char cmd[1000];
  sprintf(cmd,"%s --plugin \"%s\" \"%s\" \"%s\"", phf_binary.c_str(),
	  filename, filename_out, pfiname);
  printf ("Starting photoflow: %s\n",cmd);
  //system("which photoflow");
  system(cmd);
#else
  gchar *argv[] =
    {
      (gchar*)(phf_binary.c_str()),
      "--plugin",
      (gchar *) filename,
      (gchar *) filename_out,
      (gchar *) pfiname,
      NULL
    };
  if (g_spawn_sync (NULL, argv, NULL,
		    //                     G_SPAWN_STDOUT_TO_DEV_NULL |
		    (GSpawnFlags)(G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH),
                    NULL, NULL, &photoflow_stdout,
                    NULL, NULL, error))
#endif
  {
    gboolean test = g_file_test (filename_out,G_FILE_TEST_EXISTS);
    std::cout<<"g_file_test ("<<filename_out<<",G_FILE_TEST_EXISTS): "<<test<<std::endl;
    if( test == TRUE ) {
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
      } else {
        printf("file-photoflow::load_image(): failed to load \"%s\"\n",
            filename_out);
      }
    } else {
      printf("file-photoflow::load_image(): file \"%s\" not found\n",
          filename_out);
    }
  }

  printf ("photoflow_stdout: %p\n", (void*)photoflow_stdout);
  if (photoflow_stdout) printf ("%s\n", photoflow_stdout);
  g_free(photoflow_stdout);

  printf ("filename_out: %s\n", filename_out);
  printf ("pfiname: %s\n", pfiname);

  //g_unlink (filename_out);
  g_unlink (pfiname);
  g_free (filename_out);
  g_free (pfiname);

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
                    (GSpawnFlags)(G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH),
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
