
#if HAVE_GIMP_2_9
#include <gegl.h>
#endif
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <libgimpbase/gimpbase.h>
#include <libgimpwidgets/gimpwidgets.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>

#include <tiffio.h>

#include <lcms2.h>

#include <gexiv2/gexiv2-metadata.h>

#include <libgen.h>

#include <fstream>
#include <sstream>
#include <iostream>

#define VERSION "0.2.5"

//#define HAVE_GIMP_2_9 1

static int
save_tiff (const char* path, GeglBuffer *input,
    const GeglRectangle *result, const Babl *format,
    void* iccdata, glong iccsize)
{
  gshort color_space, compression = COMPRESSION_NONE;
  gushort bits_per_sample, samples_per_pixel;
  gboolean has_alpha, alpha_is_premultiplied = FALSE;
  gushort sample_format, predictor = 0;
  gushort extra_types[1];
  glong rows_per_stripe = 1;
  gint bytes_per_pixel, bytes_per_row;
  const Babl *type, *model;
  gchar format_string[32];
  //const Babl *format;

  TIFF* tiff = TIFFOpen( path, "w" );

  g_return_val_if_fail(tiff != NULL, -1);

  printf("TIFF file %s opened\n", path);

  TIFFSetField(tiff, TIFFTAG_SUBFILETYPE, 0);
  TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);

  TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, result->width);
  TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, result->height);

  if( (iccdata!=NULL) && (iccsize>0) )
    TIFFSetField( tiff, TIFFTAG_ICCPROFILE, iccsize, iccdata );

  //format = gegl_buffer_get_format(input);

  model = babl_format_get_model(format);
  type = babl_format_get_type(format, 0);
