/************************************************************************/
/* Copyright (c) 2002-2006 Paul Duncan <paul@pablotron.org>             */
/*                                                                      */
/* Permission is hereby granted, free of charge, to any person          */
/* obtaining a copy of this software and associated documentation files */
/* (the "Software"), to deal in the Software without restriction,       */
/* including without limitation the rights to use, copy, modify, merge, */
/* publish, distribute, sublicense, and/or sell copies of the Software, */
/* and to permit persons to whom the Software is furnished to do so,    */
/* subject to the following conditions:                                 */
/*                                                                      */
/* The above copyright notice and this permission notice shall be       */
/* included in all copies of the Software, its documentation and        */
/* marketing & publicity materials, and acknowledgment shall be given   */
/* in the documentation, materials and software packages that this      */
/* Software was used.                                                   */
/*                                                                      */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,      */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF   */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                */
/* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY     */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE    */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.               */
/************************************************************************/

#include <ruby.h>
#include <musicbrainz/mb_c.h>
#include <musicbrainz/queries.h>
#include <musicbrainz/browser.h>

#define MB_VERSION "0.3.0"
#define UNUSED(a) ((void) (a))

/**********************************************************************/
/* Buffer Type                                                        */
/*                                                                    */
/* NOTE: By default, this is set to static char to save a bit of      */
/* memory and speed things up.  Unfortunately, this also makes the    */
/* functions non-reentrant.  Fortunately, Ruby isn't reentrant        */
/* either, so this shouldn't affect anyone.  If that changes at some  */
/* point, or if this gives you problems, change the line below from   */
/* "static char" to "char", recompile, and send me a nasty-gram at    */
/* the email address above :).                                        */
/**********************************************************************/
#define MB_BUFFER static char

/**********************************************************************/
/* Various Buffer Sizes.                                              */
/*                                                                    */
/* These are more conservative than previous releases.  This change   */
/* shouldn't affect anyone (except by saving a few kilobytes of       */
/* memory here and there), but if they do, please let me know.        */
/**********************************************************************/
#define MB_HOST_BUFSIZ      1024
#define MB_ERR_BUFSIZ       1024
#define MB_RESULT_BUFSIZ    1024
#define MB_VERSION_BUFSIZ   32
#define MB_ID_BUFSIZ        128
#define MB_FRAG_BUFSIZ      256

#define MB_QUERY(a,b,c)                  \
  do {                                   \
    VALUE v = rb_str_new2(c);            \
    rb_define_const(mQuery, (b), v);     \
    rb_define_const(mQuery, a "_" b, v); \
  } while (0)

static VALUE mMB,     /* MusicBrainz          */
             eErr,    /* MusicBrainz::Error   */
             cClient, /* MusicBrainz::Client  */
             cTRM,    /* MusicBrainz::TRM     */
             mQuery;  /* MusicBrainz::Query   */

/* 
 * Document-module: MusicBrainz
 *
 * See MusicBrainz::Client and MusicBrainz::TRM for API documentation.
 */

/* 
 * parse host specification and (optionally) extract the port name.
 */
static void parse_hostspec(const int argc, 
                           const VALUE *argv, 
                           char *ret_host, 
                           size_t ret_host_len, 
                           int *ret_port) {
  VALUE host, port;
  char *ptr;

  /* grab host and port from argument list */
  host = port = Qnil;
  rb_scan_args(argc, argv, "11", &host, &port);

  /* clear buffer, copy hostspec, and make sure buffer is null-terminated */
  memset(ret_host, 0, ret_host_len);
  strncpy(ret_host, StringValueCStr(host), ret_host_len);
  ret_host[ret_host_len - 1] = '\0';

  /* if we only got one argument, then scan it for a port suffix */
  if (argc == 1) {
    if ((ptr = strchr(ret_host, ':')) != NULL) {
      *ptr = '\0';
      *ret_port = atoi(ptr + 1);
    }
  } else {
    *ret_port = NUM2INT(port);
  }

  /* make sure specified port is in range */
  if (*ret_port < 0 || *ret_port > 0xffff)
    rb_raise(eErr, "invalid port: %d", *ret_port);
}


/*
 * Document-class: MusicBrainz::Client
 *
 * Client query interface to the MusicBrainz music library.  The
 * easiest way to explain the code is probably with a simple example.
 *
 *   # load the musicbrainz library
 *   require 'musicbrainz'
 *
 *   # create a musicbrainz client handle
 *   mb = MusicBrainz::Client.new
 *
 *   # create a musicbrainz client handle
 *   mb = MusicBrainz::Client.new
 *
 *   # search for albums named "mirror conspiracy"
 *   album_name = 'Mirror Conspiracy'
 *   query_ok = mb.query(MusicBrainz::Query::FindAlbumByName, album_name)
 *
 *   # if there weren't any errors, then print the number of matching albums
 *   if query_ok
 *     num_albums = mb.result(MusicBrainz::Query::GetNumAlbums).to_i
 *     puts "Number of Results: #{num_albums}"
 *   end
 *
 */

/*******************************/
/* MusicBrainz::Client methods */
/*******************************/
static void client_free(void *mb) {
  mb_Delete(* (musicbrainz_t*) mb);
  free(mb);
}

static VALUE mb_client_alloc(VALUE klass) {
  musicbrainz_t *mb;

  if ((mb = malloc(sizeof(musicbrainz_t))) == NULL)
    rb_raise(eErr, "couldn't allocate memory for Client structure");

  return Data_Wrap_Struct(klass, 0, client_free, mb);
}

#ifndef HAVE_RB_DEFINE_ALLOC_FUNC
/*
 * Allocate and initialize a new MusicBrainz::Client object.
 *
 * Example:
 *   mb = MusicBrainz::Client.new
 */
VALUE mb_client_new(VALUE klass) {
  VALUE self;
  musicbrainz_t *mb;

  self = mb_client_alloc(klass);
  rb_obj_call_init(self, 0, NULL);

  return self;
}
#endif /* HAVE_RB_DEFINE_ALLOC_FUNC */

/*
 * :nodoc:
 *
 * Constructor for MusicBrainz::Client object.
 */
static VALUE mb_client_init(VALUE self) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  *mb = mb_New();
  return self;
}

/*
 * Get the version of a MusicBrainz::Client object.
 *
 * Note: this actually returns the version of the MusicBrainz library.
 *
 * Example:
 *   puts 'MusicBrainz version: ' << mb.version
 *   
 */
static VALUE mb_client_version(VALUE self) {
  musicbrainz_t *mb;
  MB_BUFFER buf[MB_VERSION_BUFSIZ];
  int ver[3];

  Data_Get_Struct(self, musicbrainz_t, mb);

  mb_GetVersion(*mb, &(ver[0]), &(ver[1]), &(ver[2]));
  snprintf(buf, sizeof(buf), "%d.%d.%d", ver[0], ver[1], ver[2]);

  return rb_str_new2(buf);
}

/*
 * Set the server name and port for the MusicBrainz::Client object.
 *
 * Returns false if MusicBrainz could not connect to the server. If this
 * method is not called, the default server is 'www.musicbrainz.org',
 * port 80.
 *
 * Aliases:
 *   MusicBrainz::Client#set_server
 *
 * Examples:
 *   # connect to www.musicbrainz.org, port 80
 *   mb.server = 'www.musicbrainz.org'
 *
 *   # connect to www.example.com, port 31337
 *   mb.server = 'www.example.com:31337'
 *
 *   # connect to www.musicbrainz.org, port 80
 *   mb.set_server 'www.musicbrainz.org'
 *
 *   # connect to www.example.com, port 31337
 *   mb.set_server 'www.example.com', 31337
 *
 */
static VALUE mb_client_set_server(int argc, VALUE *argv, VALUE self) {
  musicbrainz_t *mb;
  MB_BUFFER host[MB_HOST_BUFSIZ];
  int port;

  /* grab mb handle */
  Data_Get_Struct(self, musicbrainz_t, mb);
  
  /* clear host buffer and set default port */
  memset(host, 0, sizeof(host));
  port = 80;

  parse_hostspec(argc, argv, host, sizeof(host), &port);
  
  return mb_SetServer(*mb, host, port) ? Qtrue : Qfalse;
}

/*
 * Enable debugging output for this MusicBrainz::Client object.
 * 
 * Note: Debugging output is sent to standard output, not standard
 * error.
 * 
 * Example:
 *   mb.debug = true
 *
 */
static VALUE mb_client_set_debug(VALUE self, VALUE debug) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_SetDebug(*mb, (debug == Qtrue));
  return debug;
}

/*
 * Set the proxy name and port for the MusicBrainz::Client object.
 *
 * Returns false if MusicBrainz could not connect to the proxy.
 *
 * Aliases:
 *   MusicBrainz::Client#set_proxy
 *
 * Examples:
 *   # connect to 'proxy.localdomain', port 8080
 *   mb.proxy = 'proxy.localdomain'
 *
 *   # connect to proxy.example.com, port 31337
 *   mb.proxy = 'proxy.example.com:31337'
 *
 *   # connect to www.musicbrainz.org, port 8080
 *   mb.set_proxy 'www.musicbrainz.org'
 *
 *   # connect to proxy.example.com, port 31337
 *   mb.set_proxy 'proxy.example.com', 31337
 *
 */
static VALUE mb_client_set_proxy(int argc, VALUE *argv, VALUE self) {
  musicbrainz_t *mb;
  MB_BUFFER host[MB_HOST_BUFSIZ];
  int port;

  /* get musicbrainz handle */
  Data_Get_Struct(self, musicbrainz_t, mb);
  
  /* clear host buffer and set default port */
  memset(host, 0, sizeof(host));
  port = 8080;

  parse_hostspec(argc, argv, host, sizeof(host), &port);
  
  return mb_SetProxy(*mb, host, port) ? Qtrue : Qfalse;
}

