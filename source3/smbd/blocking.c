/* 
   Unix SMB/CIFS implementation.
   Blocking Locking functions
   Copyright (C) Jeremy Allison 1998-2003

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "smbd/globals.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

/****************************************************************************
 Determine if this is a secondary element of a chained SMB.
  **************************************************************************/

static void received_unlock_msg(struct messaging_context *msg,
				void *private_data,
				uint32_t msg_type,
				struct server_id server_id,
				DATA_BLOB *data);

static void brl_timeout_fn(struct event_context *event_ctx,
			   struct timed_event *te,
			   struct timeval now,
			   void *private_data)
{
	SMB_ASSERT(brl_timeout == te);
	TALLOC_FREE(brl_timeout);

	change_to_root_user();	/* TODO: Possibly run all timed events as
				 * root */

	process_blocking_lock_queue();
}

/****************************************************************************
 We need a version of timeval_min that treats zero timval as infinite.
****************************************************************************/

static struct timeval timeval_brl_min(const struct timeval *tv1,
					const struct timeval *tv2)
{
	if (timeval_is_zero(tv1)) {
		return *tv2;
	}
	if (timeval_is_zero(tv2)) {
		return *tv1;
	}
	return timeval_min(tv1, tv2);
}

/****************************************************************************
 After a change to blocking_lock_queue, recalculate the timed_event for the
 next processing.
****************************************************************************/

static bool recalc_brl_timeout(void)
{
	struct blocking_lock_record *blr;
	struct timeval next_timeout;

	TALLOC_FREE(brl_timeout);

	next_timeout = timeval_zero();	

	for (blr = blocking_lock_queue; blr; blr = blr->next) {
		if (timeval_is_zero(&blr->expire_time)) {
			/*
			 * If we're blocked on pid 0xFFFFFFFF this is
			 * a POSIX lock, so calculate a timeout of
			 * 10 seconds into the future.
			 */
                        if (blr->blocking_pid == 0xFFFFFFFF) {
				struct timeval psx_to = timeval_current_ofs(10, 0);
				next_timeout = timeval_brl_min(&next_timeout, &psx_to);
                        }

			continue;
		}

		next_timeout = timeval_brl_min(&next_timeout, &blr->expire_time);
	}

	if (timeval_is_zero(&next_timeout)) {
		DEBUG(10, ("Next timeout = Infinite.\n"));
		return True;
	}

	if (DEBUGLVL(10)) {
		struct timeval cur, from_now;

		cur = timeval_current();
		from_now = timeval_until(&cur, &next_timeout);
		DEBUG(10, ("Next timeout = %d.%d seconds from now.\n",
		    (int)from_now.tv_sec, (int)from_now.tv_usec));
	}

	if (!(brl_timeout = event_add_timed(smbd_event_context(), NULL,
					    next_timeout,
					    brl_timeout_fn, NULL))) {
		return False;
	}

	return True;
}


/****************************************************************************
 Function to push a blocking lock request onto the lock queue.
****************************************************************************/

