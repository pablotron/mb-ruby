#include <ruby.h>
#include <musicbrainz/mb_c.h>
#include <musicbrainz/queries.h>
#include <musicbrainz/queries.h>

#define MB_VERSION "0.1.0"
#define UNUSED(a) ((void) (a))

#define MB_QUERY(a,b,c)                  \
  do {                                   \
    VALUE v = rb_str_new2(c);            \
    rb_define_const(mQuery, (b), v);     \
    rb_define_const(mQuery, a "_" b, v); \
  } while (0)

static VALUE mMB, /* MusicBrainz */
             cClient,
             cTRM,
             mQuery;

/*******************************/
/* MusicBrainz::Client methods */
/*******************************/
static void client_free(void *mb) {
  mb_Delete(* (musicbrainz_t*) mb);
  free(mb);
}

/*
 * Allocate and initialize a new MusicBrainz::Client object.
 *
 * Example:
 *   mb = MusicBrainz::Client.new
 */
VALUE mb_client_new(VALUE klass) {
  VALUE self;
  musicbrainz_t *mb;

  mb = malloc(sizeof(musicbrainz_t));
  *mb = mb_New();

  self = Data_Wrap_Struct(klass, 0, client_free, mb);
  rb_obj_call_init(self, 0, NULL);

  return self;
}

/*
 * Constructor for MusicBrainz::Client object.
 *
 * This method is currently empty.  You should never call this method
 * directly unless you're instantiating a derived class (ie, you know
 * what you're doing).
 *
 */
static VALUE mb_client_init(VALUE self) {
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
  char buf[BUFSIZ];
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
  char *ptr, host[BUFSIZ];
  int port;

  Data_Get_Struct(self, musicbrainz_t, mb);
  
  memset(host, 0, sizeof(host));
  port = 0;
  
  switch (argc) {
    case 1:
      port = 80;
      snprintf(host, sizeof(host), "%s", RSTRING(argv[0])->ptr);
      if ((ptr = strstr(host, ":")) != NULL) {
        ptr = '\0';
        port = atoi(ptr + 1);
      }
      break;
    case 2:
      snprintf(host, sizeof(host), "%s", RSTRING(argv[0])->ptr);
      port = NUM2INT(argv[1]);
      break;
    default:
      rb_raise(rb_eException, "Invalid argument count: %d.", argc);
  }
  
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
  char *ptr, host[BUFSIZ];
  int port;

  Data_Get_Struct(self, musicbrainz_t, mb);
  
  memset(host, 0, sizeof(host));
  port = 0;
  
  switch (argc) {
    case 1:
      port = 8080;
      snprintf(host, sizeof(host), "%s", RSTRING(argv[0])->ptr);
      if ((ptr = strstr(host, ":")) != NULL) {
        ptr = '\0';
        port = atoi(ptr + 1);
      }
      break;
    case 2:
      snprintf(host, sizeof(host), "%s", RSTRING(argv[0])->ptr);
      port = NUM2INT(argv[1]);
      break;
    default:
      rb_raise(rb_eException, "Invalid argument count: %d.", argc);
  }
  
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
  u = RSTRING(user)->ptr; p = RSTRING(pass)->ptr;

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
  return mb_SetDevice(*mb, RSTRING(device)->ptr) ? Qtrue : Qfalse;
}