/*
 * Set user authentication for a MusicBrainz::Client object.
 *
 * This method is optional.  It only needs to be called if you want to
 * submit data to the server and give the user credit for the
 * submission.  If you want to submit data anonymously, don't call this
 * method.  Returns true if the authentication was successful.
 *
 * Aliases:
 *   MusicBrainz::Client#authenticate
 *
 * Examples:
 *   # connect as user "MrMusic", password "s3kr3tp455w0rd"
 *   mb.auth 'MrMusic', 's3kr3tp455w0rd'
 *
 *   # connect as user "MBrox", password "hithere!"
 *   mb.authenticate 'MBrox', 'hithere!'
 *
 */
static VALUE mb_client_auth(VALUE self, VALUE user, VALUE pass) {
  musicbrainz_t *mb;
  char *u, *p;

  Data_Get_Struct(self, musicbrainz_t, mb);
  u = StringValueCStr(user); 
  p = StringValueCStr(pass);

  return mb_Authenticate(*mb, u, p) ? Qtrue : Qfalse;
}

/*
 * Set CD-ROM device for a MusicBrainz::Client object.
 *
 * On Unix systems, this is a path (eg "/dev/scd0") and defaults to
 * "/dev/cdrom". On Win32 systems, it's a drive letter (eg "E:").  This
 * method always returns true.
 *
 * Aliases:
 *   MusicBrainz::Client#set_device
 *
 * Examples:
 *   # set device to "/dev/scd1"
 *   mb.device = '/dev/scd1'
 *
 *   # set CD-ROM to "E:" on a Win32 system
 *   mb.set_device 'E:'
 *
 */
static VALUE mb_client_set_device(VALUE self, VALUE device) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  return mb_SetDevice(*mb, StringValueCStr(device)) ? Qtrue : Qfalse;
}

/*
 * Enable UTF-8 output for a MusicBrainz::Client object.
 * 
 * Note: Defaults to ISO-8859-1 output.  If this is set to true, then
 * UTF-8 will be used instead.
 * 
 * Aliases:
 *   MusicBrainz::Client#use_utf8=
 *   MusicBrainz::Client#set_use_utf8
 *
 * Examples:
 *   mb.utf8 = true
 *   mb.use_utf8 = true
 *   mb.set_use_utf8 true
 *
 */
static VALUE mb_client_set_use_utf8(VALUE self, VALUE use_utf8) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_UseUTF8(*mb, (use_utf8 == Qtrue));
  return use_utf8;
}

/*
 * Set the search depth for a MusicBrainz::Client object.
 * 
 * Note: Defaults to 2.
 * 
 * Aliases:
 *   MusicBrainz::Client#set_depth
 *
 * Examples:
 *   mb.depth = 5
 *   mb.set_depth 5
 *
 */
static VALUE mb_client_set_depth(VALUE self, VALUE depth) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_SetDepth(*mb, NUM2INT(depth));
  return self;
}

/*
 * Set the maximum number of items for a MusicBrainz::Client object.
 * 
 * Set the maximum number of items to return from a query for a
 * MusicBrainz::Client object.  If the query yields more items than this
 * number, the server will omit the excess results.  Defaults to 25.
 * 
 * Aliases:
 *   MusicBrainz::Client#set_max_items
 *
 * Examples:
 *   mb.max_items = 5
 *   mb.set_max_items 5
 *
 */
static VALUE mb_client_set_max_items(VALUE self, VALUE max_items) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_SetMaxItems(*mb, NUM2INT(max_items));
  return self;
}

/*
 * Query the MusicBrainz server with this MusicBrainz::Client object.
 *
 * Returns true if the query was successful (even if it didn't return
 * any results).
 *
 * See the MusicBrainz::Query documentation for information on various
 * query types.
 *
 * Examples:
 *   # get general return status of prior query
 *   mb.query MusicBrainz::Query::GetStatus
 *
 *   # query the MusicBrainz server for an album titled "Airdrawndagger"
 *   # by an artist "Sasha"
 *   mb.query MusicBrainz::Query::AlbumFindAlbum,
 *            'Sasha',
 *            'Airdrawndagger'
 *
 */
static VALUE mb_client_query(int argc, VALUE *argv, VALUE self) {
  musicbrainz_t *mb;
  VALUE ret = Qfalse;
  char *obj, **args;
  int i;

  Data_Get_Struct(self, musicbrainz_t, mb);
  switch (argc) {
    case 0:
      rb_raise(eErr, "Invalid argument count: %d.", argc);
      break;
    case 1:
      ret = mb_Query(*mb, StringValueCStr(argv[0])) ? Qtrue : Qfalse;
      break;
    default:
      /* grab object */
      obj = RSTRING(argv[0])->ptr;

      /* allocate argument list */
      if ((args = malloc(sizeof(char*) * argc)) == NULL)
        rb_raise(eErr, "couldn't allocate query argument list");

      /* add each argument list, then terminate the list  */
      for (i = 1; i < argc; i++)
        args[i - 1] = RSTRING(argv[i])->ptr;
      args[argc - 1] = NULL;

      /* execute query and free argument list */
      ret = mb_QueryWithArgs(*mb, obj, args) ? Qtrue : Qfalse;
      free(args);
  }

  return ret;
}

/*
 * Get web-based MusicBrainz CD-ROM submission URL for CD-ROM device associated with this MusicBrainz::Client object.
 *
 * Use MusicBrainz::Client#device to set the CD-ROM device. Returns nil
 * on error.
 *
 * Aliases:
 *   MusicBrainz::Client#get_url
 *   MusicBrainz::Client#get_web_submit_url
 *
 * Examples:
 *   url = mb.url
 *   url = mb.get_url
 *   url = mb.get_web_submit_url
 *
 */
static VALUE mb_client_url(VALUE self) {
  musicbrainz_t *mb;
  MB_BUFFER buf[MB_HOST_BUFSIZ];
  VALUE ret = Qnil;

  Data_Get_Struct(self, musicbrainz_t, mb);
  if (mb_GetWebSubmitURL(*mb, buf, sizeof(buf)))
    ret = rb_str_new2(buf);

  return ret;
}

/*
 * Retrieve error from last call to MusicBrainz::Client#query with this MusicBrainz::Client object.
 *
 * Aliases:
 *   MusicBrainz::Client#get_error
 *   MusicBrainz::Client#get_query_error
 *
 * Example:
 *   # simple query and error return
 *   query = MusicBrainz::Query::GetStatus
 *   puts 'Error: ' << mb.error unless mb.query query
 *
 */
static VALUE mb_client_error(VALUE self) {
  musicbrainz_t *mb;
  MB_BUFFER buf[MB_ERR_BUFSIZ];

  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_GetQueryError(*mb, buf, sizeof(buf));

  return rb_str_new2(buf);
}
  
/*
 * Select a context in the query result of this MusicBrainz::Client object.
 *
 * Returns true if the select query was successful (even if it didn't
 * return any results).
 *
 * See the MusicBrainz::Query documentation for information on various
 * query types.
 *
 * Examples:
 *   # return to the top-level context of the current query
 *   mb.select MusicBrainz::Query::Rewind
 *
 *   # select the second artist from a query that returned a list of
 *   # artists
 *   mb.select MusicBrainz::Query::SelectArtist, 2
 *
 */
static VALUE mb_client_select(int argc, VALUE *argv, VALUE self) {
  musicbrainz_t *mb;
  VALUE ret = Qfalse;
  char *obj;
  int i, *args;

  Data_Get_Struct(self, musicbrainz_t, mb);
  switch (argc) {
    case 0:
      rb_raise(eErr, "Invalid argument count: %d.", argc);
      break;
    case 1:
      ret = mb_Select(*mb, StringValueCStr(argv[0])) ? Qtrue : Qfalse;
      break;
    case 2:
      obj = StringValueCStr(argv[0]);
      i = FIX2INT(argv[1]);
      ret = mb_Select1(*mb, obj, i) ? Qtrue : Qfalse;
      break;
    default:
      /* grab object */
      obj = StringValueCStr(argv[0]);

      /* allocate argument list */
      if ((args = malloc(sizeof(int) * argc)) == NULL)
        rb_raise(eErr, "couldn't allocate memory for argument list");

      /* add arguments to list and NULL-terminate list */
      for (i = 1; i < argc; i++)
        args[i - 1] = FIX2INT(argv[i]);
      args[argc - 1] = 0;

      /* run query and free argument list */
      ret = mb_SelectWithArgs(*mb, obj, args) ? Qtrue : Qfalse;
      free(args);
  }

  return ret;
}

/* 
 * Extract a piece of information from the data returned by a successful query by a MusicBrainz::Client object.
 *
 * Returns nil if there was an error or if the correct piece of data was
 * not found.
 *
 * Note: Certain result queries require an ordinal argument.  See the
 * MusicBrainz result query (MBE_*) documentation for additional
 * information.
 *
 * Aliases:
 *   MusicBrainz::Client#get_result 
 *   MusicBrainz::Client#get_result_data
 *
 * Examples:
 *   # get the name of the currently selected album
 *   album_name = mb.result MusicBrainz::Query::AlbumGetAlbumName
 *
 *   # get the duration of the 5th track on the current album
 *   duration = mb.result MusicBrainz::Query::AlbumGetTrackDuration, 5
 */
static VALUE mb_client_result(int argc, VALUE *argv, VALUE self) {
  musicbrainz_t *mb;
  VALUE ret = Qnil;
  MB_BUFFER buf[MB_RESULT_BUFSIZ];
  char *obj;

  Data_Get_Struct(self, musicbrainz_t, mb);
  obj = argc ? StringValueCStr(argv[0]) : NULL;
  switch (argc) {
    case 1:
      if (mb_GetResultData(*mb, obj, buf, sizeof(buf)))
        if (strlen(buf))
          ret = rb_str_new2(buf);
      break;
    case 2:
      if (mb_GetResultData1(*mb, obj, buf, sizeof(buf), FIX2INT(argv[1])))
        if (strlen(buf))
          ret = rb_str_new2(buf);
      break;
    default:
      rb_raise(eErr, "Invalid argument count: %d.", argc);
  }

  return ret;
}