bool push_blocking_lock_request( struct byte_range_lock *br_lck,
		struct smb_request *req,
		files_struct *fsp,
		int lock_timeout,
		int lock_num,
		uint32_t lock_pid,
		enum brl_type lock_type,
		enum brl_flavour lock_flav,
		uint64_t offset,
		uint64_t count,
		uint32_t blocking_pid)
{
	struct blocking_lock_record *blr;
	NTSTATUS status;

	if(req_is_in_chain(req)) {
		DEBUG(0,("push_blocking_lock_request: cannot queue a chained request (currently).\n"));
		return False;
	}

	/*
	 * Now queue an entry on the blocking lock queue. We setup
	 * the expiration time here.
	 */

	blr = talloc(NULL, struct blocking_lock_record);
	if (blr == NULL) {
		DEBUG(0,("push_blocking_lock_request: Malloc fail !\n" ));
		return False;
	}

	blr->next = NULL;
	blr->prev = NULL;

	blr->fsp = fsp;
	if (lock_timeout == -1) {
		blr->expire_time.tv_sec = 0;
		blr->expire_time.tv_usec = 0; /* Never expire. */
	} else {
		blr->expire_time = timeval_current_ofs(lock_timeout/1000,
					(lock_timeout % 1000) * 1000);
	}
	blr->lock_num = lock_num;
	blr->lock_pid = lock_pid;
	blr->blocking_pid = blocking_pid;
	blr->lock_flav = lock_flav;
	blr->lock_type = lock_type;
	blr->offset = offset;
	blr->count = count;
      
	/* Specific brl_lock() implementations can fill this in. */
	blr->blr_private = NULL;

	/* Add a pending lock record for this. */
	status = brl_lock(smbd_messaging_context(),
			br_lck,
			lock_pid,
			procid_self(),
			offset,
			count,
			lock_type == READ_LOCK ? PENDING_READ_LOCK : PENDING_WRITE_LOCK,
			blr->lock_flav,
			lock_timeout ? True : False, /* blocking_lock. */
			NULL,
			blr);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("push_blocking_lock_request: failed to add PENDING_LOCK record.\n"));
		TALLOC_FREE(blr);
		return False;
	}

	SMB_PERFCOUNT_DEFER_OP(&req->pcd, &req->pcd);
	blr->req = talloc_move(blr, &req);

	DLIST_ADD_END(blocking_lock_queue, blr, struct blocking_lock_record *);
	recalc_brl_timeout();

	/* Ensure we'll receive messages when this is unlocked. */
	if (!blocking_lock_unlock_state) {
		messaging_register(smbd_messaging_context(), NULL,
				   MSG_SMB_UNLOCK, received_unlock_msg);
		blocking_lock_unlock_state = true;
	}

	DEBUG(3,("push_blocking_lock_request: lock request blocked with "
		"expiry time (%u sec. %u usec) (+%d msec) for fnum = %d, name = %s\n",
		(unsigned int)blr->expire_time.tv_sec,
		(unsigned int)blr->expire_time.tv_usec, lock_timeout,
		blr->fsp->fnum, blr->fsp->fsp_name ));

	/* Push the MID of this packet on the signing queue. */
	srv_defer_sign_response(blr->req->mid);

	return True;
}

/****************************************************************************
 Return a lockingX success SMB.
*****************************************************************************/

static void reply_lockingX_success(struct blocking_lock_record *blr)
{
	reply_outbuf(blr->req, 2, 0);

	/*
	 * As this message is a lockingX call we must handle
	 * any following chained message correctly.
	 * This is normally handled in construct_reply(),
	 * but as that calls switch_message, we can't use
	 * that here and must set up the chain info manually.
	 */

	chain_reply(blr->req);
	TALLOC_FREE(blr->req->outbuf);
}

/****************************************************************************
 Return a generic lock fail error blocking call.
*****************************************************************************/

static void generic_blocking_lock_error(struct blocking_lock_record *blr, NTSTATUS status)
{
	/* whenever a timeout is given w2k maps LOCK_NOT_GRANTED to
	   FILE_LOCK_CONFLICT! (tridge) */
	if (NT_STATUS_EQUAL(status, NT_STATUS_LOCK_NOT_GRANTED)) {
		status = NT_STATUS_FILE_LOCK_CONFLICT;
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_FILE_LOCK_CONFLICT)) {
		/* Store the last lock error. */
		files_struct *fsp = blr->fsp;

		if (fsp) {
			fsp->last_lock_failure.context.smbpid = blr->lock_pid;
			fsp->last_lock_failure.context.tid = fsp->conn->cnum;
			fsp->last_lock_failure.context.pid = procid_self();
			fsp->last_lock_failure.start = blr->offset;
			fsp->last_lock_failure.size = blr->count;
			fsp->last_lock_failure.fnum = fsp->fnum;
			fsp->last_lock_failure.lock_type = READ_LOCK; /* Don't care. */
			fsp->last_lock_failure.lock_flav = blr->lock_flav;
		}
	}

	reply_nterror(blr->req, status);
	if (!srv_send_smb(smbd_server_fd(), (char *)blr->req->outbuf,
			  blr->req->encrypted, NULL)) {
		exit_server_cleanly("generic_blocking_lock_error: srv_send_smb failed.");
	}
	TALLOC_FREE(blr->req->outbuf);
}

/****************************************************************************
 Return a lock fail error for a lockingX call. Undo all the locks we have 
 obtained first.
*****************************************************************************/