/*
 * Enable UTF8 output for a MusicBrainz::Client object.
 * 
 * Note: Defaults to ISO-8859-1 output.  If this is set to true, then
 * UTF8 will be used instead.
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
 * TODO: See the MusicBrainz documentation for information on various
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
  switch(argc) {
    case 0:
      rb_raise(rb_eException, "Invalid argument count: %d.", argc);
      break;
    case 1:
      ret = mb_Query(*mb, RSTRING(argv[0])->ptr) ? Qtrue : Qfalse;
      break;
    default:
      args = malloc(sizeof(char*) * argc);
      obj = RSTRING(argv[0])->ptr;
      for (i = 1; i < argc; i++)
        args[i - 1] = RSTRING(argv[i])->ptr;
      args[argc - 1] = NULL;

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
  char buf[BUFSIZ];
  int len;
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
  char buf[BUFSIZ];
  int len;

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
 * TODO: This method uses select queries (MBS_*).  See the MusicBrainz
 * documentation for information on various query types.
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
  switch(argc) {
    case 0:
      rb_raise(rb_eException, "Invalid argument count: %d.", argc);
      break;
    case 1:
      ret = mb_Select(*mb, RSTRING(argv[0])->ptr) ? Qtrue : Qfalse;
      break;
    case 2:
      obj = RSTRING(argv[0])->ptr;
      i = FIX2INT(argv[1]);
      ret = mb_Select1(*mb, obj, i) ? Qtrue : Qfalse;
      break;
    default:
      args = malloc(sizeof(int) * argc);
      obj = RSTRING(argv[0])->ptr;
      for (i = 1; i < argc; i++)
        args[i - 1] = FIX2INT(argv[i]);
      args[argc - 1] = 0;

      ret = mb_SelectWithArgs(*mb, obj, args) ? Qtrue : Qfalse;
      free(args);
  }

  return ret;
}

/* Extract a piece of information from the data returned by a successful query by a MusicBrainz::Client object.
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
  char *obj, buf[BUFSIZ];

  Data_Get_Struct(self, musicbrainz_t, mb);
  obj = argc ? RSTRING(argv[0])->ptr : NULL;
  switch(argc) {
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
      rb_raise(rb_eException, "Invalid argument count: %d.", argc);
  }

  return ret;
}

/* See if a piece of information exists in data returned by a successful query by a MusicBrainz::Client object.
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
  obj = argc ? RSTRING(argv[0])->ptr : NULL;
  switch(argc) {
    case 1:
      ret = mb_DoesResultExist(*mb, obj) ? Qtrue : Qfalse;
      break;
    case 2:
      ret = mb_DoesResultExist1(*mb, obj, FIX2INT(argv[1])) ? Qtrue : Qfalse;
      break;
    default:
      rb_raise(rb_eException, "Invalid argument count: %d.", argc);
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
      ret = rb_str_new2(buf);
      free(buf);
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
  return mb_SetResultRDF(*mb, RSTRING(rdf)->ptr) ? Qtrue : Qfalse;
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
  char buf[BUFSIZ];

  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_GetIDFromURL(*mb, RSTRING(url)->ptr, buf, sizeof(buf));

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
  char buf[BUFSIZ];

  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_GetFragmentFromURL(*mb, RSTRING(url)->ptr, buf, sizeof(buf));

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
  return INT2FIX(mb_GetOrdinalFromList(*mb,
                                       RSTRING(list)->ptr,
                                       RSTRING(uri)->ptr));
}

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
  char buf[41];

  Data_Get_Struct(self, musicbrainz_t, mb);
  mb_CalculateSha1(*mb, RSTRING(path)->ptr, buf);

  return rb_str_new2(buf);
}

/* 
 * Calculate the Bitzi bitprint info for a given filename.
 *
 * Aliases:
 *   MusicBrainz::Client#calculate_bitprint
 *
 * Examples:
 *   # print the bitprint for a file called 'foo.mp3'
 *   bp = mb.bitprint 'foo.mp3'
 *   puts 'foo.mp3 bitprint: ' << bp['bitprint']
 *
 */
static VALUE mb_client_bitprint(VALUE self, VALUE path) {
  musicbrainz_t *mb;
  BitprintInfo *info;
  VALUE ret = Qnil;

  Data_Get_Struct(self, musicbrainz_t, mb);
  if (mb_CalculateBitprint(*mb, RSTRING(path)->ptr, info)) {
    ret = rb_hash_new();
    rb_hash_aset(ret, rb_str_new2("filename"), rb_str_new2(info->filename));
    rb_hash_aset(ret, rb_str_new2("bitprint"), rb_str_new2(info->bitprint));
    rb_hash_aset(ret, rb_str_new2("first20"), rb_str_new2(info->first20));
    rb_hash_aset(ret, rb_str_new2("audioSha1"), rb_str_new2(info->audioSha1));
    rb_hash_aset(ret, rb_str_new2("length"), INT2FIX(info->length));
    rb_hash_aset(ret, rb_str_new2("duration"), INT2FIX(info->duration));
    rb_hash_aset(ret, rb_str_new2("samplerate"), INT2FIX(info->samplerate));
    rb_hash_aset(ret, rb_str_new2("bitrate"), INT2FIX(info->bitrate));
    rb_hash_aset(ret, rb_str_new2("stereo"), INT2FIX(info->stereo));
    rb_hash_aset(ret, rb_str_new2("vbr"), INT2FIX(info->vbr));
  }

  return ret;
}

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
  BitprintInfo *info;
  VALUE ret = Qnil;
  int dr, br, st, sr;

  Data_Get_Struct(self, musicbrainz_t, mb);
  if (mb_GetMP3Info(*mb, RSTRING(path)->ptr, &dr, &br, &st, &sr)) {
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
  url_str = url ? RSTRING(url)->ptr : NULL;
  browser_str = browser ? RSTRING(browser)->ptr : NULL;
  return LaunchBrowser(url_str, browser_str) ? Qtrue : Qfalse;
}

