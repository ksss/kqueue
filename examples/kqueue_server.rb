#! /usr/bin/env ruby

require 'kqueue'
require 'socket'

class KqServer
  include Kqueue::Event::Constants

  def self.start(port)
    kqs = new(port)
    yield kqs
    loop do
      kqs.wait.each do |ev|
        ev.udata && ev.udata[1].call(ev.udata[0])
      end
    end
  end

  def initialize(port)
    @kq = Kqueue.new
    @server = TCPServer.open(port)
    set_event @server, EVFILT_READ do
      socket = @server.accept
      set_event socket, EVFILT_READ do |socket|
        loop do
          chunk = socket.recv(1024)
          @on_data && @on_data.call(chunk)
          break if chunk.length < 4 || chunk[-4..-1] == "\r\n\r\n"
        end
        set_event socket, EVFILT_WRITE do |socket|
          @on_end && @on_end.call(socket)
          del_event(socket, EVFILT_READ)
          del_event(socket, EVFILT_WRITE)
          socket.close
        end
      end
    end
  end

  def set_event(io, filter, &block)
    @kq.kevent([Kqueue::Event.new(io.fileno, filter, EV_ADD, 0, 0, [io, block])], 0)
  end

  def del_event(io, filter)
    @kq.kevent([Kqueue::Event.new(io.fileno, filter, EV_DELETE, 0, 0, nil)], 0)
  end

  def wait
    @kq.kevent(nil, 128, 5)
  end

  def on_data(&block)
    @on_data = block
  end

  def on_end(&block)
    @on_end = block
  end
end

puts "run http://127.0.0.1:4000/"

KqServer.start(4000) do |s|
  s.on_data do |chunk|
    print chunk
  end
  s.on_end do |socket|
    socket.write [
      "HTTP/1.0 200 OK\r\n",
      "Content-Length: 11\r\n",
      "Content-Type: text/html\r\n",
      "\r\n",
      "Hello World\r\n",
    ].join("")
  end
end