/*
  if (model == babl_model("Y") || model == babl_model("Y'"))
  {
    has_alpha = FALSE;
    color_space = PHOTOMETRIC_MINISBLACK;
    //model = babl_model("Y'");
    samples_per_pixel = 1;
  }
  else if (model == babl_model("YA") || model == babl_model("Y'A"))
  {
    has_alpha = TRUE;
    alpha_is_premultiplied = FALSE;
    color_space = PHOTOMETRIC_MINISBLACK;
    //model = babl_model("Y'A");
    samples_per_pixel = 2;
  }
  else if (model == babl_model("YaA") || model == babl_model("Y'aA"))
  {
    has_alpha = TRUE;
    alpha_is_premultiplied = TRUE;
    color_space = PHOTOMETRIC_MINISBLACK;
    //model = babl_model("Y'aA");
    samples_per_pixel = 2;
  }
  else*/ if (model == babl_model("RGB")
#ifndef BABL_FLIPS_DISABLED
      || model == babl_model("R'G'B'")
#endif
      )
  {
    has_alpha = FALSE;
    color_space = PHOTOMETRIC_RGB;
    //model = babl_model("R'G'B'");
    samples_per_pixel = 3;
    predictor = 2;
  }
  else if (model == babl_model("RGBA")
#ifndef BABL_FLIPS_DISABLED
      || model == babl_model("R'G'B'A")
#endif
      )
  {
    has_alpha = TRUE;
    alpha_is_premultiplied = FALSE;
    color_space = PHOTOMETRIC_RGB;
    //model = babl_model("R'G'B'A");
    samples_per_pixel = 4;
    predictor = 2;
  }
  else if (model == babl_model("RaGaBaA")
#ifndef BABL_FLIPS_DISABLED
      || model == babl_model("R'aG'aB'aA")
#endif
      )
  {
    has_alpha = TRUE;
    alpha_is_premultiplied = TRUE;
    color_space = PHOTOMETRIC_RGB;
    //model = babl_model("R'aG'aB'aA");
    samples_per_pixel = 4;
    predictor = 2;
  }
  else
  {
    g_warning("color space not supported: %s", babl_get_name(model));

    has_alpha = TRUE;
    alpha_is_premultiplied = TRUE;
    color_space = PHOTOMETRIC_RGB;
    model = babl_model("R'aG'aB'aA");
    samples_per_pixel = 4;
    predictor = 2;
  }

  TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, color_space);
  TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
  TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  if (has_alpha)
  {
    if (alpha_is_premultiplied)
      extra_types[0] = EXTRASAMPLE_ASSOCALPHA;
    else
      extra_types[0] = EXTRASAMPLE_UNASSALPHA;

    TIFFSetField(tiff, TIFFTAG_EXTRASAMPLES, 1, extra_types);
  }

  if (predictor != 0)
  {
    if (compression == COMPRESSION_LZW)
      TIFFSetField(tiff, TIFFTAG_PREDICTOR, predictor);
    else if (compression == COMPRESSION_ADOBE_DEFLATE)
      TIFFSetField(tiff, TIFFTAG_PREDICTOR, predictor);
  }

  if (type == babl_type("u8"))
  {
    sample_format = SAMPLEFORMAT_UINT;
    bits_per_sample = 8;
  }
  else if (type == babl_type("half"))
  {
    sample_format = SAMPLEFORMAT_IEEEFP;
    bits_per_sample = 16;
  }
  else if (type == babl_type("u16"))
  {
    sample_format = SAMPLEFORMAT_UINT;
    bits_per_sample = 16;
  }
  else if (type == babl_type("float"))
  {
    sample_format = SAMPLEFORMAT_IEEEFP;
    bits_per_sample = 32;
  }
  else if (type == babl_type("u32"))
  {
    sample_format = SAMPLEFORMAT_UINT;
    bits_per_sample = 32;
  }
  else  if (type == babl_type("double"))
  {
    sample_format = SAMPLEFORMAT_IEEEFP;
    bits_per_sample = 64;
  }
  else
  {
    g_warning("sample format not supported: %s", babl_get_name(type));

    sample_format = SAMPLEFORMAT_UINT;
    type = babl_type("u8");
    bits_per_sample = 8;
  }

  TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
  TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, sample_format);

  TIFFSetField(tiff, TIFFTAG_COMPRESSION, compression);

  if ((compression == COMPRESSION_CCITTFAX3 ||
      compression == COMPRESSION_CCITTFAX4) &&
      (bits_per_sample != 1 || samples_per_pixel != 1))
  {
    g_critical("only monochrome pictures can be compressed "
        "with \"CCITT Group 4\" or \"CCITT Group 3\"");
    return -1;
  }

  g_snprintf(format_string, 32, "%s %s",
      babl_get_name(model), babl_get_name(type));

  format = babl_format(format_string);

  printf("BABL format: %s\n", format_string);

  /* "Choose RowsPerStrip such that each strip is about 8K bytes." */
  bytes_per_row = babl_format_get_bytes_per_pixel(format) * result->width;
  while (bytes_per_row * rows_per_stripe <= 8192)
    rows_per_stripe++;

  rows_per_stripe = MIN(rows_per_stripe, result->height);

  TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, rows_per_stripe);

  gint tile_width = result->width;
  gint tile_height = result->height;
  guchar *buffer;
  gint x, y;

  bytes_per_pixel = babl_format_get_bytes_per_pixel(format);
  bytes_per_row = bytes_per_pixel * tile_width;

  buffer = g_try_new(guchar, bytes_per_row * tile_height);

  printf("TIFF write buffer: %p\n", (void*)buffer);

  g_assert(buffer != NULL);

  for (y = result->y; y < result->y + tile_height; y += tile_height)
  {
    for (x = result->x; x < result->x + tile_width; x += tile_width)
    {
      GeglRectangle tile = { x, y, tile_width, tile_height };
      gint row;

      gegl_buffer_get(input, &tile, 1.0, format, buffer,
          GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

      for (row = y; row < y + tile_height; row++)
      {
        guchar *tile_row = buffer + (bytes_per_row * (row - y));
        gint written;

        written = TIFFWriteScanline(tiff, tile_row, row, 0);

        if(row==0) printf("scanline %d: written=%d\n", row, written);

        if (!written)
        {
          g_critical("failed a scanline write on row %d", row);
          continue;
        }
      }
    }
  }

  printf("Flushing TIFF data\n");
  TIFFFlushData(tiff);
  TIFFClose(tiff);

  g_free(buffer);
}