/****************************/
/* MusicBrainz::TRM methods */
/****************************/
static void trm_free(void *trm) {
  trm_Delete(* (trm_t*) trm);
  free(trm);
}

/*
 * Allocate and initialize a new MusicBrainz::TRM object.
 *
 * Example:
 *   trm = MusicBrainz::TRM.new
 */
VALUE mb_trm_new(VALUE klass) {
  VALUE self;
  trm_t *trm;

  trm = malloc(sizeof(trm_t));
  *trm = trm_New();

  self = Data_Wrap_Struct(klass, 0, trm_free, trm);
  rb_obj_call_init(self, 0, NULL);

  return self;
}

/*
 * Constructor for MusicBrainz::TRM object.
 *
 * This method is currently empty.  You should never call this method
 * directly unless you're instantiating a derived class (ie, you know
 * what you're doing).
 *
 */
static VALUE mb_trm_init(VALUE self) {
  return self;
}

/*
 * Set the proxy name and port for the MusicBrainz::TRM object.
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
  char *ptr, host[BUFSIZ];
  int port;

  Data_Get_Struct(self, trm_t, trm);
  
  memset(host, 0, sizeof(host));
  port = 0;
  
  switch (argc) {
    case 1:
      port = 8080;
      snprintf(host, sizeof(host), "%s", RSTRING(argv[0])->ptr);
      if ((ptr = strstr(host, ":")) != NULL) {
        ptr = '\0';
        port = atoi(ptr + 1);
      }
      break;
    case 2:
      snprintf(host, sizeof(host), "%s", RSTRING(argv[0])->ptr);
      port = NUM2INT(argv[1]);
      break;
    default:
      rb_raise(rb_eException, "Invalid argument count: %d.", argc);
  }
  
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
 *   trm.generate_signature buf, BUFSIZ
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
 * Call this after MusicBrainz::TRM#generate_signature has returned 1.
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
  char sig[17], *id = NULL;
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
      rb_raise(rb_eException, "Invalid argument count: %d.", argc);
  }

  if (trm_FinalizeSignature(*trm, sig, id))
    ret = rb_str_new(sig, 16);

  return ret;
}

/*
 * Convert 16-bypte raw signature into a human-readable 36-byte ASCII string.
 *
 * Used after MusicBrainz::TRM#generate_signature has returned false and
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
  char buf[64];
  Data_Get_Struct(self, trm_t, trm);
  trm_ConvertSigToASCII(*trm, RSTRING(sig)->ptr, buf);
  return rb_str_new2(buf);
}


/******************/
/* init functions */
/******************/
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
  MB_QUERY("MBE", "AlbumGetNumCdindexIds", MBE_AlbumGetNumCdindexIds);
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
  MB_QUERY("MBE", "QuickGetAlbumName", MBE_QuickGetAlbumName);
  MB_QUERY("MBE", "QuickGetTrackName", MBE_QuickGetTrackName);
  MB_QUERY("MBE", "QuickGetTrackNum", MBE_QuickGetTrackNum);
  MB_QUERY("MBE", "QuickGetTrackId", MBE_QuickGetTrackId);
  MB_QUERY("MBE", "QuickGetTrackDuration", MBE_QuickGetTrackDuration);

  MB_QUERY("MBE", "LookupGetType", MBE_LookupGetType);
  MB_QUERY("MBE", "LookupGetRelevance", MBE_LookupGetRelevance);
  MB_QUERY("MBE", "LookupGetArtistId", MBE_LookupGetArtistId);
  MB_QUERY("MBE", "LookupGetAlbumId", MBE_LookupGetAlbumId);
  MB_QUERY("MBE", "LookupGetTrackId", MBE_LookupGetTrackId);

  MB_QUERY("MBE", "TOCGetCDIndexId", MBE_TOCGetCDIndexId);
  MB_QUERY("MBE", "TOCGetFirstTrack", MBE_TOCGetFirstTrack);
  MB_QUERY("MBE", "TOCGetLastTrack", MBE_TOCGetLastTrack);
  MB_QUERY("MBE", "TOCGetTrackSectorOffset", MBE_TOCGetTrackSectorOffset);
  MB_QUERY("MBE", "TOCGetTrackNumSectors", MBE_TOCGetTrackNumSectors);

  MB_QUERY("MBE", "AuthGetSessionId", MBE_AuthGetSessionId);
  MB_QUERY("MBE", "AuthGetChallenge", MBE_AuthGetChallenge);

  MB_QUERY("MBQ", "GetCDInfo", MBQ_GetCDInfo);
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

  MB_QUERY("MBQ", "SubmitTrack", MBQ_SubmitTrack);
  MB_QUERY("MBQ", "SubmitTrackTRMId", MBQ_SubmitTrackTRMId);
  MB_QUERY("MBQ", "FileInfoLookup", MBQ_FileInfoLookup);
}

