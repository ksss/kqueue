require 'test/unit'
require 'timeout'
require 'tempfile'
require 'kqueue'

class TestIOKqueue < Test::Unit::TestCase
  include Kqueue::Event::Constants

  def test_initalize
    kq = Kqueue.new
    assert_instance_of(Kqueue, kq)
    assert_kind_of(IO, kq)
    assert { true == kq.close_on_exec? }
    kq.close

    Kqueue.open do |kq|
      assert_instance_of(Kqueue, kq)
      assert_instance_of(Kqueue::Event, Kqueue::Event.new)
    end
  end

  def test_kevent
    Tempfile.open("test") do |f|
      Kqueue.open do |kq|
        kev_r = Kqueue::Event.new(f.fileno, EVFILT_READ, EV_ADD, 0, 0, nil)
        kev_w = Kqueue::Event.new(f.fileno, EVFILT_WRITE, EV_ADD, 0, 0, nil)
        assert { [] == kq.kevent([kev_r, kev_w], 0) }
        assert { [] == kq.kevent(nil, 0) }
        assert { [Kqueue::Event.new(f.fileno, EVFILT_WRITE, EV_ADD, 0, 1, nil)] == kq.kevent(nil, 2) }
        assert { [Kqueue::Event.new(f.fileno, EVFILT_WRITE, EV_ADD, 0, 1, nil)] == kq.kevent(nil, 2) }
        message = 'hello world'
        f.write message
        f.rewind

        expect = [
          Kqueue::Event.new(f.fileno, EVFILT_WRITE, EV_ADD, 0, 1, nil),
          Kqueue::Event.new(f.fileno, EVFILT_READ, EV_ADD, 0, message.length, nil)
        ]
        assert { expect == kq.kevent(nil, 2) }
        f.close
        assert { [] == kq.kevent(nil, 2, 0) }
        assert_raise(ArgumentError) { kq.kevent(nil, -1) }
      end
    end
  end

  def test_kevent_delete
    IO.pipe do |r, w|
      Kqueue.open do |kq|
        kev_w = Kqueue::Event.new(w.fileno, EVFILT_WRITE, EV_ADD, 0, 0, nil)
        kq.kevent([kev_w], 0)
        assert { [Kqueue::Event.new(w.fileno, EVFILT_WRITE, EV_ADD, 0, EV_ERROR, nil)] == kq.kevent(nil, 2, 0) }

        kev_del = Kqueue::Event.new(w.fileno, EVFILT_WRITE, EV_DELETE, 0, 0, nil)
        kq.kevent([kev_del], 0)
        GC.start
        assert { [] == kq.kevent(nil, 2, 0) }

        kev_del = Kqueue::Event.new(w.fileno, EVFILT_WRITE, EV_ADD|EV_DELETE, 0, 0, nil)
        assert_raise(ArgumentError) { kq.kevent([kev_del], 0) }

        kev_del = Kqueue::Event.new(w.fileno, EVFILT_READ, EV_DELETE, 0, 0, nil)
        assert_raise(ArgumentError) { kq.kevent([kev_del], 0) }
      end
    end
  end

  def test_kevent_have_udata
    IO.pipe do |r, w|
      Kqueue.open do |kq|
        obj = Object.new
        object_id = obj.object_id
        kev_w = Kqueue::Event.new(w.fileno, EVFILT_WRITE, EV_ADD, 0, 0, obj)
        ret = kq.kevent([kev_w], 1)
        GC.start
        assert { object_id == ret.first.udata.object_id }
      end
    end

    IO.pipe do |r, w|
      Kqueue.open do |kq|
        str = "in_string"
        ary = [:in_array]
        prc = Proc.new{ :in_proc }
        kev_w = Kqueue::Event.new(w.fileno, EVFILT_WRITE, EV_ADD, 0, 0, [str, ary, prc])
        ret = kq.kevent([kev_w], 1)
        GC.start
        assert { [Kqueue::Event.new(w.fileno, EVFILT_WRITE, EV_ADD, 0, EV_ERROR, [str, ary, prc])] == ret }
        assert { "in_string" == ret.first.udata[0] }
        assert { [:in_array] == ret.first.udata[1] }
        assert { prc == ret.first.udata[2] }
      end
    end
  end

  def test_kevent_timeout
    IO.pipe do |r, w|
      Kqueue.open do |kq|
        kev_r = Kqueue::Event.new(r.fileno, EVFILT_READ, EV_ADD, 0, 0, nil)
        assert { [] == kq.kevent([kev_r], 1, 0) }
        assert { [] == kq.kevent([kev_r], 1, 0.01) }
        assert_raise(Timeout::Error) do
          timeout(0.01) { kq.kevent(nil, 1, 0.02) }
        end
        assert_raise(Errno::EINVAL) do
          kq.kevent(nil, 0, -1)
        end
      end
    end
  end

  def test_kevent_with_thread
    IO.pipe do |r, w|
      Kqueue.open do |kq|
        message = 'ok go'
        kev_r = Kqueue::Event.new(r.fileno, EVFILT_READ, EV_ADD, 0, 0, nil)
        w.write message
        w.close
        GC.start
        t = Thread.new {
          kq.kevent([kev_r], 1)
        }
        assert { [Kqueue::Event.new(r.fileno, EVFILT_READ, EV_ADD|EV_EOF, 0, message.length, nil)] == t.value }
      end
    end
  end

  def test_file
    Tempfile.create("test") do |f|
      Kqueue.open do |kq|
        kevs = [
          Kqueue::Event.new(f.fileno, EVFILT_WRITE, EV_ADD|EV_ENABLE|EV_CLEAR, 0, 0, [1]),
          Kqueue::Event.new(f.fileno, EVFILT_READ, EV_ADD|EV_ENABLE|EV_CLEAR, 0, 0, [2]),
          Kqueue::Event.new(f.fileno, EVFILT_WRITE, EV_ADD, 0, 0, [3]),
          Kqueue::Event.new(f.fileno, EVFILT_READ, EV_ADD, 0, 0, [4]),
        ]
        kq.kevent(kevs, 0)

        GC.start
        assert { [[3]] == kq.kevent(nil, 4).map(&:udata) }

        f.write 'ok'
        f.rewind
        GC.start
        assert { [[4], [3]] == kq.kevent(nil, 4).map(&:udata) }
      end
    end
  end
end