static gint
load_tiff(TIFF* tiff, GeglBuffer *output)
{
  gshort color_space, compression;
  gushort bits_per_sample, samples_per_pixel;
  gushort sample_format;
  gboolean has_alpha = FALSE;
  gboolean alpha_is_premultiplied = FALSE;
  gushort *extra_types = NULL;
  gushort nb_extras, planar_config;
  gboolean fallback_mode = FALSE;
  gchar format_string[32];
  guint width, height;

  g_return_val_if_fail(tiff != NULL, -1);

  if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width))
  {
    g_warning("could not get TIFF image width");
    return -1;
  }
  else if (!TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height))
  {
    g_warning("could not get TIFF image height");
    return -1;
  }

  bool is_linear = false;
  void* iccdata;
  glong iccsize;
  cmsHPROFILE iccprofile;

  if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &iccsize, &iccdata)) {
    iccprofile = cmsOpenProfileFromMem(iccdata, iccsize);
    _TIFFfree(iccdata);
    char tstr[1024];
    cmsGetProfileInfoASCII(iccprofile, cmsInfoDescription, "en", "US", tstr, 1024);
    cmsToneCurve *red_trc   = (cmsToneCurve*)cmsReadTag(iccprofile, cmsSigRedTRCTag);
    is_linear = cmsIsToneCurveLinear(red_trc);
    std::cout<<std::endl<<std::endl<<"load_tiff(): embedded profile: "<<tstr<<"  is_linear="<<is_linear<<std::endl<<std::endl;
    cmsCloseProfile( iccprofile );
  }

  TIFFGetFieldDefaulted(tiff, TIFFTAG_COMPRESSION, &compression);
  if (!TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &color_space))
  {
    g_warning("could not get photometric from TIFF image");
    if (compression == COMPRESSION_CCITTFAX3 ||
        compression == COMPRESSION_CCITTFAX4 ||
        compression == COMPRESSION_CCITTRLE  ||
        compression == COMPRESSION_CCITTRLEW)
    {
      g_message("assuming min-is-white (CCITT compressed)");
      color_space = PHOTOMETRIC_MINISWHITE;
    }
    else
    {
      g_message("assuming min-is-black");
      color_space = PHOTOMETRIC_MINISBLACK;
    }
  }

  TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  if (!TIFFGetField(tiff, TIFFTAG_EXTRASAMPLES, &nb_extras, &extra_types))
    nb_extras = 0;

  if (nb_extras > 0)
  {
    if (extra_types[0] == EXTRASAMPLE_ASSOCALPHA)
    {
      has_alpha = TRUE;
      alpha_is_premultiplied = TRUE;
      nb_extras--;
    }
    else if (extra_types[0] == EXTRASAMPLE_UNASSALPHA)
    {
      has_alpha = TRUE;
      alpha_is_premultiplied = FALSE;
      nb_extras--;
    }
    else if (extra_types[0] == EXTRASAMPLE_UNSPECIFIED)
    {
      has_alpha = TRUE;
      alpha_is_premultiplied = FALSE;
      nb_extras--;
    }
  }

  switch(color_space)
  {
  case PHOTOMETRIC_MINISBLACK:
  case PHOTOMETRIC_MINISWHITE:
    if (samples_per_pixel > 1 + nb_extras)
    {
      nb_extras = samples_per_pixel - 2;
      has_alpha = TRUE;
    }

    if (has_alpha)
    {
      if(alpha_is_premultiplied)
        g_strlcpy(format_string, "Y'aA ", 32);
      else
        g_strlcpy(format_string, "Y'A ", 32);
    }
    else
      g_strlcpy(format_string, "Y' ", 32);
    break;

  case PHOTOMETRIC_RGB:
    if (samples_per_pixel > 3 + nb_extras)
    {
      nb_extras = samples_per_pixel - 4;
      has_alpha = TRUE;
    }

    if (has_alpha)
    {
      if (alpha_is_premultiplied)
        g_strlcpy(format_string, "R'aG'aB'aA ", 32);
      else
        g_strlcpy(format_string, "R'G'B'A ", 32);
    }
    else {
#ifdef BABL_FLIPS_DISABLED
      g_strlcpy(format_string, "RGB ", 32);
#else
      if( is_linear )
        g_strlcpy(format_string, "RGB ", 32);
      else
        g_strlcpy(format_string, "R'G'B' ", 32);
#endif
    }
    break;

  default:
    fallback_mode = TRUE;
    break;
  }

  printf("is_linear: %d   format_string: %s\n", (int)is_linear, format_string);

  TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLEFORMAT, &sample_format);
  TIFFGetFieldDefaulted(tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);

  switch(bits_per_sample)
  {
  case 8:
    g_strlcat(format_string, "u8", 32);
    break;

  case 16:
    if (sample_format == SAMPLEFORMAT_IEEEFP)
      g_strlcat(format_string, "half", 32);
    else
      g_strlcat(format_string, "u16", 32);
    break;

  case 32:
    if (sample_format == SAMPLEFORMAT_IEEEFP)
      g_strlcat(format_string, "float", 32);
    else
      g_strlcat(format_string, "u32", 32);
    break;

  case 64:
    g_strlcat(format_string, "double", 32);
    break;

  default:
    fallback_mode = TRUE;
    break;
  }

  if (fallback_mode == TRUE)
    g_strlcpy(format_string, "R'aG'aB'aA u8", 32);

  TIFFGetFieldDefaulted(tiff, TIFFTAG_PLANARCONFIG, &planar_config);

  const Babl* format = babl_format(format_string);

  guint32 tile_width = (guint32) width;
  guint32 tile_height = 1;
  guchar *buffer;
  gint x, y;

  if (!TIFFIsTiled(tiff))
    buffer = g_try_new(guchar, TIFFScanlineSize(tiff));
  else
  {
    TIFFGetField(tiff, TIFFTAG_TILEWIDTH, &tile_width);
    TIFFGetField(tiff, TIFFTAG_TILELENGTH, &tile_height);

    buffer = g_try_new(guchar, TIFFTileSize(tiff));
  }

  g_assert(buffer != NULL);

  for (y = 0; y < height; y += tile_height)
  {
    for (x = 0; x < width; x += tile_width)
    {
      GeglRectangle tile = { x, y, tile_width, tile_height };

      if (TIFFIsTiled(tiff))
        TIFFReadTile(tiff, buffer, x, y, 0, 0);
      else
        TIFFReadScanline(tiff, buffer, y, 0);

      gegl_buffer_set(output, &tile, 0, format,
          (guchar *) buffer,
          GEGL_AUTO_ROWSTRIDE);
    }
  }

  g_free(buffer);
  return 0;
}






