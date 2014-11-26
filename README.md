kqueue
===

A binding of kqueue on Ruby.

**kqueue** can use BSD system only. (because must be installed sys/event.h)

# Usage

```ruby
require 'kqueue'

# Inheritance
# class Kqueue < IO
#   # ident: file descriptor identifier for this event
#   # filter: filter for event(just one)
#   # flags: general flags(complex fragment)
#   # fflags: filter-specific flags(complex fragment)
#   # data: filter-specific data
#   # udata: opaque user data(can set any object)
#   class Event < Struct.new(:ident, :filter, :flags, :fflags, :data, :udata)
#     # Event::Constants include all kqueue constants
#     include Event::Constants
#   end
# end

# create kqueue object and ensure close
Kqueue.open do |kq|
  # ...
end

# or
begin
  kq = Kqueue.new
  # ...
ensure
  kq.close
end

# watch event
IO.pipe do |r, w|
  Kqueue.open do |kq|
    kev = Kqueue::Event.new(r.fileno, EVFILT_READ, EV_ADD, 0, 0, proc {
      puts "you can read #{r.fileno} with non block!"
    })
    # set a event
    kq.kevent([kev], 0)

    # wait a event
    # wait until event occur
    kq.kevent(nil, 1)

    # wait 0.1 sec
    kq.kevent(nil, 1, 0.1) #=> []

    w.write 'ok go'
    w.flush

    # get a notified event
    kevs = kq.kevent(nil, 1) #=> [#<a Event object>]

    # udata have one any object
    kevs.first.udata.call #=> you can read 7 with non block!
  end
end
```

## filter

filter|description
---|---
EVFILT_READ|on read
EVFILT_WRITE|on write
EVFILT_AIO|attached to aio requests
EVFILT_VNODE|attached to vnodes
EVFILT_PROC|attached to struct proc
EVFILT_SIGNAL|attached to signal
EVFILT_TIMER|timers
EVFILT_MACHPORT|Mach portsets

## flags

flags|description
---|---
EV_ADD|add event to kq (implies enable)
EV_DELETE|delete event from kq
EV_ENABLE|enable event
EV_DISABLE|disable event (not reported)
EV_ONESHOT|only report one occurrence
EV_CLEAR|clear event state after reporting
EV_RECEIPT|force EV_ERROR on success, data == 0
EV_ERROR|error, data contains errno(returned value)
EV_EOF|EOF detected(returned value)

# fflags

fflags|description
---|---
NOTE_DELETE|vnode was removed
NOTE_WRITE|data contents changed
NOTE_EXTEND|size increased
NOTE_ATTRIB|attributes changed
NOTE_LINK|link count changed
NOTE_RENAME|vnode was renamed
NOTE_REVOKE|vnode access was revoked
NOTE_EXIT|process exited
NOTE_EXITSTATUS|exit status to be returned, valid for child process only
NOTE_FORK|process forked
NOTE_EXEC|process exec'd
NOTE_SIGNAL|shared with EVFILT_SIGNAL
NOTE_REAP|process reaped

## Installation

Add this line to your application's Gemfile:

    gem 'kqueue'

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install kqueue

# Pro Tips

- Support call without GVL in CRuby (use rb\_thread\_call\_without\_gvl())

# Fork Me !

This is experimental implementation.
I'm waiting for your idea and Pull Request !

# see also

- man kqueue
- http://people.freebsd.org/~jlemon/papers/kqueue_freenix.pdf
- https://www.freebsd.org/cgi/man.cgi?query=kqueue
- your /usr/include/sys/event.h
