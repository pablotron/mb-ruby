require 'rubygems'

spec = Gem::Specification.new do |s|

  #### Basic information.

  s.name = 'MB-Ruby'
  s.version = '0.1.0'
  s.summary = <<-EOF
    MusicBrainz bindings for Ruby.
  EOF
  s.description = <<-EOF
    MusicBrainz bindings for Ruby.
  EOF

  s.requirements << 'MusicBrainz, version 2.0.0 (or newer)'
  s.requirements << 'Ruby, version 1.6.7 (or newer)'

  #### Which files are to be included in this gem?  Everything!  (Except CVS directories.)

  s.files = Dir.glob("**/*").delete_if { |item| item.include?("CVS") }

  #### C code extensions.

  s.require_path = 'lib' # is this correct?
  s.extensions << "extconf.rb"

  #### Load-time details: library and application (you will need one or both).
  s.autorequire = 'fam'
  s.has_rdoc = true
  s.rdoc_options = ['musicbrainz.c', 'README', 'ChangeLog', 'AUTHORS', 'COPYING', 'TODO']

  #### Author and project details.

  s.author = 'Paul Duncan'
  s.email = 'pabs@pablotron.org'
  s.homepage = 'http://www.pablotron.org/software/mb-ruby/'
  s.rubyforge_project = 'mb-ruby'
end