/* 
 * Return the integer value of a query by a MusicBrainz::Client object.
 *
 * Note: Certain result queries require an ordinal argument.  See the
 * MusicBrainz result query (MBE_*) documentation for additional
 * information.
 *
 * Aliases:
 *   MusicBrainz::Client#result_int
 *   MusicBrainz::Client#get_result_int
 *
 * Examples:
 *   # get the name of the currently selected album
 *   album_name = mb.result MusicBrainz::Query::AlbumGetAlbumName
 *
 *   # get the duration of the 5th track on the current album
 *   duration = mb.result MusicBrainz::Query::AlbumGetTrackDuration, 5
 */
static VALUE mb_client_result_int(int argc, VALUE *argv, VALUE self) {
  musicbrainz_t *mb;
  int ret;
  char *obj;

  Data_Get_Struct(self, musicbrainz_t, mb);
  obj = argc ? StringValueCStr(argv[0]) : NULL;

  switch (argc) {
    case 1:
      ret = mb_GetResultInt(*mb, obj);
      break;
    case 2:
      ret = mb_GetResultInt1(*mb, obj, FIX2INT(argv[1]));
      break;
    default:
      rb_raise(eErr, "Invalid argument count: %d.", argc);
  }

  return INT2FIX(ret);
}

/* 
 * See if a piece of information exists in data returned by a successful query by a MusicBrainz::Client object.
 *
 * Note: Certain result queries require an ordinal argument.  See the
 * MusicBrainz result query (MBE_*) documentation for additional
 * information.
 *
 * Aliases:
 *   MusicBrainz::Client#result_exists?
 *   MusicBrainz::Client#does_result_exist?
 *
 * Examples:
 *   # does the current album have a name?
 *   puts 'named album' if mb.exists? MusicBrainz::Query::AlbumGetAlbumName
 *
 *   # does the current album have a type?
 *   puts 'has a type' if mb.exists? MusicBrainz::Query::AlbumGetAlbumType
 */
static VALUE mb_client_exists(int argc, VALUE *argv, VALUE self) {
  musicbrainz_t *mb;
  VALUE ret = Qfalse;
  char *obj;

  Data_Get_Struct(self, musicbrainz_t, mb);
  obj = argc ? StringValueCStr(argv[0]) : NULL;
  switch (argc) {
    case 1:
      ret = mb_DoesResultExist(*mb, obj) ? Qtrue : Qfalse;
      break;
    case 2:
      ret = mb_DoesResultExist1(*mb, obj, FIX2INT(argv[1])) ? Qtrue : Qfalse;
      break;
    default:
      rb_raise(eErr, "Invalid argument count: %d.", argc);
  }

  return ret;
}

/*
 * Get the RDF that was returned by the server of a MusicBrainz::Client object.
 *
 * Returns nil if a string could not be allocated or if there was an
 * error.
 * 
 * Aliases:
 *   MusicBrainz::Client#get_rdf
 *   MusicBrainz::Client#result_rdf
 *   MusicBrainz::Client#get_result_rdf
 *   
 * Example:
 *   rdf = mb.rdf
 *
 */
static VALUE mb_client_rdf(VALUE self) {
  musicbrainz_t *mb;
  VALUE ret = Qnil;
  char *buf;
  int len;

  Data_Get_Struct(self, musicbrainz_t, mb);
  if ((len = mb_GetResultRDFLen(*mb)) > 0) {
    if ((buf = malloc(len + 1)) != NULL) {
      mb_GetResultRDF(*mb, buf, len + 1);
      ret = rb_str_new(buf, len);
      free(buf);
    } else {
      rb_raise(eErr, "couldn't allocate memory for RDF buffer");
    }
  }

  return ret;
}

/*
 * Get the length of the RDF that was returned by the server of a MusicBrainz::Client object.
 *
 * Aliases:
 *   MusicBrainz::Client#get_rdf_len
 *   MusicBrainz::Client#result_rdf_len
 *   MusicBrainz::Client#get_result_rdf_len
 *   
 * Example:
 *   rdf_len = mb.rdf_len
 *
 */
static VALUE mb_client_rdf_len(VALUE self) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  return INT2FIX(mb_GetResultRDFLen(*mb));
}

/*
 * Set the RDF to use for data extraction for a MusicBrainz::Client object.
 *
 * Note: Advanced users only.
 *
 * Aliases:
 *   MusicBrainz::Client#set_rdf
 *   MusicBrainz::Client#result_rdf=
 *   MusicBrainz::Client#set_result_rdf
 *
 * Example:
 *   mb.rdf = result_rdf
 *
 */
static VALUE mb_client_set_rdf(VALUE self, VALUE rdf) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  return mb_SetResultRDF(*mb, StringValueCStr(rdf)) ? Qtrue : Qfalse;
}

/*
 * Extract the actual artist/album/track ID from a MBE_GetxxxxxId query.
 *
 * The MBE_GETxxxxxId queries return a URL where additional RDF metadata
 * for a given ID can be retrieved.  Callers may wish to extract only
 * the ID of an artist/album/track for reference.
 *
 * Aliases:
 *   MusicBrainz::Client#get_id_from_url
 *
 * Examples:
 *   # get the artist name of the first track on the album
 *   url = mb.result MusicBrainz::Query::AlbumGetArtistId, 1
 *   id = mb.id_from_url url
 *   
 */
static VALUE mb_client_id_from_url(VALUE self, VALUE url) {
  musicbrainz_t *mb;
  MB_BUFFER buf[MB_ID_BUFSIZ];

  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_GetIDFromURL(*mb, StringValueCStr(url), buf, sizeof(buf));

  return rb_str_new2(buf);
}

/*
 * Extract an identifier fragment from a URL.
 *
 * Given a URI, this method will return the string that follows the #
 * separator (e.g. when passed
 * "http://musicbrainz.org/mm/mq-1.1#ArtistResult", this method will
 * return "ArtistResult").
 * 
 *
 * Aliases:
 *   MusicBrainz::Client#get_fragment_from_url
 *
 * Examples:
 *   # get the artist name of the first track on the album
 *   url = mb.result MusicBrainz::Query::AlbumGetArtistId, 1
 *   frag = mb.fragment_from_url url
 *   
 */
static VALUE mb_client_frag_from_url(VALUE self, VALUE url) {
  musicbrainz_t *mb;
  MB_BUFFER buf[MB_FRAG_BUFSIZ];

  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_GetFragmentFromURL(*mb, StringValueCStr(url), buf, sizeof(buf));

  return rb_str_new2(buf);
}

/*
 * Get the ordinal (list position) of an item in a list.
 *
 * Normally used to retrieve the track number out of a list of tracks in
 * an album.
 *
 * Aliases:
 *   MusicBrainz::Client#get_ordinal
 *   MusicBrainz::Client#get_ordinal_from_list
 *
 * Examples:
 *   # get the ordinal of a track based on the URI
 *   list = mb.result MusicBrainz::Query::AlbumGetTrackList
 *   uri  = mb.result MusicBrainz::Query::AlbumGetTrackId, 5
 *   ordinal = mb.ordinal list, uri
 *   
 */
static VALUE mb_client_ordinal(VALUE self, VALUE list, VALUE uri) {
  musicbrainz_t *mb;
  Data_Get_Struct(self, musicbrainz_t, mb);
  return INT2FIX(mb_GetOrdinalFromList(*mb, StringValueCStr(list), StringValueCStr(uri)));
}

#if 0
/* 
 * Calculate the SHA1 hash for a given filename.
 *
 * Aliases:
 *   MusicBrainz::Client#calculate_sha1
 *
 * Examples:
 *   sha1 = mb.sha1 'foo.mp3'
 *
 */
static VALUE mb_client_sha1(VALUE self, VALUE path) {
  musicbrainz_t *mb;
  MB_BUFFER buf[41];

  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_CalculateSha1(*mb, StringValueCStr(path), buf);

  return rb_str_new2(buf);
}
#endif /* 0 */

/*
 * Calculate the crucial pieces of information for an MP3 file.
 *
 * Note: This method returns the duration of the MP3 in milliseconds, so
 * you'll need to divide the duration by 1000 before passing it to
 * MusicBrainz::TRM methods.
 *
 * Aliases:
 *   MusicBrainz::Client#get_mp3_info
 *
 * Examples:
 *   # get the duration and stereo of an MP3 named 'foo.mp3'
 *   info = mb.mp3_info 'foo.mp3'
 *   puts 'duration (ms): ' << info['duration'] << ', ' <<
 *        'stereo: ' << (info['stereo'] ? 'yes' : 'no')
 *   
 */
static VALUE mb_client_mp3_info(VALUE self, VALUE path) {
  musicbrainz_t *mb;
  VALUE ret = Qnil;
  int dr, br, st, sr;

  Data_Get_Struct(self, musicbrainz_t, mb);
  if (mb_GetMP3Info(*mb, StringValueCStr(path), &dr, &br, &st, &sr)) {
    ret = rb_hash_new();
    rb_hash_aset(ret, rb_str_new2("duration"), INT2FIX(dr));
    rb_hash_aset(ret, rb_str_new2("bitrate"), INT2FIX(br));
    rb_hash_aset(ret, rb_str_new2("stereo"), st ? Qtrue : Qfalse);
    rb_hash_aset(ret, rb_str_new2("samplerate"), INT2FIX(sr));
  }

  return ret;
}