static void reply_lockingX_error(struct blocking_lock_record *blr, NTSTATUS status)
{
	files_struct *fsp = blr->fsp;
	uint16 num_ulocks = SVAL(blr->req->vwv+6, 0);
	uint64_t count = (uint64_t)0, offset = (uint64_t) 0;
	uint32 lock_pid;
	unsigned char locktype = CVAL(blr->req->vwv+3, 0);
	bool large_file_format = (locktype & LOCKING_ANDX_LARGE_FILES);
	uint8_t *data;
	int i;

	data = (uint8_t *)blr->req->buf
		+ ((large_file_format ? 20 : 10)*num_ulocks);

	/* 
	 * Data now points at the beginning of the list
	 * of smb_lkrng structs.
	 */

	/*
	 * Ensure we don't do a remove on the lock that just failed,
	 * as under POSIX rules, if we have a lock already there, we
	 * will delete it (and we shouldn't) .....
	 */

	for(i = blr->lock_num - 1; i >= 0; i--) {
		bool err;

		lock_pid = get_lock_pid( data, i, large_file_format);
		count = get_lock_count( data, i, large_file_format);
		offset = get_lock_offset( data, i, large_file_format, &err);

		/*
		 * We know err cannot be set as if it was the lock
		 * request would never have been queued. JRA.
		 */

		do_unlock(smbd_messaging_context(),
			fsp,
			lock_pid,
			count,
			offset,
			WINDOWS_LOCK);
	}

	generic_blocking_lock_error(blr, status);
}

/****************************************************************************
 Return a lock fail error.
*****************************************************************************/

static void blocking_lock_reply_error(struct blocking_lock_record *blr, NTSTATUS status)
{
	DEBUG(10, ("Replying with error=%s. BLR = %p\n", nt_errstr(status), blr));

	switch(blr->req->cmd) {
	case SMBlockingX:
		reply_lockingX_error(blr, status);
		break;
	case SMBtrans2:
	case SMBtranss2:
		reply_nterror(blr->req, status);

		/*
		 * construct_reply_common has done us the favor to pre-fill
		 * the command field with SMBtranss2 which is wrong :-)
		 */
		SCVAL(blr->req->outbuf,smb_com,SMBtrans2);

		if (!srv_send_smb(smbd_server_fd(),
				  (char *)blr->req->outbuf,
				  IS_CONN_ENCRYPTED(blr->fsp->conn),
				  NULL)) {
			exit_server_cleanly("blocking_lock_reply_error: "
					    "srv_send_smb failed.");
		}
		TALLOC_FREE(blr->req->outbuf);
		break;
	default:
		DEBUG(0,("blocking_lock_reply_error: PANIC - unknown type on blocking lock queue - exiting.!\n"));
		exit_server("PANIC - unknown type on blocking lock queue");
	}
}

/****************************************************************************
 Attempt to finish off getting all pending blocking locks for a lockingX call.
 Returns True if we want to be removed from the list.
*****************************************************************************/

static bool process_lockingX(struct blocking_lock_record *blr)
{
	unsigned char locktype = CVAL(blr->req->vwv+3, 0);
	files_struct *fsp = blr->fsp;
	uint16 num_ulocks = SVAL(blr->req->vwv+6, 0);
	uint16 num_locks = SVAL(blr->req->vwv+7, 0);
	uint64_t count = (uint64_t)0, offset = (uint64_t)0;
	uint32 lock_pid;
	bool large_file_format = (locktype & LOCKING_ANDX_LARGE_FILES);
	uint8_t *data;
	NTSTATUS status = NT_STATUS_OK;

	data = (uint8_t *)blr->req->buf
		+ ((large_file_format ? 20 : 10)*num_ulocks);

	/* 
	 * Data now points at the beginning of the list
	 * of smb_lkrng structs.
	 */

	for(; blr->lock_num < num_locks; blr->lock_num++) {
		struct byte_range_lock *br_lck = NULL;
		bool err;

		lock_pid = get_lock_pid( data, blr->lock_num, large_file_format);
		count = get_lock_count( data, blr->lock_num, large_file_format);
		offset = get_lock_offset( data, blr->lock_num, large_file_format, &err);

		/*
		 * We know err cannot be set as if it was the lock
		 * request would never have been queued. JRA.
		 */
		errno = 0;
		br_lck = do_lock(smbd_messaging_context(),
				fsp,
				lock_pid,
				count,
				offset, 
				((locktype & LOCKING_ANDX_SHARED_LOCK) ?
					READ_LOCK : WRITE_LOCK),
				WINDOWS_LOCK,
				True,
				&status,
				&blr->blocking_pid,
				blr);

		TALLOC_FREE(br_lck);

		if (NT_STATUS_IS_ERR(status)) {
			break;
		}
	}

	if(blr->lock_num == num_locks) {
		/*
		 * Success - we got all the locks.
		 */

		DEBUG(3,("process_lockingX file = %s, fnum=%d type=%d num_locks=%d\n",
			 fsp->fsp_name, fsp->fnum, (unsigned int)locktype, num_locks) );

		reply_lockingX_success(blr);
		return True;
	}

	if (!NT_STATUS_EQUAL(status,NT_STATUS_LOCK_NOT_GRANTED) &&
	    !NT_STATUS_EQUAL(status,NT_STATUS_FILE_LOCK_CONFLICT)) {
		/*
		 * We have other than a "can't get lock"
		 * error. Free any locks we had and return an error.
		 * Return True so we get dequeued.
		 */
		blocking_lock_reply_error(blr, status);
		return True;
	}

	/*
	 * Still can't get all the locks - keep waiting.
	 */

	DEBUG(10,("process_lockingX: only got %d locks of %d needed for file %s, fnum = %d. \
Waiting....\n", 
		  blr->lock_num, num_locks, fsp->fsp_name, fsp->fnum));

	return False;
}

