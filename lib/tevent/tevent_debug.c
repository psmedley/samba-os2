/*
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2005

     ** NOTE! The following LGPL license applies to the tevent
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#define TEVENT_DEPRECATED
#include "tevent.h"
#include "tevent_internal.h"

#undef tevent_thread_call_depth_reset_from_req

/********************************************************************
 * Debug wrapper functions, modeled (with lot's of code copied as is)
 * after the ev debug wrapper functions
 ********************************************************************/

/*
  this allows the user to choose their own debug function
*/
int tevent_set_debug(struct tevent_context *ev,
		     void (*debug)(void *context,
				   enum tevent_debug_level level,
				   const char *fmt,
				   va_list ap) PRINTF_ATTRIBUTE(3,0),
		     void *context)
{
	if (ev->wrapper.glue != NULL) {
		ev = tevent_wrapper_main_ev(ev);
		tevent_abort(ev, "tevent_set_debug() on wrapper");
		errno = EINVAL;
		return -1;
	}
	if (debug != NULL) {
		/*
		 * tevent_set_max_debug_level(ev, TEVENT_DEBUG_TRACE)
		 * can be used to get full tracing, but we can to
		 * avoid overhead by default.
		 */
		ev->debug_ops.max_level = TEVENT_DEBUG_WARNING;
	} else {
		ev->debug_ops.max_level = TEVENT_DEBUG_FATAL;
	}
	ev->debug_ops.debug = debug;
	ev->debug_ops.context = context;
	return 0;
}

enum tevent_debug_level
tevent_set_max_debug_level(struct tevent_context *ev,
			   enum tevent_debug_level max_level)
{
	enum tevent_debug_level old_level;
	old_level = ev->debug_ops.max_level;
	ev->debug_ops.max_level = max_level;
	return old_level;
}

/*
  debug function for ev_set_debug_stderr
*/
static void tevent_debug_stderr(void *private_data,
				enum tevent_debug_level level,
				const char *fmt,
				va_list ap) PRINTF_ATTRIBUTE(3,0);
static void tevent_debug_stderr(void *private_data,
				enum tevent_debug_level level,
				const char *fmt, va_list ap)
{
	if (level <= TEVENT_DEBUG_WARNING) {
		vfprintf(stderr, fmt, ap);
	}
}

/*
  convenience function to setup debug messages on stderr
  messages of level TEVENT_DEBUG_WARNING and higher are printed
*/
int tevent_set_debug_stderr(struct tevent_context *ev)
{
	return tevent_set_debug(ev, tevent_debug_stderr, ev);
}

/*
 * log a message
 *
 * The default debug action is to ignore debugging messages.
 * This is the most appropriate action for a library.
 * Applications using the library must decide where to
 * redirect debugging messages
*/
void tevent_debug(struct tevent_context *ev, enum tevent_debug_level level,
		  const char *fmt, ...)
{
	va_list ap;
	if (!ev) {
		return;
	}
	if (ev->wrapper.glue != NULL) {
		ev = tevent_wrapper_main_ev(ev);
	}
	if (level > ev->debug_ops.max_level) {
		return;
	}
	if (ev->debug_ops.debug == NULL) {
		return;
	}
	va_start(ap, fmt);
	ev->debug_ops.debug(ev->debug_ops.context, level, fmt, ap);
	va_end(ap);
}

void tevent_set_trace_callback(struct tevent_context *ev,
			       tevent_trace_callback_t cb,
			       void *private_data)
{
	if (ev->wrapper.glue != NULL) {
		ev = tevent_wrapper_main_ev(ev);
		tevent_abort(ev, "tevent_set_trace_callback() on wrapper");
		return;
	}

	ev->tracing.point.callback = cb;
	ev->tracing.point.private_data = private_data;
}

void tevent_get_trace_callback(struct tevent_context *ev,
			       tevent_trace_callback_t *cb,
			       void *private_data)
{
	*cb = ev->tracing.point.callback;
	*(void**)private_data = ev->tracing.point.private_data;
}

void tevent_trace_point_callback(struct tevent_context *ev,
				 enum tevent_trace_point tp)
{
	if (ev->tracing.point.callback != NULL) {
		ev->tracing.point.callback(tp, ev->tracing.point.private_data);
	}
}

void tevent_set_trace_fd_callback(struct tevent_context *ev,
				  tevent_trace_fd_callback_t cb,
				  void *private_data)
{
	if (ev->wrapper.glue != NULL) {
		ev = tevent_wrapper_main_ev(ev);
		tevent_abort(ev, "tevent_set_trace_fd_callback() on wrapper");
		return;
	}

	ev->tracing.fde.callback = cb;
	ev->tracing.fde.private_data = private_data;
}

void tevent_get_trace_fd_callback(struct tevent_context *ev,
				  tevent_trace_fd_callback_t *cb,
				  void *p_private_data)
{
	*cb = ev->tracing.fde.callback;
	*(void**)p_private_data = ev->tracing.fde.private_data;
}

void tevent_trace_fd_callback(struct tevent_context *ev,
			      struct tevent_fd *fde,
			      enum tevent_event_trace_point tp)
{
	if (ev->tracing.fde.callback != NULL) {
		ev->tracing.fde.callback(fde, tp, ev->tracing.fde.private_data);
	}
}