/*
 * Launch a URL in the specified browser.
 *
 * Aliases:
 *   MusicBrainz::Client#browser
 *   MusicBrainz::Client#launch_browser
 *
 * Examples:
 *   mb.launch uri, 'galeon'
 *
 */
static VALUE mb_client_launch(VALUE self, VALUE url, VALUE browser) {
  char *url_str, *browser_str;
  url_str = url ? StringValueCStr(url) : NULL;
  browser_str = browser ? StringValueCStr(browser) : NULL;
  return LaunchBrowser(url_str, browser_str) ? Qtrue : Qfalse;
}


/*
 * Document-class: MusicBrainz::TRM
 *
 * Client API to generate MusicBrainz TRM signatures.  The
 * easiest way to explain the code is probably with a simple example.
 *
 *   # load the musicbrainz library
 *   require 'musicbrainz'
 *
 *   # create a musicbrainz trm handle
 *   trm = MusicBrainz::TRM.new
 *
 *   # prepare for CD-quality audio
 *   samples, channels, bits = 44100, 2, 16
 *   trm.pcm_data samples, channels, bits
 *   
 *   # read data from file and pass it to the TRM handle
 *   # until MusicBrainz has enough information to generate
 *   # a signature
 *   while buf = fh.read(4096)
 *     break if trm.generate_signature(buf)
 *   end
 *   
 *   # check for signature
 *   if sig = trm.finalize_signature
 *     # print human-readable version of signature
 *     puts trm.convert_sig(sig)
 *   else
 *     $stderr.puts "Couldn't generate signature"
 *   end
 *
 */

/****************************/
/* MusicBrainz::TRM methods */
/****************************/
static void trm_free(void *trm) {
  trm_Delete(* (trm_t*) trm);
  free(trm);
}

static VALUE mb_trm_alloc(VALUE klass) {
  trm_t *trm;

  if ((trm = malloc(sizeof(trm_t))) == NULL)
    rb_raise(eErr, "Couldn't allocate memory for TRM structure");

  return Data_Wrap_Struct(klass, 0, trm_free, trm);
}

#ifndef HAVE_RB_DEFINE_ALLOC_FUNC
/*
 * Allocate and initialize a new MusicBrainz::TRM object.
 *
 * Example:
 *   trm = MusicBrainz::TRM.new
 */
VALUE mb_trm_new(VALUE klass) {
  VALUE self;

  self = mb_trm_alloc(klass);
  rb_obj_call_init(self, 0, NULL);

  return self;
}
#endif /* !HAVE_RB_DEFINE_ALLOC_FUNC */

/*
 * :nodoc:
 *
 * Constructor for MusicBrainz::TRM object.
 */
static VALUE mb_trm_init(VALUE self) {
  trm_t *trm;

  Data_Get_Struct(self, trm_t, trm);
  *trm = trm_New();

  return self;
}

/*
 * Set the proxy name and port for the MusicBrainz::TRM object.
 * 
 * Note: If unspecified, the port defaults to 8080.
 *
 * Returns false if MusicBrainz could not connect to the proxy.
 *
 * Aliases:
 *   MusicBrainz::TRM#set_proxy
 *
 * Examples:
 *   # connect to 'proxy.localdomain', port 8080
 *   trm.proxy = 'proxy.localdomain'
 *
 *   # connect to proxy.example.com, port 31337
 *   trm.proxy = 'proxy.example.com:31337'
 *
 *   # connect to www.musicbrainz.org, port 8080
 *   trm.set_proxy 'www.musicbrainz.org'
 *
 *   # connect to proxy.example.com, port 31337
 *   trm.set_proxy 'proxy.example.com', 31337
 *
 */
static VALUE mb_trm_set_proxy(int argc, VALUE *argv, VALUE self) {
  trm_t *trm;
  MB_BUFFER host[MB_HOST_BUFSIZ];
  int port;

  Data_Get_Struct(self, trm_t, trm);
  
  memset(host, 0, sizeof(host));
  port = 8080;
  
  parse_hostspec(argc, argv, host, sizeof(host), &port);
  
  return trm_SetProxy(*trm, host, port) ? Qtrue : Qfalse;
}

/*
 * Set the information of an audio stream to be signatured.
 *
 * Note: this MUST be called before attempting to generate a signature.
 *
 * samples: samples per second (Hz) of audio data (eg 44100)
 * channels: number of audio channels (eg 1 for mono, or two for stereo)
 * bits: bits per sample (eg 8 or 16)
 * 
 * Aliases:
 *   MusicBrainz::TRM#set_pcm_data
 *   MusicBrainz::TRM#pcm_data_info
 *   MusicBrainz::TRM#set_pcm_data_info
 *
 * Examples:
 *   # prepare for CD-quality audio
 *   samples, channels, bits = 44100, 2, 16
 *   trm.pcm_data samples, channels, bits
 *
 */
static VALUE mb_trm_set_pcm_data(VALUE self, VALUE samples, VALUE chans, VALUE bps) {
  trm_t *trm;
  Data_Get_Struct(self, trm_t, trm);
  trm_SetPCMDataInfo(*trm, FIX2INT(samples), FIX2INT(chans), FIX2INT(bps));
  return self;
}

/*
 * Set the length of an audio stream (in seconds).
 *
 * Note: This method is optional, but if it is called, it must be called
 * after MusicBrainz::TRM#pcm_data and before any calls to
 * MusicBrainz::TRM#generate_signature.
 *
 * Aliases:
 *   MusicBrainz::TRM#set_length
 *   MusicBrainz::TRM#song_length=
 *   MusicBrainz::TRM#set_song_length
 *
 * Examples:
 *   trm.length = 4000
 */
static VALUE mb_trm_set_length(VALUE self, VALUE len) {
  trm_t *trm;
  Data_Get_Struct(self, trm_t, trm);
  trm_SetSongLength(*trm, FIX2INT(len));
  return self;
}

/*
 * Pass raw PCM data to generate a signature.
 *
 * Note: MusicBrainz::TRM#pcm_data must be called before this function.
 *
 * Returns true if enough data has been sent to generate a signature,
 * and false if more data is needed.
 *
 * Example:
 *   trm.generate_signature buf
 *
 */
static VALUE mb_trm_gen_sig(VALUE self, VALUE buf) {
  trm_t *trm;
  char *ptr;
  int len;

  Data_Get_Struct(self, trm_t, trm);
  ptr = RSTRING(buf)->ptr;
  len = RSTRING(buf)->len;

  return trm_GenerateSignature(*trm, ptr, len) ? Qtrue : Qfalse;
}

/*
 * Finalize generated signature.
 *
 * Call this after MusicBrainz::TRM#generate_signature has returned true.
 *
 * Accepts an optional 16-byte string, used to associate the signature
 * with a particular collection in the RElatable Engine.  Returns nil on
 * error, or a 16 byte signature on success.
 *
 * Example:
 *   sig = trm.finalize_signature
 *
 */
static VALUE mb_trm_finalize_sig(int argc, VALUE *argv, VALUE self) {
  trm_t *trm;
  MB_BUFFER sig[32];
  char *id = NULL;
  VALUE ret = Qnil;

  Data_Get_Struct(self, trm_t, trm);
  switch (argc) {
    case 0:
      break;
    case 1:
      if (argv[0] != Qnil)
        id = RSTRING(argv[0])->ptr;
      break;
    default:
      rb_raise(eErr, "Invalid argument count: %d.", argc);
  }

  if (!trm_FinalizeSignature(*trm, sig, id))
    ret = rb_str_new(sig, 16);

  return ret;
}

/*
 * Convert 16-byte raw signature into a human-readable 36-byte ASCII string.
 *
 * Used after MusicBrainz::TRM#generate_signature has returned true and
 * MusicBrainz::TRM#finalize_signature has returned a signature.
 *
 * Aliases:
 *   MusicBrainz::TRM#sig_to_ascii
 *   MusicBrainz::TRM#convert_sig_to_ascii
 *
 * Examples:
 *   puts 'signature: ' << trm.convert_sig raw_sig
 *
 */
static VALUE mb_trm_convert_sig(VALUE self, VALUE sig) {
  trm_t *trm;
  MB_BUFFER buf[64];

  Data_Get_Struct(self, trm_t, trm);
  trm_ConvertSigToASCII(*trm, StringValuePtr(sig), buf);

  return rb_str_new(buf, MB_ID_LEN);
}


/******************/
/* INIT FUNCTIONS */
/******************/