void Init_musicbrainz(void) {
  mMB = rb_define_module("MusicBrainz");
  rb_define_const(mMB, "VERSION", rb_str_new2(MB_VERSION));

  /* define id length constants */
  rb_define_const(mMB, "ID_LEN", INT2FIX(MB_ID_LEN));
  rb_define_const(mMB, "MB_ID_LEN", INT2FIX(MB_ID_LEN));
  rb_define_const(mMB, "CDINDEX_ID_LEN", INT2FIX(MB_CDINDEX_ID_LEN));
  rb_define_const(mMB, "MB_CDINDEX_ID_LEN", INT2FIX(MB_CDINDEX_ID_LEN));
  
  define_queries();
  
  /************************************/
  /* define MusicBrainz::Client class */
  /************************************/
  cClient = rb_define_class_under(mMB, "Client", rb_cObject);
  rb_define_singleton_method(cClient, "new", mb_client_new, 0);
  rb_define_singleton_method(cClient, "initialize", mb_client_init, 0);

  rb_define_method(cClient, "version", mb_client_version, 0);
  rb_define_alias(cClient, "get_version", "version");
  
  rb_define_method(cClient, "server=", mb_client_set_server, -1);
  rb_define_alias(cClient, "set_server", "server=");

  rb_define_method(cClient, "debug=", mb_client_set_debug, 1);

  rb_define_method(cClient, "proxy=", mb_client_set_proxy, -1);
  rb_define_alias(cClient, "set_proxy", "proxy=");

  rb_define_method(cClient, "auth", mb_client_auth, 2);
  rb_define_alias(cClient, "authenticate", "auth");

  rb_define_method(cClient, "device=", mb_client_set_device, -1);
  rb_define_alias(cClient, "set_device", "device=");

  rb_define_method(cClient, "utf8=", mb_client_set_use_utf8, -1);
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

  /*rb_define_method(cClient, "result_int", mb_client_result_int, -1);
  rb_define_alias(cClient, "get_result_int", "result_int");*/

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

  rb_define_method(cClient, "sha1", mb_client_sha1, 1);
  rb_define_alias(cClient, "calculate_sha1", "sha1");

  rb_define_method(cClient, "bitprint", mb_client_bitprint, 1);
  rb_define_alias(cClient, "calculate_bitprint", "bitprint");

  rb_define_method(cClient, "mp3_info", mb_client_mp3_info, 1);
  rb_define_alias(cClient, "get_mp3_info", "mp3_info");

  rb_define_method(cClient, "launch", mb_client_launch, 2);
  rb_define_alias(cClient, "browser", "launch");
  rb_define_alias(cClient, "launch_browser", "launch");
  
  /********************/
  /* define TRM class */
  /********************/
  cTRM = rb_define_class_under(mMB, "TRM", rb_cObject);
  rb_define_singleton_method(cTRM, "new", mb_trm_new, 0);
  rb_define_singleton_method(cTRM, "initialize", mb_trm_init, 0);

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