void tevent_set_trace_signal_callback(struct tevent_context *ev,
				      tevent_trace_signal_callback_t cb,
				      void *private_data)
{
	if (ev->wrapper.glue != NULL) {
		ev = tevent_wrapper_main_ev(ev);
		tevent_abort(ev, "tevent_set_trace_signal_callback() "
			     "on wrapper");
		return;
	}

	ev->tracing.se.callback = cb;
	ev->tracing.se.private_data = private_data;
}

void tevent_get_trace_signal_callback(struct tevent_context *ev,
				      tevent_trace_signal_callback_t *cb,
				      void *p_private_data)
{
	*cb = ev->tracing.se.callback;
	*(void**)p_private_data = ev->tracing.se.private_data;
}

void tevent_trace_signal_callback(struct tevent_context *ev,
				  struct tevent_signal *se,
				  enum tevent_event_trace_point tp)
{
	if (ev->tracing.se.callback != NULL) {
		ev->tracing.se.callback(se, tp, ev->tracing.se.private_data);
	}
}

void tevent_set_trace_timer_callback(struct tevent_context *ev,
				     tevent_trace_timer_callback_t cb,
				     void *private_data)
{
	if (ev->wrapper.glue != NULL) {
		ev = tevent_wrapper_main_ev(ev);
		tevent_abort(ev, "tevent_set_trace_timer_callback() "
			     "on wrapper");
		return;
	}

	ev->tracing.te.callback = cb;
	ev->tracing.te.private_data = private_data;
}

void tevent_get_trace_timer_callback(struct tevent_context *ev,
				     tevent_trace_timer_callback_t *cb,
				     void *p_private_data)
{
	*cb = ev->tracing.te.callback;
	*(void**)p_private_data = ev->tracing.te.private_data;
}

void tevent_trace_timer_callback(struct tevent_context *ev,
				 struct tevent_timer *te,
				 enum tevent_event_trace_point tp)
{
	if (ev->tracing.te.callback != NULL) {
		ev->tracing.te.callback(te, tp, ev->tracing.te.private_data);
	}
}

void tevent_set_trace_immediate_callback(struct tevent_context *ev,
					 tevent_trace_immediate_callback_t cb,
					 void *private_data)
{
	if (ev->wrapper.glue != NULL) {
		ev = tevent_wrapper_main_ev(ev);
		tevent_abort(ev, "tevent_set_trace_immediate_callback() "
		             "on wrapper");
		return;
	}

	ev->tracing.im.callback = cb;
	ev->tracing.im.private_data = private_data;
}

void tevent_get_trace_immediate_callback(struct tevent_context *ev,
					 tevent_trace_immediate_callback_t *cb,
					 void *p_private_data)
{
	*cb = ev->tracing.im.callback;
	*(void**)p_private_data = ev->tracing.im.private_data;
}

void tevent_trace_immediate_callback(struct tevent_context *ev,
				     struct tevent_immediate *im,
				     enum tevent_event_trace_point tp)
{
	if (ev->tracing.im.callback != NULL) {
		ev->tracing.im.callback(im, tp, ev->tracing.im.private_data);
	}
}

void tevent_set_trace_queue_callback(struct tevent_context *ev,
				     tevent_trace_queue_callback_t cb,
				     void *private_data)
{
	if (ev->wrapper.glue != NULL) {
		ev = tevent_wrapper_main_ev(ev);
		tevent_abort(ev, "tevent_set_trace_queue_callback() "
		             "on wrapper");
		return;
	}

	ev->tracing.qe.callback = cb;
	ev->tracing.qe.private_data = private_data;
}

void tevent_get_trace_queue_callback(struct tevent_context *ev,
				     tevent_trace_queue_callback_t *cb,
				     void *p_private_data)
{
	*cb = ev->tracing.qe.callback;
	*(void**)p_private_data = ev->tracing.qe.private_data;
}

void tevent_trace_queue_callback(struct tevent_context *ev,
				 struct tevent_queue_entry *qe,
				 enum tevent_event_trace_point tp)
{
	if (ev->tracing.qe.callback != NULL) {
		ev->tracing.qe.callback(qe, tp, ev->tracing.qe.private_data);
	}
}

_PRIVATE_ __thread
struct tevent_thread_call_depth_state tevent_thread_call_depth_state_g;

void tevent_thread_call_depth_activate(size_t *ptr)
{
}

void tevent_thread_call_depth_deactivate(void)
{
}

void tevent_thread_call_depth_start(struct tevent_req *req)
{
}

void tevent_thread_call_depth_reset_from_req(struct tevent_req *req)
{
	_tevent_thread_call_depth_reset_from_req(req, NULL);
}

void _tevent_thread_call_depth_reset_from_req(struct tevent_req *req,
					     const char *fname)
{
	if (tevent_thread_call_depth_state_g.cb != NULL) {
		tevent_thread_call_depth_state_g.cb(
			tevent_thread_call_depth_state_g.cb_private,
			TEVENT_CALL_FLOW_REQ_RESET,
			req,
			req->internal.call_depth,
			fname);
	}
}

void tevent_thread_call_depth_set_callback(tevent_call_depth_callback_t f,
					   void *private_data)
{
	/* In case of deactivation, make sure that call depth is set to 0 */
	if (tevent_thread_call_depth_state_g.cb != NULL) {
		tevent_thread_call_depth_state_g.cb(
			tevent_thread_call_depth_state_g.cb_private,
			TEVENT_CALL_FLOW_REQ_RESET,
			NULL,
			0,
			"tevent_thread_call_depth_set_callback");
	}
	tevent_thread_call_depth_state_g = (struct tevent_thread_call_depth_state)
	{
		.cb = f,
		.cb_private = private_data,
	};
}