/*
 * Document-module: MusicBrainz::Query
 * 
 * This module contains the constants used as parameters to the
 * MusicBrainz::Client#query and MusicBrainz::Client#result methods.
 *
 * Each constant is defined in two forms; with or without the 
 * type prefix.  For example, the MusicBrainz::Query::SelectArtist
 * constant is defined as both MusicBrainz::Query::SelectArtist
 * _and_ MusicBrainz::Query::MBS_SelectArtist.  This is done in
 * order to keep the Ruby API comparable to the C and C++ API.
 *
 * The following sections contain a full list of defined constants,
 * along with a brief description of each one.
 * 
 * == Select Queries
 * * <code>MusicBrainz::Query::VARIOUS_ARTIST_ID</code>:
 *   The MusicBrainz artist id used to indicate that an album is a
 *   various artist album.
 * * <code>MusicBrainz::Query::Rewind</code>:
 *   Use this query to reset the current context back to the top level of
 *   the response.
 * * <code>MusicBrainz::Query::Back</code>:
 *   Use this query to change the current context back one level.
 * * <code>MusicBrainz::Query::SelectArtist</code>:
 *   Use this Select Query to select an artist from an query that returns
 *   a list of artists. Giving the argument 1 for the ordinal selects 
 *   the first artist in the list, 2 the second and so on. Use 
 *   MusicBrainz::Query::ArtistXXXXXX queries to extract data after
 *   the select.
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectAlbum</code>:
 *   Use this Select Query to select an album from an query that returns
 *   a list of albums. Giving the argument 1 for the ordinal selects 
 *   the first album in the list, 2 the second and so on. Use
 *   MusicBrainz::Query::AlbumXXXXXX queries to extract data after
 *   the select.
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectTrack</code>:
 *   Use this Select Query to select a track from an query that returns
 *   a list of tracks. Giving the argument 1 for the ordinal selects 
 *   the first track in the list, 2 the second and so on. Use
 *   MusicBrainz::Query::TrackXXXXXX queries to extract data after
 *   the select.
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectTrackArtist</code>:
 *   Use this Select Query to select an the corresponding artist from a track 
 *   context. MusicBrainz::Query::ArtistXXXXXX queries to extract
 *   data after the select.
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectTrackAlbum</code>:
 *   Use this Select Query to select an the corresponding artist from a track 
 *   context. MusicBrainz::Query::ArtistXXXXXX queries to extract
 *   data after the select.
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectTrmid</code>:
 *   Use this Select Query to select a trmid from the list. 
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectCdindexid</code>:
 *   Use this Select Query to select a CD Index id from the list. 
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectReleaseDate</code>:
 *   Use this Select Query to select a Release date/country from the list. 
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectLookupResult</code>:
 *   Use this Select Query to select a result from a lookupResultList.
 *   This select will be used in conjunction with
 *   MusicBrainz::Query::FileLookup.
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::SelectLookupResultArtist</code>:
 *   Use this Select Query to select the artist from a lookup result.
 *   This select will be used in conjunction with
 *   MusicBrainz::Query::FileLookup.
 * * <code>MusicBrainz::Query::SelectLookupResultAlbum</code>:
 *   Use this Select Query to select the album from a lookup result.
 *   This select will be used in conjunction with
 *   MusicBrainz::Query::FileLookup.
 * * <code>MusicBrainz::Query::SelectLookupResultTrack</code>:
 *   Use this Select Query to select the track from a lookup result.
 *   This select will be used in conjunction with
 *   MusicBrainz::Query::FileLookup.
 * * <code>MusicBrainz::Query::SelectRelationship</code>:
 *   Use this Select Query to select a relationship from a list
 *   of advanced relationships.
 *   NOTE: This select requires one ordinal argument.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 *
 * == Internal Queries
 * * <code>MusicBrainz::Query::QuerySubject</code>:
 *   Internal use only.
 * * <code>MusicBrainz::Query::GetError</code>:
 *   Internal use only.
 *
 * == Top-Level Queries
 * 
 * The following queries are used with FileInfoLookup.
 *
 * * <code>MusicBrainz::Query::GetStatus</code>:
 *   Get the general return status of this query. Values for this
 *   include OK or fuzzy. Fuzzy is returned when the server made 
 *   a fuzzy match somewhere while handling the query.
 *
 * == Numeric Queries
 * 
 * Queries used to determine the number of items used by a query.
 *
 * * <code>MusicBrainz::Query::GetNumArtists</code>:
 *   Return the number of artist returned in this query.
 * * <code>MusicBrainz::Query::GetNumAlbums</code>:
 *   Return the number of albums returned in this query.
 * * <code>MusicBrainz::Query::GetNumTracks</code>:
 *   Return the number of tracks returned in this query.
 * * <code>MusicBrainz::Query::GetNumTrmids</code>:
 *   Return the number of trmids returned in this query.
 * * <code>MusicBrainz::Query::GetNumLookupResults</code>:
 *   Return the number of lookup results returned in this query.
 *
 * == Artist List Queries
 * * <code>MusicBrainz::Query::ArtistGetArtistName</code>:
 *   Return the name of the currently selected Album
 * * <code>MusicBrainz::Query::ArtistGetArtistSortName</code>:
 *   Return the name of the currently selected Album
 * * <code>MusicBrainz::Query::ArtistGetArtistId</code>:
 *   Return the ID of the currently selected Album. The value of this
 *   query is indeed empty!
 * * <code>MusicBrainz::Query::ArtistGetAlbumName</code>:
 *   Return the name of the nth album. Requires an ordinal argument to select
 *   an album from a list of albums in the current artist
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::ArtistGetAlbumId</code>:
 *   Return the ID of the nth album. Requires an ordinal argument to select
 *   an album from a list of albums in the current artist
 *   NOTE: This select requires one ordinal argument.
 *
 * == Album List Queries
 * * <code>MusicBrainz::Query::AlbumGetAlbumName</code>:
 *   Return the name of the currently selected Album
 * * <code>MusicBrainz::Query::AlbumGetAlbumId</code>:
 *   Return the ID of the currently selected Album. The value of this
 * query is indeed empty!
 * * <code>MusicBrainz::Query::AlbumGetAlbumStatus</code>:
 *   Return the release status of the currently selected Album.
 * * <code>MusicBrainz::Query::AlbumGetAlbumType</code>:
 *   Return the release type of the currently selected Album.
 * * <code>MusicBrainz::Query::AlbumGetAlbumAmazonAsin</code>:
 *   Return the <a href='http://amazon.com/'>Amazon.con</a> ASIN for
 *   the selected Album.
 *   NOTE: This query is only defined for MusicBrainz 2.1.0 and newer.
 * * <code>MusicBrainz::Query::AlbumGetNumCdindexIds</code>:
 *   Return the number of cdindexds returned in this query.
 * * <code>MusicBrainz::Query::AlbumGetNumReleaseDates</code>:
 *   Return the number of release dates returned in this query.
 *   NOTE: This query is only defined for MusicBrainz 2.1.0 and newer.
 * * <code>MusicBrainz::Query::AlbumGetAlbumArtistId</code>:
 *   Return the Artist ID of the currently selected Album. This may return 
 *   the artist id for the Various Artists' artist, and then you should 
 *   check the artist for each track of the album seperately with 
 *   MusicBrainz::Query::AlbumGetArtistName,
 *   MusicBrainz::Query::AlbumGetArtistSortName, and
 *   MusicBrainz::Query::AlbumGetArtistId.
 * * <code>MusicBrainz::Query::AlbumGetNumTracks</code>:
 *   Return the mumber of tracks in the currently selected Album
 * * <code>MusicBrainz::Query::AlbumGetTrackId</code>:
 *   Return the Id of the nth track in the album. Requires a
 *   track index ordinal. 1 for the first track, etc...
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::AlbumGetTrackList</code>:
 *   Return the track list of an album. This extractor should only be used
 *   to specify a list for MusicBrainz::Client#ordinal.
 * * <code>MusicBrainz::Query::AlbumGetTrackNum</code>:
 *   Return the track number of the nth track in the album. Requires a
 *   track index ordinal. 1 for the first track, etc...
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::AlbumGetTrackName</code>:
 *   Return the track name of the nth track in the album. Requires a
 *   track index ordinal. 1 for the first track, etc...
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::AlbumGetTrackDuration</code>:
 *   Return the track duration of the nth track in the album. Requires a
 *   track index ordinal. 1 for the first track, etc...
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::AlbumGetArtistName</code>:
 *   Return the artist name of the nth track in the album. Requires a
 *   track index ordinal. 1 for the first track, etc...
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::AlbumGetArtistSortName</code>:
 *   Return the artist sortname of the nth track in the album. Requires a
 *   track index ordinal. 1 for the first track, etc...
 *   NOTE: This select requires one ordinal argument.
 * * <code>MusicBrainz::Query::AlbumGetArtistId</code>:
 *   Return the artist Id of the nth track in the album. Requires a
 *   track index ordinal. 1 for the first track, etc...
 *   NOTE: This select requires one ordinal argument.
 *
 * == Track List Queries
 * * <code>MusicBrainz::Query::TrackGetTrackName</code>:
 *   Return the name of the currently selected track
 * * <code>MusicBrainz::Query::TrackGetTrackId</code>:
 *   Return the ID of the currently selected track. The value of this
 *   query is indeed empty!
 * * <code>MusicBrainz::Query::TrackGetTrackNum</code>:
 *   Return the track number in the currently selected track
 * * <code>MusicBrainz::Query::TrackGetTrackDuration</code>:
 *   Return the track duration in the currently selected track
 * * <code>MusicBrainz::Query::TrackGetArtistName</code>:
 *   Return the name of the artist for this track. 
 * * <code>MusicBrainz::Query::TrackGetArtistSortName</code>:
 *   Return the sortname of the artist for this track. 
 * * <code>MusicBrainz::Query::TrackGetArtistId</code>:
 *   Return the Id of the artist for this track. 

 * == Quick Track Queries
 * * <code>MusicBrainz::Query::QuickGetArtistName</code>:
 *   Return the name of the aritst
 * * <code>MusicBrainz::Query::QuickGetArtistSortName</code>:
 *   Return the sortname of the artist
 * * <code>MusicBrainz::Query::QuickGetArtistId</code>:
 *   Return the id of the artist
 * * <code>MusicBrainz::Query::QuickGetAlbumName</code>:
 *   Return the name of the album.
 * * <code>MusicBrainz::Query::QuickGetTrackName</code>:
 *   Return the name of the track.
 * * <code>MusicBrainz::Query::QuickGetTrackNum</code>:
 *   Return the track number in the currently selected track.
 * * <code>MusicBrainz::Query::QuickGetTrackId</code>:
 *   Return the MB track id
 * * <code>MusicBrainz::Query::QuickGetTrackDuration</code>:
 *   Return the track duration.
 *
 * == Release Queries
 * * <code>MusicBrainz::Query::ReleaseGetDate</code>:
 *   Return the release date
 * * <code>MusicBrainz::Query::ReleaseGetCountry</code>:
 *   Return the release country
 *
 * == File Lookup Queries
 * * <code>MusicBrainz::Query::LookupGetType</code>:
 *   Return the type of the lookup result
 * * <code>MusicBrainz::Query::LookupGetRelevance</code>:
 *   Return the relevance of the lookup result
 * * <code>MusicBrainz::Query::LookupGetArtistId</code>:
 *   Return the artist id of the lookup result
 * * <code>MusicBrainz::Query::LookupGetAlbumId</code>:
 *   Return the album id of the lookup result
 * * <code>MusicBrainz::Query::LookupGetAlbumArtistId</code>:
 *   Return the artist id of the lookup result
 *   NOTE: This query is only defined for MusicBrainz 2.1.0 and newer.
 * * <code>MusicBrainz::Query::LookupGetTrackId</code>:
 *   Return the track id of the lookup result
 * * <code>MusicBrainz::Query::LookupGetTrackArtistId</code>:
 *   Return the artist id of the lookup result
 *   NOTE: This query is only defined for MusicBrainz 2.1.0 and newer.
 *
 * == Relationship Queries
 *
 * Used to extract results from the
 * MusicBrainz::Query::GetXXXXXRelationsById queries.
 *
 * * <code>MusicBrainz::Query::GetRelationshipType</code>:
 *   Get the type of an advanced relationships link. Please note that these
 *   relationship types can change over time!
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipDirection</code>:
 *   Get the direction of a link between two like entities. This
 *   data element will only be present for links between like types
 *   (eg artist-artist links) and IFF the link direction is 
 *   reverse of what the RDF implies.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipArtistId</code>:
 *   Get the artist id that this link points to.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipArtistName</code>:
 *   Get the artist name that this link points to.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipAlbumId</code>:
 *   Get the album id that this link points to.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipAlbumName</code>:
 *   Get the album name that this link points to.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipTrackId</code>:
 *   Get the track id that this link points to.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipTrackName</code>:
 *   Get the track name that this link points to.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipURL</code>:
 *   Get the URL that this link points to.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetRelationshipAttribute</code>:
 *   Get the vocal/instrument attributes. Must pass an ordinal to
 *   indicate which attribute to get.
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 *
 * == CD Table of Contents Queries
 *
 * Used to extract results from the MusicBrainz::Query::GetCDTOC
 * query.
 *
 * * <code>MusicBrainz::Query::TOCGetCDIndexId</code>:
 *   Return the CDIndex ID from the table of contents from the CD
 * * <code>MusicBrainz::Query::TOCGetFirstTrack</code>:
 *   Return the first track number from the table of contents from the CD
 * * <code>MusicBrainz::Query::TOCGetLastTrack</code>:
 *   Return the last track number (total number of tracks on the CD) 
 *   from the table of contents from the CD
 * * <code>MusicBrainz::Query::TOCGetTrackSectorOffset</code>:
 *   Return the sector offset from the nth track. One ordinal argument must
 *   be given to specify the track. Track 1 is a special lead-out track,
 *   and the actual track 1 on a CD can be retrieved as track 2 and so forth.
 * * <code>MusicBrainz::Query::TOCGetTrackNumSectors</code>:
 *   Return the number of sectors for the nth track. One ordinal
 *   argument must be given to specify the track. Track 1 is a special
 *   lead-out track, and the actual track 1 on a CD can be retrieved
 *   as track 2 and so forth.
 *
 * == Authentication Queries
 *
 * Used to extract results from the
 * MusicBrainz::Query::AuthenticateQuery query.
 *
 * * <code>MusicBrainz::Query::AuthGetSessionId</code>:
 *   Return the Session Id from the Auth Query. This query will be used 
 *   internally by the client library.
 * * <code>MusicBrainz::Query::AuthGetChallenge</code>:
 *   Return the Auth Challenge data from the Auth Query. This query will be used 
 *   internally by the client library.
 *
 * == Local Queries
 * * <code>MusicBrainz::Query::GetCDInfo</code>:
 *   Use this query to look up a CD from MusicBrainz. This query will
 *   examine the CD-ROM in the CD-ROM drive specified by mb_SetDevice
 *   and then send the CD-ROM data to the server. The server will then
 *   find any matching CDs and return then as an albumList.
 * * <code>MusicBrainz::Query::GetCDTOC</code>:
 *   Use this query to examine the table of contents of a CD. This query will
 *   examine the CD-ROM in the CD-ROM drive specified by mb_SetDevice, and
 *   then let the use extract data from the table of contents using the
 *   MBQ_TOCXXXXX functions. No live net connection is required for this query.
 *   NOTE: This query is only defined for MusicBrainz 2.1.0 and newer.
 * * <code>MusicBrainz::Query::AssociateCD</code>:
 *   Internal use only. (For right now)
 * 
 * == Server Queries 
 * 
 * The following queries must have argument(s) substituted in them.
 * 
 * * <code>MusicBrainz::Query::Authenticate</code>:
 *   This query is use to start an authenticated session with the MB server.
 *   The username is sent to the server, and the server responds with 
 *   session id and a challenge sequence that the client needs to use to create 
 *   a session key. The session key and session id need to be provided with
 *   the MusicBrainz::Query::SubmitXXXX functions in order to give
 *   moderators/users credit for their submissions. This query will be
 *   carried out by the client libary automatically -- you should not
 *   need to use it.
 * * <code>MusicBrainz::Query::GetCDInfoFromCDIndexId</code>:
 *   Use this query to return an albumList for the given CD Index Id
 * * <code>MusicBrainz::Query::TrackInfoFromTRMId</code>:
 *   Use this query to return the metadata information (artistname,
 *   albumname, trackname, tracknumber) for a given trm id. Optionally, 
 *   you can also specifiy the basic artist metadata, so that if the server
 *   cannot match on the TRM id, it will attempt to match based on the basic
 *   metadata.
 *   In case of a TRM collision (where one TRM may point to more than one track)
 *   this function will return more than on track. The user (or tagging app)
 *   must decide which track information is correct.
 *
 *   Parameters:
 *   1. trmid: The TRM id for the track to be looked up
 *   2. artistName: The name of the artist
 *   3. albumName: The name of the album
 *   4. trackName: The name of the track
 *   5. trackNum: The number of the track
 * * <code>MusicBrainz::Query::QuickTrackInfoFromTrackId</code>:
 *   Use this query to return the basic metadata information (artistname,
 *   albumname, trackname, tracknumber) for a given track mb id
 * * <code>MusicBrainz::Query::FindArtistByName</code>:
 *   Use this query to find artists by name. This function returns an artistList 
 *   for the given artist name.
 * * <code>MusicBrainz::Query::FindAlbumByName</code>:
 *   Use this query to find albums by name. This function returns an albumList 
 *   for the given album name. 
 * * <code>MusicBrainz::Query::FindTrackByName</code>:
 *   Use this query to find tracks by name. This function returns a trackList 
 *   for the given track name. 
 * * <code>MusicBrainz::Query::FindDistinctTRMId</code>:
 *   Use this function to find TRM Ids that match a given artistName
 *   and trackName, This query returns a trmidList.
 * * <code>MusicBrainz::Query::GetArtistById</code>:
 *   Retrieve an artistList from a given Artist id 
 * * <code>MusicBrainz::Query::GetAlbumById</code>:
 *   Retrieve an albumList from a given Album id 
 * * <code>MusicBrainz::Query::GetTrackById</code>:
 *   Retrieve an trackList from a given Track id 
 * * <code>MusicBrainz::Query::GetTrackByTRMId</code>:
 *   Retrieve an trackList from a given TRM Id 
 * * <code>MusicBrainz::Query::GetArtistRelationsById</code>:
 *   Retrieve an artistList with advanced relationships from a given artist id
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetAlbumRelationsById</code>:
 *   Retrieve an albumList with advanced relationships from a given album id
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 * * <code>MusicBrainz::Query::GetTrackRelationsById</code>:
 *   Retrieve a trackList with advanced relationships from a given track id
 *   NOTE: This query is only defined for MusicBrainz 2.1.2 and newer.
 *
 * == Internal Submission Queries
 * * <code>MusicBrainz::Query::SubmitTrack</code>:
 *   Internal use only.
 * * <code>MusicBrainz::Query::SubmitTrackTRMId</code>:
 *   Submit a single TrackId, TRM Id pair to MusicBrainz. This query can
 *   handle only one pair at a time, which is inefficient. The user may wish
 *   to create the query RDF text by hand and provide more than one pair
 *   in the rdf:Bag, since the server can handle up to 1000 pairs in one
 *   query.
 *   
 *   Parameters:
 *   1. TrackGID: The Global ID field of the track
 *   2. trmid: The TRM Id of the track.
 * * <code>MusicBrainz::Query::FileInfoLookup</code>:
 *   Lookup metadata for one file. This function can be used by tagging applications
 *   to attempt to match a given track with a track in the database. The server will
 *   attempt to match an artist, album and track during three phases. If 
 *   at any one lookup phase the server finds ONE item only, it will move on to
 *   to the next phase. If no items are returned, an error message is returned. If 
 *   more then one item is returned, the end-user will have to choose one from
 *   the returned list and then make another call to the server. To express the
 *   choice made by a user, the client should leave the artistName/albumName empty and 
 *   provide the artistId and/or albumId empty on the subsequent call. Once an artistId
 *   or albumId is provided the server will pick up from the given Ids and attempt to
 *   resolve the next phase.
 *   
 *   Parameters:
 *   1. ArtistName: The name of the artist, gathered from ID3 tags or user input
 *   2. AlbumName: The name of the album, also from ID3 or user input
 *   3. TrackName: The name of the track
 *   4. TrackNum: The track number of the track being matched
 *   5. Duration: The duration of the track being matched
 *   6. FileName: The name of the file that is being matched. This will only be used if the ArtistName, AlbumName or TrackName fields are blank. 
 *   7. ArtistId: The AritstId resolved from an earlier call. 
 *   8. AlbumId: The AlbumId resolved from an earlier call. 
 *   9. MaxItems: The maximum number of items to return if the server cannot determine an exact match.
 *
 */
