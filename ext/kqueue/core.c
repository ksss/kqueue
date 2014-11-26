#include "ruby.h"
#include "ruby/io.h"
#include "ruby/thread.h"

#if defined(HAVE_SYS_TYPES_H) && defined(HAVE_SYS_EVENT_H) && defined(HAVE_SYS_TIME_H)

#include <fcntl.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

VALUE cKqueue;
VALUE mKqueue_Event_Constants;
VALUE cKqueue_Event;
ID id_udata_marks;

#define IVAR_GET(self) rb_ivar_get((self), id_udata_marks)
#define IVAR_SET(self, obj) rb_ivar_set((self), id_udata_marks, (obj))

enum enumEvents {
  IDENT,
  FILTER,
  FLAGS,
  FFLAGS,
  DATA,
  UDATA
};

static VALUE
rb_kqueue_initialize(VALUE self)
{
  rb_io_t *fp;
  int fd;

  fd = kqueue();
  if (fd == -1)
    rb_sys_fail("kqueue");
  rb_update_max_fd(fd);

  MakeOpenFile(self, fp);
  fp->fd = fd;
  fp->mode = FMODE_READABLE|FMODE_BINMODE;
  rb_io_ascii8bit_binmode(self);

  IVAR_SET(self, rb_hash_new());

  return self;
}

struct kqueue_kevent_args {
  int fd;
  struct kevent *chl;
  int ch_len;
  struct kevent *evl;
  int ev_len;
  struct timespec *ts;
};

static void *
rb_kqueue_kevent_func(void *ptr)
{
  const struct kqueue_kevent_args *args = ptr;
  return (void*)(long)kevent(args->fd, args->chl, args->ch_len, args->evl, args->ev_len, args->ts);
}

static VALUE
rb_kqueue_kevent(int argc, VALUE *argv, VALUE self)
{
  VALUE ch_list, max_events, timeout, ev, ready_evlist;
  struct kqueue_kevent_args args;
  struct timespec ts;
  struct timespec *pts = NULL;
  struct timeval tv;
  int i;
  int ev_len = 0, ch_len = 0;
  int ready;
  struct kevent *chl = NULL, *evl = NULL;
  rb_io_t *fptr, *fptr_io;

  fptr = RFILE(self)->fptr;
  rb_io_check_initialized(fptr);

  rb_scan_args(argc, argv, "21", &ch_list, &max_events, &timeout);
  if (!NIL_P(max_events)) {
    ev_len = FIX2INT(max_events);
  }

  if (!NIL_P(timeout)) {
    tv = rb_time_timeval(timeout);
    ts.tv_sec = (time_t)tv.tv_sec;
    ts.tv_nsec = (long)tv.tv_usec * 1000;
    pts = &ts;
  }

  if (!NIL_P(ch_list)) {
    Check_Type(ch_list, T_ARRAY);
    ch_len = RARRAY_LEN(ch_list);

    chl = alloca(sizeof(struct kevent) * ch_len);
    for (i = 0; i < ch_len; i++) {
      ev = RARRAY_AREF(ch_list, i);
      if (!rb_obj_is_kind_of(ev, cKqueue_Event)) {
        rb_raise(rb_eTypeError, "must be set Array of Event object");
      }

      chl[i].ident = FIX2INT(RSTRUCT_GET(ev, IDENT));
      chl[i].filter = FIX2INT(RSTRUCT_GET(ev, FILTER));
      chl[i].flags = FIX2UINT(RSTRUCT_GET(ev, FLAGS));
      chl[i].fflags = FIX2UINT(RSTRUCT_GET(ev, FFLAGS));
      chl[i].data = NUM2LONG(RSTRUCT_GET(ev, DATA));
      {
        VALUE fileno = RSTRUCT_GET(ev, IDENT);
        VALUE filter = RSTRUCT_GET(ev, FILTER);
        VALUE udata = RSTRUCT_GET(ev, UDATA);
        VALUE ivar = IVAR_GET(self);
        VALUE h = rb_hash_aref(ivar, fileno);
        if ((chl[i].flags & EV_DELETE) == EV_DELETE) {
          if (chl[i].flags & ~EV_DELETE) {
            rb_raise(rb_eArgError, "EV_DELETE cannot set with other");
          }
          if (NIL_P(h)) {
            rb_raise(rb_eArgError, "delete udata not found");
          }
          rb_hash_delete(h, filter);
          if (RHASH_EMPTY_P(h)) {
            rb_hash_delete(ivar, fileno);
          }
          udata = (VALUE)0;
        }
        else {
          if (NIL_P(h)) {
            h = rb_hash_aset(ivar, fileno, rb_hash_new());
          }
          rb_hash_aset(h, filter, udata);
        }
        IVAR_SET(self, ivar);
        chl[i].udata = (void*)udata;
      }
    }
  }

  if (0 < ev_len) {
    evl = alloca(sizeof(struct kevent) * ev_len);
  }
  else if (ev_len < 0) {
    rb_raise(rb_eArgError, "negative size");
  }

  args.fd = fptr->fd;
  args.chl = chl;
  args.ch_len = ch_len;
  args.evl = evl;
  args.ev_len = ev_len;
  args.ts = pts;

RETRY:
  ready = (int)(long)rb_thread_call_without_gvl(rb_kqueue_kevent_func, &args, RUBY_UBF_IO, 0);
  if (ready == -1) {
    if (errno == EINTR)
      goto RETRY;
    else
      rb_sys_fail("kevent");
  }

  ready_evlist = rb_ary_new_capa(ready);
  for (i = 0; i < ready; i++) {
    ev = rb_obj_alloc(cKqueue_Event);
    RSTRUCT_SET(ev, IDENT, INT2FIX(evl[i].ident));
    RSTRUCT_SET(ev, FILTER, INT2FIX(evl[i].filter));
    RSTRUCT_SET(ev, FLAGS, INT2FIX(evl[i].flags));
    RSTRUCT_SET(ev, FFLAGS, INT2FIX(evl[i].fflags));
    RSTRUCT_SET(ev, FLAGS, INT2FIX(evl[i].flags));
    RSTRUCT_SET(ev, DATA, INT2FIX(evl[i].data));
    RSTRUCT_SET(ev, UDATA, (VALUE)evl[i].udata);
    rb_ary_store(ready_evlist, i, ev);
  }
  return ready_evlist;
}
#endif