/****************************************************************************
 Attempt to get the posix lock request from a SMBtrans2 call.
 Returns True if we want to be removed from the list.
*****************************************************************************/

static bool process_trans2(struct blocking_lock_record *blr)
{
	char params[2];
	NTSTATUS status;
	struct byte_range_lock *br_lck = do_lock(smbd_messaging_context(),
						blr->fsp,
						blr->lock_pid,
						blr->count,
						blr->offset,
						blr->lock_type,
						blr->lock_flav,
						True,
						&status,
						&blr->blocking_pid,
						blr);
	TALLOC_FREE(br_lck);

	if (!NT_STATUS_IS_OK(status)) {
		if (ERROR_WAS_LOCK_DENIED(status)) {
			/* Still can't get the lock, just keep waiting. */
			return False;
		}	
		/*
		 * We have other than a "can't get lock"
		 * error. Send an error and return True so we get dequeued.
		 */
		blocking_lock_reply_error(blr, status);
		return True;
	}

	/* We finally got the lock, return success. */

	SSVAL(params,0,0);
	/* Fake up max_data_bytes here - we know it fits. */
	send_trans2_replies(blr->fsp->conn, blr->req, params, 2, NULL, 0, 0xffff);
	return True;
}


/****************************************************************************
 Process a blocking lock SMB.
 Returns True if we want to be removed from the list.
*****************************************************************************/

static bool blocking_lock_record_process(struct blocking_lock_record *blr)
{
	switch(blr->req->cmd) {
		case SMBlockingX:
			return process_lockingX(blr);
		case SMBtrans2:
		case SMBtranss2:
			return process_trans2(blr);
		default:
			DEBUG(0,("blocking_lock_record_process: PANIC - unknown type on blocking lock queue - exiting.!\n"));
			exit_server("PANIC - unknown type on blocking lock queue");
	}
	return False; /* Keep compiler happy. */
}

/****************************************************************************
 Cancel entries by fnum from the blocking lock pending queue.
*****************************************************************************/

void cancel_pending_lock_requests_by_fid(files_struct *fsp, struct byte_range_lock *br_lck)
{
	struct blocking_lock_record *blr, *blr_cancelled, *next = NULL;

	for(blr = blocking_lock_queue; blr; blr = next) {
		unsigned char locktype = 0;

		next = blr->next;
		if (blr->fsp->fnum != fsp->fnum) {
			continue;
		}

		if (blr->req->cmd == SMBlockingX) {
			locktype = CVAL(blr->req->vwv+3, 0);
		}

		DEBUG(10, ("remove_pending_lock_requests_by_fid - removing "
			   "request type %d for file %s fnum = %d\n",
			   blr->req->cmd, fsp->fsp_name, fsp->fnum));

		blr_cancelled = blocking_lock_cancel(fsp,
				     blr->lock_pid,
				     blr->offset,
				     blr->count,
				     blr->lock_flav,
				     locktype,
				     NT_STATUS_RANGE_NOT_LOCKED);

		SMB_ASSERT(blr_cancelled == blr);

		brl_lock_cancel(br_lck,
				blr->lock_pid,
				procid_self(),
				blr->offset,
				blr->count,
				blr->lock_flav,
				blr);

		/* We're closing the file fsp here, so ensure
		 * we don't have a dangling pointer. */
		blr->fsp = NULL;
	}
}