static void define_queries(void) {
  mQuery = rb_define_module_under(mMB, "Query");

  MB_QUERY("MBI", "VARIOUS_ARTIST_ID", MBI_VARIOUS_ARTIST_ID);

  MB_QUERY("MBS", "Rewind", MBS_Rewind);
  MB_QUERY("MBS", "Back", MBS_Back);

  MB_QUERY("MBS", "SelectArtist", MBS_SelectArtist);
  MB_QUERY("MBS", "SelectAlbum", MBS_SelectAlbum);
  MB_QUERY("MBS", "SelectTrack", MBS_SelectTrack);
  MB_QUERY("MBS", "SelectTrackArtist", MBS_SelectTrackArtist);
  MB_QUERY("MBS", "SelectTrackAlbum", MBS_SelectTrackAlbum);
  MB_QUERY("MBS", "SelectTrmid", MBS_SelectTrmid);
  MB_QUERY("MBS", "SelectCdindexid", MBS_SelectCdindexid);
  MB_QUERY("MBS", "SelectLookupResult", MBS_SelectLookupResult);
  MB_QUERY("MBS", "SelectLookupResultArtist", MBS_SelectLookupResultArtist);
  MB_QUERY("MBS", "SelectLookupResultTrack", MBS_SelectLookupResultTrack);
#ifdef MBS_SelectRelationship
  MB_QUERY("MBS", "SelectRelationship", MBS_SelectRelationship);
#endif /* MBS_SelectRelationship */
#ifdef MBS_SelectReleaseDate
  MB_QUERY("MBS", "SelectReleaseDate", MBS_SelectReleaseDate);
#endif /* MBS_SelectReleaseDate */

  MB_QUERY("MBE", "QuerySubject", MBE_QuerySubject);
  MB_QUERY("MBE", "GetError", MBE_GetError);

  MB_QUERY("MBE", "GetStatus", MBE_GetStatus);
  MB_QUERY("MBE", "GetNumArtists", MBE_GetNumArtists);
  MB_QUERY("MBE", "GetNumAlbums", MBE_GetNumAlbums);
  MB_QUERY("MBE", "GetNumTracks", MBE_GetNumTracks);
  MB_QUERY("MBE", "GetNumTrmids", MBE_GetNumTrmids);
  MB_QUERY("MBE", "GetNumLookupResults", MBE_GetNumLookupResults);

  MB_QUERY("MBE", "ArtistGetArtistName", MBE_ArtistGetArtistName);
  MB_QUERY("MBE", "ArtistGetArtistSortName", MBE_ArtistGetArtistSortName);
  MB_QUERY("MBE", "ArtistGetArtistId", MBE_ArtistGetArtistId);
  MB_QUERY("MBE", "ArtistGetAlbumName", MBE_ArtistGetAlbumName);
  MB_QUERY("MBE", "ArtistGetAlbumId", MBE_ArtistGetAlbumId);

  MB_QUERY("MBE", "AlbumGetAlbumName", MBE_AlbumGetAlbumName);
  MB_QUERY("MBE", "AlbumGetAlbumId", MBE_AlbumGetAlbumId);
  MB_QUERY("MBE", "AlbumGetAlbumStatus", MBE_AlbumGetAlbumStatus);
  MB_QUERY("MBE", "AlbumGetAlbumType", MBE_AlbumGetAlbumType);
#ifdef MBE_AlbumGetAmazonAsin
  MB_QUERY("MBE", "AlbumGetAmazonAsin", MBE_AlbumGetAmazonAsin);
#endif /* MBE_AlbumGetAmazonAsin */
  MB_QUERY("MBE", "AlbumGetNumCdindexIds", MBE_AlbumGetNumCdindexIds);
#ifdef MBE_AlbumGetNumReleaseDates
  MB_QUERY("MBE", "AlbumGetNumReleaseDates", MBE_AlbumGetNumReleaseDates);
#endif /* MBE_AlbumGetNumReleaseDates */
  MB_QUERY("MBE", "AlbumGetAlbumArtistId", MBE_AlbumGetAlbumArtistId);
  MB_QUERY("MBE", "AlbumGetNumTracks", MBE_AlbumGetNumTracks);

  MB_QUERY("MBE", "AlbumGetTrackId", MBE_AlbumGetTrackId);
  MB_QUERY("MBE", "AlbumGetTrackList", MBE_AlbumGetTrackList);
  MB_QUERY("MBE", "AlbumGetTrackNum", MBE_AlbumGetTrackNum);
  MB_QUERY("MBE", "AlbumGetTrackName", MBE_AlbumGetTrackName);
  MB_QUERY("MBE", "AlbumGetTrackDuration", MBE_AlbumGetTrackDuration);

  MB_QUERY("MBE", "AlbumGetArtistName", MBE_AlbumGetArtistName);
  MB_QUERY("MBE", "AlbumGetArtistSortName", MBE_AlbumGetArtistSortName);
  MB_QUERY("MBE", "AlbumGetArtistId", MBE_AlbumGetArtistId);

  MB_QUERY("MBE", "TrackGetTrackName", MBE_TrackGetTrackName);
  MB_QUERY("MBE", "TrackGetTrackId", MBE_TrackGetTrackId);
  MB_QUERY("MBE", "TrackGetTrackNum", MBE_TrackGetTrackNum);
  MB_QUERY("MBE", "TrackGetTrackDuration", MBE_TrackGetTrackDuration);
  MB_QUERY("MBE", "TrackGetArtistName", MBE_TrackGetArtistName);
  MB_QUERY("MBE", "TrackGetArtistSortName", MBE_TrackGetArtistSortName);
  MB_QUERY("MBE", "TrackGetArtistId", MBE_TrackGetArtistId);

  MB_QUERY("MBE", "QuickGetArtistName", MBE_QuickGetArtistName);
#ifdef MBE_QuickGetArtistSortName
  MB_QUERY("MBE", "QuickGetArtistSortName", MBE_QuickGetArtistSortName);
#endif /* MBE_QuickGetArtistSortName */
#ifdef MBE_QuickGetArtistId
  MB_QUERY("MBE", "QuickGetArtistId", MBE_QuickGetArtistId);
#endif /* MBE_QuickGetArtistId */
  MB_QUERY("MBE", "QuickGetAlbumName", MBE_QuickGetAlbumName);
  MB_QUERY("MBE", "QuickGetTrackName", MBE_QuickGetTrackName);
  MB_QUERY("MBE", "QuickGetTrackNum", MBE_QuickGetTrackNum);
  MB_QUERY("MBE", "QuickGetTrackId", MBE_QuickGetTrackId);
  MB_QUERY("MBE", "QuickGetTrackDuration", MBE_QuickGetTrackDuration);

#ifdef MBE_ReleaseGetDate
  MB_QUERY("MBE", "ReleaseGetDate", MBE_ReleaseGetDate);
#endif /* MBE_ReleaseGetDate */
#ifdef MBE_ReleaseGetCountry
  MB_QUERY("MBE", "ReleaseGetCountry", MBE_ReleaseGetCountry);
#endif /* MBE_ReleaseGetCountry */

  MB_QUERY("MBE", "LookupGetType", MBE_LookupGetType);
  MB_QUERY("MBE", "LookupGetRelevance", MBE_LookupGetRelevance);
  MB_QUERY("MBE", "LookupGetArtistId", MBE_LookupGetArtistId);
  MB_QUERY("MBE", "LookupGetAlbumId", MBE_LookupGetAlbumId);
#ifdef MBE_LookupGetAlbumArtistId
  MB_QUERY("MBE", "LookupGetAlbumArtistId", MBE_LookupGetAlbumArtistId);
#endif /* MBE_LookupGetAlbumArtistId */
  MB_QUERY("MBE", "LookupGetTrackId", MBE_LookupGetTrackId);
#ifdef MBE_LookupGetTrackArtistId
  MB_QUERY("MBE", "LookupGetTrackArtistId", MBE_LookupGetTrackArtistId);
#endif /* MBE_LookupGetTrackArtistId */

  MB_QUERY("MBE", "TOCGetCDIndexId", MBE_TOCGetCDIndexId);
  MB_QUERY("MBE", "TOCGetFirstTrack", MBE_TOCGetFirstTrack);
  MB_QUERY("MBE", "TOCGetLastTrack", MBE_TOCGetLastTrack);
  MB_QUERY("MBE", "TOCGetTrackSectorOffset", MBE_TOCGetTrackSectorOffset);
  MB_QUERY("MBE", "TOCGetTrackNumSectors", MBE_TOCGetTrackNumSectors);

  MB_QUERY("MBE", "AuthGetSessionId", MBE_AuthGetSessionId);
  MB_QUERY("MBE", "AuthGetChallenge", MBE_AuthGetChallenge);

  MB_QUERY("MBQ", "GetCDInfo", MBQ_GetCDInfo);
#ifdef MBQ_GetCDTOC
  MB_QUERY("MBQ", "GetCDTOC", MBQ_GetCDTOC);
#endif /* MBQ_GetCDTOC */
  MB_QUERY("MBQ", "AssociateCD", MBQ_AssociateCD);

  MB_QUERY("MBQ", "Authenticate", MBQ_Authenticate);
  MB_QUERY("MBQ", "GetCDInfoFromCDIndexId", MBQ_GetCDInfoFromCDIndexId);
  MB_QUERY("MBQ", "TrackInfoFromTRMId", MBQ_TrackInfoFromTRMId);
  MB_QUERY("MBQ", "QuickTrackInfoFromTrackId", MBQ_QuickTrackInfoFromTrackId);
  MB_QUERY("MBQ", "FindArtistByName", MBQ_FindArtistByName);
  MB_QUERY("MBQ", "FindAlbumByName", MBQ_FindAlbumByName);
  MB_QUERY("MBQ", "FindTrackByName", MBQ_FindTrackByName);
  MB_QUERY("MBQ", "FindDistinctTRMId", MBQ_FindDistinctTRMId);
  MB_QUERY("MBQ", "GetArtistById", MBQ_GetArtistById);
  MB_QUERY("MBQ", "GetAlbumById", MBQ_GetAlbumById);
  MB_QUERY("MBQ", "GetTrackById", MBQ_GetTrackById);
  MB_QUERY("MBQ", "GetTrackByTRMId", MBQ_GetTrackByTRMId);
#ifdef MBQ_GetArtistRelationsById
  MB_QUERY("MBQ", "GetArtistRelationsById", MBQ_GetArtistRelationsById);
#endif /* MBQ_GetArtistRelationsById */
#ifdef MBQ_GetAlbumRelationsById
  MB_QUERY("MBQ", "GetAlbumRelationsById", MBQ_GetAlbumRelationsById);
#endif /* MBQ_GetAlbumRelationsById */
#ifdef MBQ_GetTrackRelationsById
  MB_QUERY("MBQ", "GetTrackRelationsById", MBQ_GetTrackRelationsById);
#endif /* MBQ_GetTrackRelationsById */

  MB_QUERY("MBQ", "SubmitTrack", MBQ_SubmitTrack);
  MB_QUERY("MBQ", "SubmitTrackTRMId", MBQ_SubmitTrackTRMId);
  MB_QUERY("MBQ", "FileInfoLookup", MBQ_FileInfoLookup);
}