// Manage different versions of the GIMP API.
#define _gimp_item_is_valid gimp_item_is_valid
#define _gimp_image_get_item_position gimp_image_get_item_position
#if GIMP_MINOR_VERSION<=8
#define _gimp_item_get_visible gimp_drawable_get_visible
#else
#define _gimp_item_get_visible gimp_item_get_visible
#endif

static std::string phf_binary;
gboolean sendToGimpMode;


//static GimpPDBStatusType status = GIMP_PDB_SUCCESS;   // The plug-in return status.


static void     init                 (void);
static void     query                (void);
static void     run                  (const gchar      *name,
                                      gint              nparams,
                                      const GimpParam  *param,
                                      gint             *nreturn_vals,
                                      GimpParam       **return_vals);

//long pf_save_gimp_image(ufraw_data *uf, GtkWidget *widget);

GimpPlugInInfo PLUG_IN_INFO = {
    init,  /* init_procedure */
    NULL,  /* quit_procedure */
    query, /* query_procedure */
    run,   /* run_procedure */
};

MAIN()

static void 
init(void)
{
  phf_binary = "photoflow";
#if defined(__APPLE__) && defined (__MACH__)
  phf_binary = "/Applications/photoflow.app/Contents/MacOS/photoflow";
  //phf_binary = "open -W /Applications/photoflow.app --args";
#endif
#ifdef WIN32
  char* user_path = getenv("USERPROFILE");
  if( user_path ) phf_binary = std::string(user_path) + "\\photoflow\\bin\\photoflow.exe";
#endif
  char* phf_path = getenv("PHOTOFLOW_PATH");
  if( phf_path ) phf_binary = phf_path;

  printf("phf_gimp::query() called, exec_path=%s\n",phf_binary.c_str());

  /* check if photoflow is installed
   * TODO: allow setting the location of the executable in preferences
   */
  gchar    *argv[]           = { (gchar*)(phf_binary.c_str()), "--version", NULL };
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
    printf("phf_gimp::init(): failed to run photoflow (%s)\n",phf_binary.c_str());
  }

  if (! have_photoflow)
    return;

  static const GimpParamDef args[] = {
      {GIMP_PDB_INT32,    (gchar*)"run_mode", (gchar*)"Interactive"},
      {GIMP_PDB_IMAGE,    (gchar*)"image",    (gchar*)"Input image"},
      {GIMP_PDB_DRAWABLE, (gchar*)"drawable", (gchar*)"Input drawable (unused)"},
  };

  gimp_install_procedure("plug-in-photoflow",             // name
      "PhotoFlow",                    // blurb
      "PhotoFlow",                    // help
      "Andrea Ferrero", // author
      "Andrea Ferrero", // copyright
      "2016",                     // date
      "_PhotoFlow...",                // menu_path
      "RGB*",              // image_types
      GIMP_PLUGIN,                // type
      G_N_ELEMENTS(args),         // nparams
      0,                          // nreturn_vals
      args,                       // params
      0);                         // return_vals

  gimp_plugin_menu_register("plug-in-photoflow", "<Image>/Filters");
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

