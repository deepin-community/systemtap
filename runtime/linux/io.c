/* -*- linux-c -*- 
 * I/O for printing warnings, errors and debug messages
 * Copyright (C) 2005-2009 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPLINUX_IO_C_
#define _STAPLINUX_IO_C_

/** @file io.c
 * @brief I/O for printing warnings, errors and debug messages.
 */
/** @addtogroup io I/O
 * @{
 */

#define WARN_STRING "WARNING: "
#define ERR_STRING "ERROR: "

enum code { INFO=0, WARN, ERROR, DBUG };

static void _stp_vlog (enum code type, const char *func, int line, const char *fmt, va_list args)
        __attribute ((format (printf, 4, 0)));

static void _stp_vlog (enum code type, const char *func, int line, const char *fmt, va_list args)
{
	struct _stp_log *log;
	unsigned long flags;
	size_t bytes_avail;
	int num;
	char *buf;
	int start = 0;

	if (!_stp_print_trylock_irqsave(&flags))
		return;

	log = per_cpu_ptr(_stp_log_pcpu, raw_smp_processor_id());
	bytes_avail = STP_BUFFER_SIZE - log->len;
	if (unlikely(!bytes_avail))
		goto err_unlock;

	buf = &log->buf[log->len];
	if (type == DBUG) {
		start = _stp_snprintf(buf, bytes_avail, "%s:%d: ", func, line);
	} else if (type == WARN) {
		strncpy(buf, WARN_STRING, bytes_avail);
		start = min(bytes_avail, sizeof(WARN_STRING) - 1);
	} else if (type == ERROR) {
		strncpy(buf, ERR_STRING, bytes_avail);
		start = min(bytes_avail, sizeof(ERR_STRING) - 1);
	}

	bytes_avail -= start;
	if (unlikely(!bytes_avail))
		goto err_unlock;

	num = vscnprintf(buf + start, bytes_avail - 1, fmt, args);
	if (num + start) {
		if (buf[num + start - 1] != '\n') {
			buf[num + start] = '\n';
			num++;
			buf[num + start] = '\0';
		}

#ifdef STAP_DEBUG_PRINTK
                if (type == DBUG) printk (KERN_DEBUG "%s", buf);
                else if (type == WARN) printk (KERN_WARNING "%s", buf);
                else if (type == ERROR) printk (KERN_ERR "%s", buf);
                else printk (KERN_INFO "%s", buf);
#else
		if (type != DBUG) {
			_stp_ctl_send(STP_OOB_DATA, buf, start + num + 1);
		} else {
			log->len += start + num;
			__stp_print_flush(log);
		}
#endif
	}
err_unlock:
	_stp_print_unlock_irqrestore(&flags);
}

/** Prints warning.
 * This function sends a warning message immediately to staprun. It
 * will also be sent over the bulk transport (relayfs) if it is
 * being used. If the last character is not a newline, then one 
 * is added. 
 * @param fmt A variable number of args.
 */
static void _stp_warn (const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	_stp_vlog (WARN, NULL, 0, fmt, args);
	va_end(args);
}

/** Exits and unloads the module.
 * This function sends a signal to staprun to tell it to
 * unload the module and exit. The module will not be 
 * unloaded until after the current probe returns.
 * @note Be careful to not treat this like the Linux exit() 
 * call. You should probably call return immediately after 
 * calling _stp_exit().
 */
static void _stp_exit (void)
{
	/* Just set the flag since this is possibly called from
	   kprobe context. A timer will come along and call
	   _stp_request_exit() for us.  */
	_stp_exit_flag = 1;
}

/** Prints error message and exits.
 * This function sends an error message immediately to staprun. It
 * will also be sent over the bulk transport (relayfs) if it is
 * being used. If the last character is not a newline, then one 
 * is added. 
 *
 * After the error message is displayed, the module will transition
 * to exiting-state (as if ^C was pressed) and will eventually unload.
 * @param fmt A variable number of args.
 * @sa _stp_exit().
 *
 * NB: this function should not be used from script-accessible tapset
 * functions.  Those should simply set CONTEXT->last_error, so that
 * script-level try/catch blocks can handle them.  This is for random
 * runtime internal matters that a script didn't invoke and can't
 * expect to handle.
 */
static void _stp_error (const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	_stp_vlog (ERROR, NULL, 0, fmt, args);
	va_end(args);
	_stp_exit();
}


/** Prints error message.
 * This function sends an error message immediately to staprun. It
 * will also be sent over the bulk transport (relayfs) if it is
 * being used. If the last character is not a newline, then one 
 * is added. 
 *
 * @param fmt A variable number of args.
 * @sa _stp_error
 */
static void _stp_softerror (const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	_stp_vlog (ERROR, NULL, 0, fmt, args);
	va_end(args);
}


static void _stp_dbug (const char *func, int line, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	_stp_vlog (DBUG, func, line, fmt, args);
	va_end(args);
}

/** @} */
#endif /* _STAPLINUX_IO_C_ */
