require 'mkmf'

$LD_FLAGS = "-lstdc++ -lm"

have_library('m', 'pow') and
have_library('stdc++', '__cxa_rethrow') and
have_library('musicbrainz', 'mb_Query') and
create_makefile('musicbrainz')
