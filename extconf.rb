require 'mkmf'

$LD_FLAGS = "-lstdc++ -lm"

have_func('pow', 'math.h') and
# note, this causes problems in cygwin.  any suggestions?
have_library('stdc++', '__cxa_rethrow') and
have_library('musicbrainz', 'mb_Query') and
create_makefile('musicbrainz')