/****************************************************************************
 Delete entries by mid from the blocking lock pending queue. Always send reply.
*****************************************************************************/

void remove_pending_lock_requests_by_mid(int mid)
{
	struct blocking_lock_record *blr, *next = NULL;

	for(blr = blocking_lock_queue; blr; blr = next) {
		files_struct *fsp;
		struct byte_range_lock *br_lck;

		next = blr->next;

		if (blr->req->mid != mid) {
			continue;
		}

		fsp = blr->fsp;
		br_lck = brl_get_locks(talloc_tos(), fsp);

		if (br_lck) {
			DEBUG(10, ("remove_pending_lock_requests_by_mid - "
				   "removing request type %d for file %s fnum "
				   "= %d\n", blr->req->cmd, fsp->fsp_name,
				   fsp->fnum ));

			brl_lock_cancel(br_lck,
					blr->lock_pid,
					procid_self(),
					blr->offset,
					blr->count,
					blr->lock_flav,
					blr);
			TALLOC_FREE(br_lck);
		}

		blocking_lock_reply_error(blr,NT_STATUS_FILE_LOCK_CONFLICT);
		DLIST_REMOVE(blocking_lock_queue, blr);
		TALLOC_FREE(blr);
	}
}

/****************************************************************************
 Is this mid a blocking lock request on the queue ?
*****************************************************************************/

bool blocking_lock_was_deferred(int mid)
{
	struct blocking_lock_record *blr, *next = NULL;

	for(blr = blocking_lock_queue; blr; blr = next) {
		next = blr->next;
		if(blr->req->mid == mid) {
			return True;
		}
	}
	return False;
}

/****************************************************************************
  Set a flag as an unlock request affects one of our pending locks.
*****************************************************************************/

static void received_unlock_msg(struct messaging_context *msg,
				void *private_data,
				uint32_t msg_type,
				struct server_id server_id,
				DATA_BLOB *data)
{
	DEBUG(10,("received_unlock_msg\n"));
	process_blocking_lock_queue();
}

/****************************************************************************
 Process the blocking lock queue. Note that this is only called as root.
*****************************************************************************/

void process_blocking_lock_queue(void)
{
	struct timeval tv_curr = timeval_current();
	struct blocking_lock_record *blr, *next = NULL;

	/*
	 * Go through the queue and see if we can get any of the locks.
	 */

	for (blr = blocking_lock_queue; blr; blr = next) {

		next = blr->next;

		/*
		 * Go through the remaining locks and try and obtain them.
		 * The call returns True if all locks were obtained successfully
		 * and False if we still need to wait.
		 */

		DEBUG(10, ("Processing BLR = %p\n", blr));

		/* We use set_current_service so connections with
		 * pending locks are not marked as idle.
		 */

		set_current_service(blr->fsp->conn,
				SVAL(blr->req->inbuf,smb_flg),
				false);

		if(blocking_lock_record_process(blr)) {
			struct byte_range_lock *br_lck = brl_get_locks(
				talloc_tos(), blr->fsp);

			DEBUG(10, ("BLR_process returned true: cancelling and "
			    "removing lock. BLR = %p\n", blr));

			if (br_lck) {
				brl_lock_cancel(br_lck,
					blr->lock_pid,
					procid_self(),
					blr->offset,
					blr->count,
					blr->lock_flav,
					blr);
				TALLOC_FREE(br_lck);
			}

			DLIST_REMOVE(blocking_lock_queue, blr);
			TALLOC_FREE(blr);
			continue;
		}

		/*
		 * We couldn't get the locks for this record on the list.
		 * If the time has expired, return a lock error.
		 */

		if (!timeval_is_zero(&blr->expire_time) && timeval_compare(&blr->expire_time, &tv_curr) <= 0) {
			struct byte_range_lock *br_lck = brl_get_locks(
				talloc_tos(), blr->fsp);

			DEBUG(10, ("Lock timed out! BLR = %p\n", blr));

			/*
			 * Lock expired - throw away all previously
			 * obtained locks and return lock error.
			 */

			if (br_lck) {
				DEBUG(5,("process_blocking_lock_queue: "
					 "pending lock fnum = %d for file %s "
					 "timed out.\n", blr->fsp->fnum,
					 blr->fsp->fsp_name ));

				brl_lock_cancel(br_lck,
					blr->lock_pid,
					procid_self(),
					blr->offset,
					blr->count,
					blr->lock_flav,
					blr);
				TALLOC_FREE(br_lck);
			}

			blocking_lock_reply_error(blr,NT_STATUS_FILE_LOCK_CONFLICT);
			DLIST_REMOVE(blocking_lock_queue, blr);
			TALLOC_FREE(blr);
		}
	}

	recalc_brl_timeout();
}

