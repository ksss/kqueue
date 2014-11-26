require 'mkmf'

headers = %w(sys/types.h sys/event.h sys/time.h)

unless headers.all?{|h| have_header(h)}
  puts "[31m*** complie error: gem 'kqueue' must be installed #{headers.to_s}. ***[m"
  puts "[31m*** you can require 'kqueue'. But, you can not use Kqueue APIs. ***[m"
end

create_makefile('kqueue')