void
Init_kqueue()
{
#if defined(HAVE_SYS_TYPES_H) && defined(HAVE_SYS_EVENT_H) && defined(HAVE_SYS_TIME_H)

  id_udata_marks = rb_intern("@udata_marks");

  cKqueue = rb_define_class("Kqueue", rb_cIO);
  rb_define_method(cKqueue, "initialize", rb_kqueue_initialize, 0);
  rb_define_method(cKqueue, "kevent", rb_kqueue_kevent, -1);

  cKqueue_Event = rb_struct_define_under(cKqueue, "Event", "ident", "filter", "flags", "fflags", "data", "udata", NULL);

  mKqueue_Event_Constants = rb_define_module_under(cKqueue_Event, "Constants");

  rb_include_module(cKqueue_Event, mKqueue_Event_Constants);

  /* actions */
  rb_define_const(mKqueue_Event_Constants, "EV_ADD", INT2FIX((u_short)EV_ADD));
  rb_define_const(mKqueue_Event_Constants, "EV_ENABLE", INT2FIX((u_short)EV_ENABLE));
  rb_define_const(mKqueue_Event_Constants, "EV_DISABLE", INT2FIX((u_short)EV_DISABLE));
  rb_define_const(mKqueue_Event_Constants, "EV_DELETE", INT2FIX((u_short)EV_DELETE));
  rb_define_const(mKqueue_Event_Constants, "EV_RECEIPT", INT2FIX((u_short)EV_RECEIPT));
  /* flags */
  rb_define_const(mKqueue_Event_Constants, "EV_ONESHOT", INT2FIX((u_short)EV_ONESHOT));
  rb_define_const(mKqueue_Event_Constants, "EV_CLEAR", INT2FIX((u_short)EV_CLEAR));
  /* returned values */
  rb_define_const(mKqueue_Event_Constants, "EV_EOF", INT2FIX((u_short)EV_EOF));
  rb_define_const(mKqueue_Event_Constants, "EV_ERROR", INT2FIX((u_short)EV_ERROR));

  /* for filter */
  rb_define_const(mKqueue_Event_Constants, "EVFILT_READ", INT2FIX((short)EVFILT_READ));
  rb_define_const(mKqueue_Event_Constants, "EVFILT_WRITE", INT2FIX((short)EVFILT_WRITE));
  rb_define_const(mKqueue_Event_Constants, "EVFILT_AIO", INT2FIX((short)EVFILT_AIO));
  rb_define_const(mKqueue_Event_Constants, "EVFILT_VNODE", INT2FIX((short)EVFILT_VNODE));
  rb_define_const(mKqueue_Event_Constants, "EVFILT_PROC", INT2FIX((short)EVFILT_PROC));
  rb_define_const(mKqueue_Event_Constants, "EVFILT_SIGNAL", INT2FIX((short)EVFILT_SIGNAL));
  rb_define_const(mKqueue_Event_Constants, "EVFILT_MACHPORT", INT2FIX((short)EVFILT_MACHPORT));

  // for EVFILT_VNODE
  rb_define_const(mKqueue_Event_Constants, "NOTE_DELETE", INT2FIX((u_int)NOTE_DELETE));
  rb_define_const(mKqueue_Event_Constants, "NOTE_WRITE", INT2FIX((u_int)NOTE_WRITE));
  rb_define_const(mKqueue_Event_Constants, "NOTE_EXTEND", INT2FIX((u_int)NOTE_EXTEND));
  rb_define_const(mKqueue_Event_Constants, "NOTE_ATTRIB", INT2FIX((u_int)NOTE_ATTRIB));
  rb_define_const(mKqueue_Event_Constants, "NOTE_LINK", INT2FIX((u_int)NOTE_LINK));
  rb_define_const(mKqueue_Event_Constants, "NOTE_RENAME", INT2FIX((u_int)NOTE_RENAME));
  rb_define_const(mKqueue_Event_Constants, "NOTE_REVOKE", INT2FIX((u_int)NOTE_REVOKE));

  // for EVFILT_PROC
  rb_define_const(mKqueue_Event_Constants, "NOTE_EXIT", INT2FIX((u_int)NOTE_EXIT));
  rb_define_const(mKqueue_Event_Constants, "NOTE_EXITSTATUS", INT2FIX((u_int)NOTE_EXITSTATUS));
  rb_define_const(mKqueue_Event_Constants, "NOTE_FORK", INT2FIX((u_int)NOTE_FORK));
  rb_define_const(mKqueue_Event_Constants, "NOTE_EXEC", INT2FIX((u_int)NOTE_EXEC));
  rb_define_const(mKqueue_Event_Constants, "NOTE_SIGNAL", INT2FIX((u_int)NOTE_SIGNAL));
  rb_define_const(mKqueue_Event_Constants, "NOTE_REAP", INT2FIX((u_int)NOTE_REAP));
#endif
}