static bool
edit_current_layer_dialog()
{
  GtkWidget        *dialog;
  GtkWidget        *hbox;
  GtkWidget        *image;
  GtkWidget        *main_vbox;
  GtkWidget        *label;
  gchar            *text;
  bool  retval;

  GtkDialogFlags flags = (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT);
  dialog = gimp_dialog_new (_("Edit Current Layer"), "pfgimp-edit-current-layer-confirm",
      NULL, flags,
      gimp_standard_help_func,
      "pfgimp-edit-current-layer-confirm-dialog",

      _("Create new"), GTK_RESPONSE_CANCEL,
      _("Edit current"),     GTK_RESPONSE_OK,

      NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
      GTK_RESPONSE_OK,
      GTK_RESPONSE_CANCEL,
      -1);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gimp_window_set_transient (GTK_WINDOW (dialog));

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
      hbox, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("dialog-warning",
      GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (hbox), main_vbox, FALSE, FALSE, 0);
  gtk_widget_show (main_vbox);

  text = g_strdup ("This is a PhotoFlow layer.\nDo you want to continue\nediting this layer\nor create a new one?");
  label = gtk_label_new (text);
  g_free (text);

  gimp_label_set_attributes (GTK_LABEL (label),
      PANGO_ATTR_SCALE,  PANGO_SCALE_LARGE,
      PANGO_ATTR_WEIGHT, PANGO_WEIGHT_BOLD,
      -1);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (main_vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  gtk_widget_show (dialog);

  switch (gimp_dialog_run (GIMP_DIALOG (dialog)))
  {
  case GTK_RESPONSE_OK:
    retval = true;
    break;

  default:
    retval = false;
    break;
  }

  gtk_widget_destroy (dialog);

  return retval;
}


void run(const gchar *name,
    gint nparams,
    const GimpParam *param,
    gint *nreturn_vals,
    GimpParam **return_vals)
{
  GimpRunMode run_mode = (GimpRunMode)param[0].data.d_int32;

  int size;
  GimpPDBStatusType status = GIMP_PDB_CALLING_ERROR;

#if HAVE_GIMP_2_9
  gegl_init(NULL, NULL);
#endif

#if HAVE_GIMP_2_9
  GeglBuffer *buffer;
#else
  GimpDrawable *drawable;
  GimpPixelRgn pixel_region;
  int tile_height, row, nrows;
#endif
  //gint32 layer;

  static GimpParam return_values[1];
  *return_vals  = return_values;
  *nreturn_vals = 1;
  return_values[0].type = GIMP_PDB_STATUS;

  phf_binary = "photoflow";
#if defined(__APPLE__) && defined (__MACH__)
  //phf_binary = "/Applications/photoflow.app/Contents/MacOS/photoflow";
  phf_binary = "open -W /Applications/photoflow.app --args";
#endif
#ifdef WIN32
  char* user_path = getenv("USERPROFILE");
  if( user_path ) phf_binary = std::string(user_path) + "\\photoflow\\bin\\photoflow.exe";
#endif
  char* phf_path = getenv("PHOTOFLOW_PATH");
  if( phf_path ) phf_binary = phf_path;

  gimp_ui_init("pfgimp", FALSE);


  gchar  *filename    = gimp_temp_name ("tif");
  gchar* tmp_path = g_strdup(filename);
  char* tmpdir = g_path_get_dirname( tmp_path );
  g_mkdir( tmpdir, 0700 );
  g_free(tmp_path);
  g_free(tmpdir);

  std::cout<<"Starting PhotoFlow plug-in"<<std::endl;

  int image_id = param[1].data.d_drawable;
#if GIMP_MINOR_VERSION<=8
  gimp_tile_cache_ntiles(2*(gimp_image_width(image_id)/gimp_tile_width() + 1));
#endif

  // Retrieve the list of desired layers.
  int
  nb_layers = 0,
  *layers = gimp_image_get_layers(image_id,&nb_layers),
  active_layer_id = gimp_image_get_active_layer(image_id);
  int source_layer_id = active_layer_id;

  GimpParasite *phf_parasite = gimp_item_get_parasite( active_layer_id, "phf-config" );
  std::cout<<"PhF plug-in: phf_parasite="<<phf_parasite<<std::endl;
  bool replace_layer = false;
  std::string pfiname = "none";
  if( phf_parasite && gimp_parasite_data_size( phf_parasite ) > 0 &&
      gimp_parasite_data( phf_parasite ) != NULL ) {

    bool result = edit_current_layer_dialog();

    //Handle the response:
    switch(result) {
    case true: {
      glong size = gimp_parasite_data_size( phf_parasite );
      pfiname = gimp_temp_name ("pfi");
      std::ofstream t;
      t.open( pfiname.c_str() );
      t.write( (const char*)(gimp_parasite_data( phf_parasite )), size );
      t.close();
      replace_layer = true;
      break;
    }
    case false:
      break;
    }
  }

  if( replace_layer ) {
    if (active_layer_id>=0) {
      int i = 0; for (i = 0; i<nb_layers; ++i) if (layers[i]==active_layer_id) break;
      if (i<nb_layers - 1) source_layer_id = layers[i + 1];
      else source_layer_id = -1;
    }
  }
  std::cout<<"PhF plug-in: pfiname="<<pfiname<<"  replace_layer="<<replace_layer<<"  source_layer_id="<<source_layer_id<<std::endl;

  //GimpParasite *exif_parasite = gimp_image_parasite_find( image_id, "gimp-image-metadata" );
  GimpMetadata* exif_metadata = gimp_image_get_metadata( image_id );
  GimpParasite *icc_parasite  = gimp_image_parasite_find( image_id, "icc-profile" );
  glong iccsize = 0;
  void* iccdata = NULL;

  std::cout<<std::endl<<std::endl
      <<"image_id: "<<image_id
      <<"  ICC parasite: "<<icc_parasite
      <<"  EXIF metadata: "<<exif_metadata
      <<std::endl<<std::endl;

  if( icc_parasite && gimp_parasite_data_size( icc_parasite ) > 0 &&
      gimp_parasite_data( icc_parasite ) != NULL ) {
    iccsize = gimp_parasite_data_size( icc_parasite );
    iccdata = malloc( iccsize );
    memcpy( iccdata, gimp_parasite_data( icc_parasite ), iccsize );
  }

  cmsBool is_lin_gamma = false;
  std::string format = "R'G'B' float";

  int in_width = 0, in_height = 0;
  if( source_layer_id >= 0 ) {
    // Get input buffer
    in_width = gimp_drawable_width( source_layer_id );
    in_height = gimp_drawable_height( source_layer_id );
    gint rgn_x, rgn_y, rgn_width, rgn_height;
    if (!_gimp_item_is_valid(source_layer_id)) return;
    if (!gimp_drawable_mask_intersect(source_layer_id,&rgn_x,&rgn_y,&rgn_width,&rgn_height)) return;
    const int spectrum = (gimp_drawable_is_rgb(source_layer_id)?3:1) +
        (gimp_drawable_has_alpha(source_layer_id)?1:0);

    if( iccdata ) {
      cmsHPROFILE iccprofile = cmsOpenProfileFromMem( iccdata, iccsize );
      if( iccprofile ) {
        char tstr[1024];
        cmsGetProfileInfoASCII(iccprofile, cmsInfoDescription, "en", "US", tstr, 1024);
        cmsToneCurve *red_trc   = (cmsToneCurve*)cmsReadTag(iccprofile, cmsSigRedTRCTag);
        is_lin_gamma = cmsIsToneCurveLinear(red_trc);
        std::cout<<std::endl<<std::endl<<"embedded profile: "<<tstr<<"  is_lin_gamma="<<is_lin_gamma<<std::endl<<std::endl;
        cmsCloseProfile( iccprofile );
      }
    }

    GeglRectangle rect;
    gegl_rectangle_set(&rect,rgn_x,rgn_y,rgn_width,rgn_height);
    buffer = gimp_drawable_get_buffer(source_layer_id);
#ifdef BABL_FLIPS_DISABLED
    format = "RGB float";
#else
    format = is_lin_gamma ? "RGB float" : "R'G'B' float";
#endif
    save_tiff( filename, buffer, &rect, babl_format(format.c_str()), iccdata, iccsize );
    g_object_unref(buffer);

    if( exif_metadata ) {
      GFile* gfile = g_file_new_for_path( filename );
      gimp_metadata_save_to_file( exif_metadata, gfile, NULL );
      g_object_unref( exif_metadata );
      g_object_unref( gfile );
    }
  }

  //gimp_parasite_free(exif_parasite);
  //gimp_parasite_free(icc_parasite);

  std::cout<<"plug-in: run_mode="<<run_mode<<"  GIMP_RUN_INTERACTIVE="<<GIMP_RUN_INTERACTIVE<<std::endl;

  if (run_mode == GIMP_RUN_INTERACTIVE) {
    /* Show the preview in interactive mode, unless if we are
     * in thumbnail mode or 'send to gimp' mode. */
    std::cout<<"  before creating window"<<std::endl;

    gchar  *filename_out = gimp_temp_name ("tif");
    gchar  *pfiname_out  = gimp_temp_name ("pfi");

     std::cout<<"  before creating window"<<std::endl;

    gchar *photoflow_stdout = NULL;
    GError      **error;

    gchar *argv[] =
    {
        (gchar*)(phf_binary.c_str()),
        "--plugin",
        (gchar *) filename,
        (gchar *) pfiname.c_str(),
        (gchar *) filename_out,
        (gchar *) pfiname_out,
        NULL
    };

    gimp_progress_init_printf (_("Opening '%s'"),
        gimp_filename_to_utf8 (filename));

    printf ("Starting photoflow... (source_layer_id=%d)\n", source_layer_id);

    char cmd[1000];
    if( source_layer_id >= 0 ) {
      sprintf(cmd,"%s --plugin \"%s\" \"%s\" \"%s\" \"%s\"", phf_binary.c_str(),
          filename, pfiname.c_str(), filename_out, pfiname_out);
    } else {
      sprintf(cmd,"%s --plugin \"%s\" \"%s\" \"%s\"", phf_binary.c_str(),
          pfiname.c_str(), filename_out, pfiname_out);
    }
    printf ("  command: %s\n",cmd);
    //system("which photoflow");
    system(cmd);
    //getchar();
    /*
    if (g_spawn_sync (NULL,
                      argv,
                      NULL,
  //                     G_SPAWN_STDOUT_TO_DEV_NULL |
      (GSpawnFlags)(G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH),
                      NULL,
                      NULL,
                      &photoflow_stdout,
                      NULL,
                      NULL,
                      error)) {
     */
    {
      TIFF* tiff  = TIFFOpen( filename_out, "r" );

      if( tiff ) {

        guint width, height;
        if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width)) {
          g_warning("could not get TIFF image width");
        } else if (!TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height)) {
          g_warning("could not get TIFF image height");
        }



        // Transfer the output layers back into GIMP.
        GimpLayerModeEffects layer_blendmode = GIMP_NORMAL_MODE;
        gint layer_posx = 0, layer_posy = 0;
        double layer_opacity = 100;

        gint32 dest_layer_id = active_layer_id;
        if( !replace_layer ) {
          /* Create the "background" layer to hold the image... */
          gint32 layer = gimp_layer_new(image_id, _("PhF output"), width,
              height, GIMP_RGB_IMAGE, 100.0,
              GIMP_NORMAL_MODE);
          std::cout<<"PhF plug-in: new layer created"<<std::endl;
#if defined(GIMP_CHECK_VERSION) && GIMP_CHECK_VERSION(2,7,3)
          gimp_image_insert_layer(image_id, layer, 0, -1);
#else
          gimp_image_add_layer(image_id, layer, -1);
#endif
          std::cout<<"PhF plug-in: new layer added"<<std::endl;
          dest_layer_id = layer;
        }
        /* Get the drawable and set the pixel region for our load... */
#if HAVE_GIMP_2_9
        buffer = gimp_drawable_get_buffer(dest_layer_id);
#else
        drawable = gimp_drawable_get(dest_layer_id);
        gimp_pixel_rgn_init(&pixel_region, drawable, 0, 0, drawable->width,
            drawable->height, TRUE, FALSE);
        tile_height = gimp_tile_height();
#endif

        std::cout<<"PhF plug-in: copying buffer..."<<std::endl;
#if HAVE_GIMP_2_9
#ifdef BABL_FLIPS_DISABLED
        format = "RGB float";
#else
        format = is_lin_gamma ? "RGB float" : "R'G'B' float";
#endif
        load_tiff( tiff, buffer );
        gimp_drawable_update(dest_layer_id,0,0,width,height);
        if( in_width != width || in_height != height )
          gimp_layer_resize(dest_layer_id,width,height,0,0);
#else
        for (row = 0; row < Crop.height; row += tile_height) {
          nrows = MIN(Crop.height - row, tile_height);
          gimp_pixel_rgn_set_rect(&pixel_region,
              uf->thumb.buffer + 3 * row * Crop.width, 0, row, Crop.width, nrows);
        }
#endif
        // Load PFI file into memory
        std::ifstream t;
        std::stringstream strstr;
        t.open( pfiname_out );
        strstr << t.rdbuf();
        char* pfi_buffer = strdup( strstr.str().c_str() );
        /*
        int length;
        t.seekg(0,std::ios::end);
        length = t.tellg();
        t.seekg(0,std::ios::beg);
        char* buffer = new char[length+1];
        t.read( buffer, length );
        buffer[length] = 0;
         */
        t.close();

        GimpParasite *cfg_parasite;
        cfg_parasite = gimp_parasite_new("phf-config",
            GIMP_PARASITE_PERSISTENT, strlen(pfi_buffer), pfi_buffer);
        gimp_item_attach_parasite(dest_layer_id, cfg_parasite);
        gimp_parasite_free(cfg_parasite);

#if HAVE_GIMP_2_9
        gegl_buffer_flush(buffer);
        g_object_unref(buffer);
#else
        gimp_drawable_flush(drawable);
        gimp_drawable_detach(drawable);
#endif
        status = GIMP_PDB_SUCCESS;
      }
    }
    g_unlink (filename_out);
    g_unlink (pfiname_out);
  } else {
    std::cout<<"plug-in: execution skipped"<<std::endl;
  }

  gimp_displays_flush();

  g_unlink (filename);
  g_unlink (pfiname.c_str());


  std::cout<<"Plug-in: setting return values"<<std::endl;
  return_values[0].data.d_status = status;
  std::cout<<"Plug-in: return values done"<<std::endl;
  return;
}