/****************************************************************************
 Handle a cancel message. Lock already moved onto the cancel queue.
*****************************************************************************/

#define MSG_BLOCKING_LOCK_CANCEL_SIZE (sizeof(struct blocking_lock_record *) + sizeof(NTSTATUS))

static void process_blocking_lock_cancel_message(struct messaging_context *ctx,
						 void *private_data,
						 uint32_t msg_type,
						 struct server_id server_id,
						 DATA_BLOB *data)
{
	NTSTATUS err;
	const char *msg = (const char *)data->data;
	struct blocking_lock_record *blr;

	if (data->data == NULL) {
		smb_panic("process_blocking_lock_cancel_message: null msg");
	}

	if (data->length != MSG_BLOCKING_LOCK_CANCEL_SIZE) {
		DEBUG(0, ("process_blocking_lock_cancel_message: "
			  "Got invalid msg len %d\n", (int)data->length));
		smb_panic("process_blocking_lock_cancel_message: bad msg");
        }

	memcpy(&blr, msg, sizeof(blr));
	memcpy(&err, &msg[sizeof(blr)], sizeof(NTSTATUS));

	DEBUG(10,("process_blocking_lock_cancel_message: returning error %s\n",
		nt_errstr(err) ));

	blocking_lock_reply_error(blr, err);
	DLIST_REMOVE(blocking_lock_cancelled_queue, blr);
	TALLOC_FREE(blr);
}

/****************************************************************************
 Send ourselves a blocking lock cancelled message. Handled asynchronously above.
 Returns the blocking_lock_record that is being cancelled.
*****************************************************************************/

struct blocking_lock_record *blocking_lock_cancel(files_struct *fsp,
			uint32 lock_pid,
			uint64_t offset,
			uint64_t count,
			enum brl_flavour lock_flav,
			unsigned char locktype,
                        NTSTATUS err)
{
	char msg[MSG_BLOCKING_LOCK_CANCEL_SIZE];
	struct blocking_lock_record *blr;

	if (!blocking_lock_cancel_state) {
		/* Register our message. */
		messaging_register(smbd_messaging_context(), NULL,
				   MSG_SMB_BLOCKING_LOCK_CANCEL,
				   process_blocking_lock_cancel_message);

		blocking_lock_cancel_state = True;
	}

	for (blr = blocking_lock_queue; blr; blr = blr->next) {
		if (fsp == blr->fsp &&
				lock_pid == blr->lock_pid &&
				offset == blr->offset &&
				count == blr->count &&
				lock_flav == blr->lock_flav) {
			break;
		}
	}

	if (!blr) {
		return NULL;
	}

	/* Check the flags are right. */
	if (blr->req->cmd == SMBlockingX &&
		(locktype & LOCKING_ANDX_LARGE_FILES) !=
			(CVAL(blr->req->vwv+3, 0) & LOCKING_ANDX_LARGE_FILES)) {
		return NULL;
	}

	/* Move to cancelled queue. */
	DLIST_REMOVE(blocking_lock_queue, blr);
	DLIST_ADD(blocking_lock_cancelled_queue, blr);

	/* Create the message. */
	memcpy(msg, &blr, sizeof(blr));
	memcpy(&msg[sizeof(blr)], &err, sizeof(NTSTATUS));

	messaging_send_buf(smbd_messaging_context(), procid_self(),
			   MSG_SMB_BLOCKING_LOCK_CANCEL,
			   (uint8 *)&msg, sizeof(msg));

	return blr;
}