void Init_musicbrainz(void) {
  mMB = rb_define_module("MusicBrainz");

  /* Version of the MB-Ruby bindings.  (use MusicBrainz::Client#version for the client library version). 
   */
  rb_define_const(mMB, "VERSION", rb_str_new2(MB_VERSION));

  /*
   * Document-class: MusicBrainz::Error
   *
   * Error class for MusicBrainz errors.  Exceptions raised by the
   * library are wrapped by this class.  If you want to catch all
   * MusicBrainz-related errors, for example, you could do something
   * like this: 
   *
   *   begin
   *     # run query
   *     mb.query MusicBrainz::Query::GetStatus
   *   rescue MusicBrainz::Error => e
   *     # catch MusicBrainz exceptions
   *     $stderr.puts "MusicBrainz error: #{e}"
   *   rescue Exception => e
   *     # catch Ruby exceptions
   *     $stderr.puts "Ruby error: #{e}"
   *   end
   *
   * Note that several methods -- in particular, MusicBrainz::Client#select and
   * MusicBrainz::Client#query -- return false rather than raising an
   * exception to indicate an error. 
   *
   */
  eErr = rb_define_class_under(mMB, "Error", rb_eStandardError);

  /* Length of a returned ID value. */
  rb_define_const(mMB, "ID_LEN", INT2FIX(MB_ID_LEN));
  /* Length of a returned ID value. */
  rb_define_const(mMB, "MB_ID_LEN", INT2FIX(MB_ID_LEN));

  /* Length of a returned CD index ID value. */
  rb_define_const(mMB, "CDINDEX_ID_LEN", INT2FIX(MB_CDINDEX_ID_LEN));
  /* Length of a returned CD index ID value. */
  rb_define_const(mMB, "MB_CDINDEX_ID_LEN", INT2FIX(MB_CDINDEX_ID_LEN));
  
  define_queries();
  
  /************************************/
  /* define MusicBrainz::Client class */
  /************************************/
  cClient = rb_define_class_under(mMB, "Client", rb_cObject);

#ifdef HAVE_RB_DEFINE_ALLOC_FUNC
  rb_define_alloc_func(cClient, mb_client_alloc);
#else /* !HAVE_RB_DEFINE_ALLOC_FUNC */
  rb_define_singleton_method(cClient, "new", mb_client_new, 0);
#endif /* HAVE_RB_DEFINE_ALLOC_FUNC */
  rb_define_method(cClient, "initialize", mb_client_init, 0);

  rb_define_method(cClient, "version", mb_client_version, 0);
  rb_define_alias(cClient, "get_version", "version");
  
  rb_define_method(cClient, "server=", mb_client_set_server, -1);
  rb_define_alias(cClient, "set_server", "server=");

  rb_define_method(cClient, "debug=", mb_client_set_debug, 1);

  rb_define_method(cClient, "proxy=", mb_client_set_proxy, -1);
  rb_define_alias(cClient, "set_proxy", "proxy=");

  rb_define_method(cClient, "auth", mb_client_auth, 2);
  rb_define_alias(cClient, "authenticate", "auth");

  rb_define_method(cClient, "device=", mb_client_set_device, 1);
  rb_define_alias(cClient, "set_device", "device=");

  rb_define_method(cClient, "utf8=", mb_client_set_use_utf8, 1);
  rb_define_alias(cClient, "use_utf8=", "utf8=");
  rb_define_alias(cClient, "set_use_utf8", "utf8=");

  rb_define_method(cClient, "depth=", mb_client_set_depth, 1);
  rb_define_alias(cClient, "set_depth", "depth=");

  rb_define_method(cClient, "max_items=", mb_client_set_max_items, 1);
  rb_define_alias(cClient, "set_max_items", "max_items=");

  rb_define_method(cClient, "query", mb_client_query, -1);

  rb_define_method(cClient, "url", mb_client_url, 0);
  rb_define_alias(cClient, "get_url", "url");
  rb_define_alias(cClient, "get_web_submit_url", "url");

  rb_define_method(cClient, "error", mb_client_error, 0);
  rb_define_alias(cClient, "get_error", "error");
  rb_define_alias(cClient, "get_query_error", "error");

  rb_define_method(cClient, "select", mb_client_select, -1);

  rb_define_method(cClient, "result", mb_client_result, -1);
  rb_define_alias(cClient, "get_result", "result");
  rb_define_alias(cClient, "get_result_data", "result");

  rb_define_method(cClient, "result_int", mb_client_result_int, -1);
  rb_define_alias(cClient, "get_result_int", "result_int");

  rb_define_method(cClient, "exists?", mb_client_exists, -1);
  rb_define_alias(cClient, "result_exists?", "exists?");
  rb_define_alias(cClient, "does_result_exist?", "exists?");

  rb_define_method(cClient, "rdf", mb_client_rdf, 0);
  rb_define_alias(cClient, "result_rdf", "rdf");
  rb_define_alias(cClient, "get_rdf", "rdf");
  rb_define_alias(cClient, "get_result_rdf", "rdf");

  rb_define_method(cClient, "rdf_len", mb_client_rdf_len, 0);
  rb_define_alias(cClient, "result_rdf_len", "rdf_len");
  rb_define_alias(cClient, "get_rdf_len", "rdf_len");
  rb_define_alias(cClient, "get_result_rdf_len", "rdf_len");

  rb_define_method(cClient, "rdf=", mb_client_set_rdf, 1);
  rb_define_alias(cClient, "set_rdf", "rdf=");
  rb_define_alias(cClient, "result_rdf=", "rdf=");
  rb_define_alias(cClient, "set_result_rdf", "rdf=");

  rb_define_method(cClient, "id_from_url", mb_client_id_from_url, 1);
  rb_define_alias(cClient, "get_id_from_url", "id_from_url");

  rb_define_method(cClient, "fragment_from_url", mb_client_frag_from_url, 1);
  rb_define_alias(cClient, "get_fragment_from_url", "fragment_from_url");

  rb_define_method(cClient, "ordinal", mb_client_ordinal, 2);
  rb_define_alias(cClient, "get_ordinal", "ordinal");
  rb_define_alias(cClient, "get_ordinal_from_list", "ordinal");

  rb_define_method(cClient, "mp3_info", mb_client_mp3_info, 1);
  rb_define_alias(cClient, "get_mp3_info", "mp3_info");

  rb_define_method(cClient, "launch", mb_client_launch, 2);
  rb_define_alias(cClient, "browser", "launch");
  rb_define_alias(cClient, "launch_browser", "launch");
  
  /********************/
  /* define TRM class */
  /********************/
  cTRM = rb_define_class_under(mMB, "TRM", rb_cObject);

#ifdef HAVE_RB_DEFINE_ALLOC_FUNC
  rb_define_alloc_func(cClient, mb_trm_alloc);
#else /* !HAVE_RB_DEFINE_ALLOC_FUNC */
  rb_define_singleton_method(cTRM, "new", mb_trm_new, 0);
#endif /* HAVE_RB_DEFINE_ALLOC_FUNC */
  rb_define_method(cTRM, "initialize", mb_trm_init, 0);

  rb_define_method(cTRM, "proxy=", mb_trm_set_proxy, -1);
  rb_define_alias(cTRM, "set_proxy", "proxy=");

  rb_define_method(cTRM, "pcm_data", mb_trm_set_pcm_data, 3);
  rb_define_alias(cTRM, "set_pcm_data", "pcm_data");
  rb_define_alias(cTRM, "pcm_data_info", "pcm_data");
  rb_define_alias(cTRM, "set_pcm_data_info", "pcm_data");

  rb_define_method(cTRM, "length=", mb_trm_set_length, 1);
  rb_define_alias(cTRM, "set_length", "length=");
  rb_define_alias(cTRM, "song_length=", "length=");
  rb_define_alias(cTRM, "set_song_length", "length=");

  rb_define_method(cTRM, "generate_signature", mb_trm_gen_sig, 1);
  rb_define_method(cTRM, "finalize_signature", mb_trm_finalize_sig, -1);

  rb_define_method(cTRM, "convert_sig", mb_trm_convert_sig, 1);
  rb_define_alias(cTRM, "sig_to_ascii", "convert_sig");
  rb_define_alias(cTRM, "convert_sig_to_ascii", "convert_sig");
}
