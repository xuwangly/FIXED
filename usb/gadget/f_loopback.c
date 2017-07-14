/*
 * f_loopback.c - USB peripheral loopback configuration driver
 *
 * Copyright (C) 2003-2008 David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/* #define VERBOSE_DEBUG */
#define VERBOSE_DEBUG

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/usb/composite.h>
#include <linux/kthread.h>

#include "g_zero.h"
#include "u_f.h"

/*
 * LOOPBACK FUNCTION ... a testing vehicle for USB peripherals,
 *
 * This takes messages of various sizes written OUT to a device, and loops
 * them back so they can be read IN from it.  It has been used by certain
 * test applications.  It supports limited testing of data queueing logic.
 *
 *
 * This is currently packaged as a configuration driver, which can't be
 * combined with other functions to make composite devices.  However, it
 * can be combined with other independent configurations.
 */
struct f_loopback {
	struct usb_function	function;

	struct usb_ep		*in_ep;
	struct usb_ep		*out_ep;
};

static inline struct f_loopback *func_to_loop(struct usb_function *f)
{
	return container_of(f, struct f_loopback, function);
}

static unsigned qlen;
static unsigned buflen;

/*-------------------------------------------------------------------------*/

static struct usb_interface_descriptor loopback_intf = {
	.bLength =		sizeof loopback_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor fs_loop_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_loop_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_loopback_descs[] = {
	(struct usb_descriptor_header *) &loopback_intf,
	(struct usb_descriptor_header *) &fs_loop_sink_desc,
	(struct usb_descriptor_header *) &fs_loop_source_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor hs_loop_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_loop_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *hs_loopback_descs[] = {
	(struct usb_descriptor_header *) &loopback_intf,
	(struct usb_descriptor_header *) &hs_loop_source_desc,
	(struct usb_descriptor_header *) &hs_loop_sink_desc,
	NULL,
};

/* super speed support: */

static struct usb_endpoint_descriptor ss_loop_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_loop_source_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_endpoint_descriptor ss_loop_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_loop_sink_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_descriptor_header *ss_loopback_descs[] = {
	(struct usb_descriptor_header *) &loopback_intf,
	(struct usb_descriptor_header *) &ss_loop_source_desc,
	(struct usb_descriptor_header *) &ss_loop_source_comp_desc,
	(struct usb_descriptor_header *) &ss_loop_sink_desc,
	(struct usb_descriptor_header *) &ss_loop_sink_comp_desc,
	NULL,
};

/* function-specific strings: */

static struct usb_string strings_loopback[] = {
	[0].s = "loop input to output",
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_loop = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_loopback,
};

static struct usb_gadget_strings *loopback_strings[] = {
	&stringtab_loop,
	NULL,
};

/*-------------------------------------------------------------------------*/

static int loopback_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_loopback	*loop = func_to_loop(f);
	int			id;
	int ret;
	pr_err("[LOL]loopback_bind!!!\n");
	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	pr_err("[LOL]usb_interface_id:%d!!!\n" , id);
	loopback_intf.bInterfaceNumber = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_loopback[0].id = id;
	loopback_intf.iInterface = id;

	/* allocate endpoints */

	loop->in_ep = usb_ep_autoconfig(cdev->gadget, &fs_loop_source_desc);
	if (!loop->in_ep) {
autoconf_fail:
		ERROR(cdev, "%s: can't autoconfigure on %s\n",
			f->name, cdev->gadget->name);
		return -ENODEV;
	}
	loop->in_ep->driver_data = cdev;	/* claim */

	loop->out_ep = usb_ep_autoconfig(cdev->gadget, &fs_loop_sink_desc);
	if (!loop->out_ep)
		goto autoconf_fail;
	loop->out_ep->driver_data = cdev;	/* claim */

	
	pr_err("[LOL]usb_ep_autoconfig success!!!\n");
	
	/* support high speed hardware */
	hs_loop_source_desc.bEndpointAddress =
		fs_loop_source_desc.bEndpointAddress;
	hs_loop_sink_desc.bEndpointAddress = fs_loop_sink_desc.bEndpointAddress;

	/* support super speed hardware */
	ss_loop_source_desc.bEndpointAddress =
		fs_loop_source_desc.bEndpointAddress;
	ss_loop_sink_desc.bEndpointAddress = fs_loop_sink_desc.bEndpointAddress;

	ret = usb_assign_descriptors(f, fs_loopback_descs, hs_loopback_descs,
			ss_loopback_descs);
	if (ret)
		return ret;
	pr_err("[LOL]%s speed %s: IN/%s, OUT/%s\n",
	    (gadget_is_superspeed(c->cdev->gadget) ? "super" :
	     (gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full")),
			f->name, loop->in_ep->name, loop->out_ep->name);
	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
	    (gadget_is_superspeed(c->cdev->gadget) ? "super" :
	     (gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full")),
			f->name, loop->in_ep->name, loop->out_ep->name);
	return 0;
}

static void lb_free_func(struct usb_function *f)
{
	struct f_lb_opts *opts;
	pr_err("[LOL]lb_free_func!!!\n");
	opts = container_of(f->fi, struct f_lb_opts, func_inst);

	mutex_lock(&opts->lock);
	opts->refcnt--;
	mutex_unlock(&opts->lock);

	usb_free_all_descriptors(f);
	kfree(func_to_loop(f));
}
void myfunc(struct work_struct*work);
DECLARE_WORK(mywork,myfunc);                  //定义


void myfunc(struct work_struct*ws)
{
   pr_err("myfunc!!!\n");
   //ssleep(1);
   //pr_err(KERN_ALERT "myfunc current->pid %d\n",current->pid);
   //ssleep(1);
   //pr_err(KERN_ALERT "myfunc current->pid %d\n",current->pid);
   //ssleep(1);
}
#if 0
static void loopback_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_loopback	*loop = ep->driver_data;
	struct usb_composite_dev *cdev = loop->function.config->cdev;
	int			status = req->status;
	schedule_work(&mywork);
	//pr_err("[LOL]now int loopback_complete\n");
	//pr_err("ep_name:%s ep_address:%c\n" , 
	//		ep->name , 
	//		ep->address
	//		);
	//pr_err("usb_request_buff:%s usb_request_length:%u\n" , 
	//		(char*)req->buf , 
	//		req->length
	//		);
	switch (status) {

	case 0:				/* normal completion? */
		if (ep == loop->out_ep) {
			req->zero = (req->actual < req->length);
			req->length = req->actual;
			status = usb_ep_queue(loop->in_ep, req, GFP_ATOMIC);
			if (status == 0)
				return;
		}
		memset(req->buf , 'c' , buflen);
		/* queue the buffer for some later OUT packet */
		req->length = buflen;
		status = usb_ep_queue(ep, req, GFP_ATOMIC);
		if (status == 0)
			return;

		/* "should never get here" */
		/* FALLTHROUGH */

	default:
		ERROR(cdev, "%s loop complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		/* FALLTHROUGH */

	/* NOTE:  since this driver doesn't maintain an explicit record
	 * of requests it submitted (just maintains qlen count), we
	 * rely on the hardware driver to clean up on disconnect or
	 * endpoint disable.
	 */
	case -ECONNABORTED:		/* hardware forced ep reset */
	case -ECONNRESET:		/* request dequeued */
	case -ESHUTDOWN:		/* disconnect from host */
		free_ep_req(ep, req);
		return;
	}
}
#else 
static void loopback_complete(struct usb_ep *ep, struct usb_request *req)
{
	//struct f_loopback	*loop = ep->driver_data;
	//struct usb_composite_dev *cdev = loop->function.config->cdev;
	//int			status = req->status;
	static unsigned char c = 0;
	struct f_loopback	*loop = ep->driver_data;

	pr_err("[LOL]loopback_complete!!!\n");
	if (ep == loop->out_ep){
		c++;
		memset(req->buf, c , req->length);
		usb_ep_queue(ep, req, GFP_ATOMIC);

	}else if(ep == loop->in_ep){
		((char *)req->buf)[req->length-1] = '\0';
		pr_err("[LOL]recv:%s\n" ,(char *)req->buf );
		usb_ep_queue(ep, req, GFP_ATOMIC);
	}
	#if 0 
	switch (status) {
	case 0:				/* normal completion? */
		if (ep == loop->out_ep) {
			/*
			 * We received some data from the host so let's
			 * queue it so host can read the from our in ep
			 */
			struct usb_request *in_req = req->context;

			in_req->zero = (req->actual < req->length);
			in_req->length = req->actual;
			ep = loop->in_ep;
			req = in_req;
		} else {
			/*
			 * We have just looped back a bunch of data
			 * to host. Now let's wait for some more data.
			 */
			req = req->context;
			ep = loop->out_ep;
		}

		/* queue the buffer back to host or for next bunch of data */
		status = usb_ep_queue(ep, req, GFP_ATOMIC);
		if (status == 0) {
			return;
		} else {
			ERROR(cdev, "Unable to loop back buffer to %s: %d\n",
			      ep->name, status);
			goto free_req;
		}

		/* "should never get here" */
	default:
		ERROR(cdev, "%s loop complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		/* FALLTHROUGH */

	/* NOTE:  since this driver doesn't maintain an explicit record
	 * of requests it submitted (just maintains qlen count), we
	 * rely on the hardware driver to clean up on disconnect or
	 * endpoint disable.
	 */
	case -ECONNABORTED:		/* hardware forced ep reset */
	case -ECONNRESET:		/* request dequeued */
	case -ESHUTDOWN:		/* disconnect from host */
free_req:
		usb_ep_free_request(ep == loop->in_ep ?
				    loop->out_ep : loop->in_ep,
				    req->context);
		free_ep_req(ep, req);
		return;
	}
	#endif
}

#endif
static void disable_loopback(struct f_loopback *loop)
{
	struct usb_composite_dev	*cdev;

	cdev = loop->function.config->cdev;
	disable_endpoints(cdev, loop->in_ep, loop->out_ep, NULL, NULL, NULL,
			NULL);
	VDBG(cdev, "%s disabled\n", loop->function.name);
}

static inline struct usb_request *lb_alloc_ep_req(struct usb_ep *ep, int len)
{
	return alloc_ep_req(ep, len, buflen);
}

static int enable_endpoint(struct usb_composite_dev *cdev, struct f_loopback *loop,
		struct usb_ep *ep)
{
	//struct usb_request			*req;
	//unsigned				i;
	int					result;

	pr_err("[LOL]enable_endpoint!!!\n");
	pr_err("[LOL]name:%s\n" , ep->name);
	/*
	 * one endpoint writes data back IN to the host while another endpoint
	 * just reads OUT packets
	 */
	result = config_ep_by_speed(cdev->gadget, &(loop->function), ep);
	if (result)
		goto fail0;
	result = usb_ep_enable(ep);
	if (result < 0)
		goto fail0;
	ep->driver_data = loop;

	/*
	 * allocate a bunch of read buffers and queue them all at once.
	 * we buffer at most 'qlen' transfers; fewer if any need more
	 * than 'buflen' bytes each.
	 */
	 /*
	 if(ep->name == loop->in_ep)
	 	return 0;
	for (i = 0; i < qlen && result == 0; i++) {
		req = lb_alloc_ep_req(ep, 0);
		if (!req)
			goto fail1;

		req->complete = loopback_complete;
		result = usb_ep_queue(ep, req, GFP_ATOMIC);
		ERROR(cdev,"[LOL]usb_ep_queue, result:%d!!!\n",result);
		if (result) {
			ERROR(cdev, "%s queue req --> %d\n",
					ep->name, result);
			goto fail1;
		}
	}*/

	return 0;

//fail1:
//	usb_ep_disable(ep);

fail0:
	pr_err("[LOL]enable_endpoint failed!!!\n");
	return result;
}
struct send_data {
	struct usb_ep *ep;
	char c ;
};

#if 0
//static struct usb_request *send_data(struct usb_ep *ep, char c)//构造并发送读请求  
static int send_data(void *data)

{  
	struct usb_ep *ep; 
    struct usb_request *req;  
	char c = 0;
    int status;  
	ep = data;
	req = alloc_ep_req(ep, 1024 , 1024);  
	if (!req)  
		continue;  
	memset(req->buf, c , req->length);	
	req->complete = loopback_complete; //请求完成函数  
	status = usb_ep_queue(ep, req, GFP_ATOMIC); //递交请求	
	if (status)   
	{  
		 //struct zero_dev *dev = ep->driver_data;  
		 printk("start %s --> %d/n", ep->name, status);  
		 free_ep_req(ep, req);	
		 req = NULL;  
	}  
	while(1){
		c++;
		memset(req->buf, c , req->length);
		msleep(1000);
	}
	return 0;
} 


static int alloc_requests(struct usb_composite_dev *cdev,
			  struct f_loopback *loop)
{
	struct usb_request *in_req;
	//struct usb_request *in_req, *out_req;
	//int i;
	//int result = 0;
	/**
 * kthread_run - create and wake a thread.
 * @threadfn: the function to run until signal_pending(current).
 * @data: data ptr for @threadfn.
 * @namefmt: printf-style name for the thread.
 *
 * Description: Convenient wrapper for kthread_create() followed by
 * wake_up_process().  Returns the kthread or ERR_PTR(-ENOMEM).
 */
 		in_req = usb_ep_alloc_request(loop->in_ep, GFP_ATOMIC);
		in_req = lb_alloc_ep_req(loop->in_ep, 512);
		if (!in_req)
			return 0;
		in_req->complete = loopback_complete;
		usb_ep_queue(loop->out_ep, out_req, GFP_ATOMIC);
	 //kthread_run(send_data , loop->in_ep, "send_data");
	return 0;
	/*
	 * allocate a bunch of read buffers and queue them all at once.
	 * we buffer at most 'qlen' transfers; We allocate buffers only
	 * for out transfer and reuse them in IN transfers to implement
	 * our loopback functionality
	 */
	 #if 0
	//for (i = 0; i < loop->qlen && result == 0; i++) {
	for (i = 0; i < qlen && result == 0; i++) {
		result = -ENOMEM;

		//in_req = usb_ep_alloc_request(loop->in_ep, GFP_ATOMIC);
		in_req = lb_alloc_ep_req(loop->in_ep, 512);
		if (!in_req)
			goto fail;

		out_req = lb_alloc_ep_req(loop->out_ep, 512);
		if (!out_req)
			goto fail_in;

		in_req->complete = loopback_complete;
		out_req->complete = loopback_complete;

		in_req->buf = out_req->buf;
		/* length will be set in complete routine */
		in_req->context = out_req;
		out_req->context = in_req;

		result = usb_ep_queue(loop->out_ep, out_req, GFP_ATOMIC);
		if (result) {
			ERROR(cdev, "%s queue req --> %d\n",
					loop->out_ep->name, result);
			goto fail_out;
		}
		memset(in_req->buf , 'Y', in_req->length);
		usb_ep_queue(loop->in_ep, in_req, GFP_ATOMIC);
	}

	return 0;

fail_out:
	free_ep_req(loop->out_ep, out_req);
fail_in:
	free_ep_req(loop->in_ep, in_req);
fail:
	return result;
	#endif
}
#endif
static int alloc_requests(struct usb_composite_dev *cdev,
			  struct f_loopback *loop)
{
	struct usb_request *in_req, *out_req;
	//int i;
	int result = 0;

	//send data
	in_req = lb_alloc_ep_req(loop->in_ep, 1024*1024*2);
	in_req->complete = loopback_complete;
	memset(in_req->buf, 0 , in_req->length);
	result = usb_ep_queue(loop->in_ep, in_req, GFP_ATOMIC);

	//recv_data
	out_req = lb_alloc_ep_req(loop->out_ep, 512);
	out_req->complete = loopback_complete;
	memset(out_req->buf, 0 , out_req->length);
	result = usb_ep_queue(loop->out_ep, out_req, GFP_ATOMIC);
	return result;
	#if 0 
	/*
	 * allocate a bunch of read buffers and queue them all at once.
	 * we buffer at most 'qlen' transfers; We allocate buffers only
	 * for out transfer and reuse them in IN transfers to implement
	 * our loopback functionality
	 */
	for (i = 0; i < loop->qlen && result == 0; i++) {
		result = -ENOMEM;

		in_req = usb_ep_alloc_request(loop->in_ep, GFP_ATOMIC);
		if (!in_req)
			goto fail;

		out_req = lb_alloc_ep_req(loop->out_ep, loop->buflen);
		if (!out_req)
			goto fail_in;

		in_req->complete = loopback_complete;
		out_req->complete = loopback_complete;

		in_req->buf = out_req->buf;
		/* length will be set in complete routine */
		in_req->context = out_req;
		out_req->context = in_req;

		result = usb_ep_queue(loop->out_ep, out_req, GFP_ATOMIC);
		if (result) {
			ERROR(cdev, "%s queue req --> %d\n",
					loop->out_ep->name, result);
			goto fail_out;
		}
	}

	return 0;

fail_out:
	free_ep_req(loop->out_ep, out_req);
fail_in:
	usb_ep_free_request(loop->in_ep, in_req);
fail:
	return result;
	#endif
}

static int
enable_loopback(struct usb_composite_dev *cdev, struct f_loopback *loop)
{
	int					result = 0;

	pr_err("[LOL]enable_loopback!!!\n");
	
	result = enable_endpoint(cdev, loop, loop->in_ep);
	if (result)
		goto out;

	result = enable_endpoint(cdev, loop, loop->out_ep);
	if (result)
		goto disable_in;

	result = alloc_requests(cdev, loop);
	if (result)
		goto disable_out;
	
	pr_err( "[LOL]%s enabled\n", loop->function.name);
	DBG(cdev, "%s enabled\n", loop->function.name);
	return result;
	
disable_out:
	usb_ep_disable(loop->out_ep);
disable_in:
	usb_ep_disable(loop->in_ep);
out:
	return result;
}

static int loopback_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	
	struct f_loopback	*loop = func_to_loop(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	
	pr_err("[LOL]loopback_set_alt!!!\n");
	/* we know alt is zero */
	if (loop->in_ep->driver_data){
		pr_err("[LOL]disable_loopback!!!\n");
		disable_loopback(loop);
	}
	return enable_loopback(cdev, loop);
}

static void loopback_disable(struct usb_function *f)
{
	struct f_loopback	*loop = func_to_loop(f);
	pr_err("[LOL]loopback_disable!!!\n");
	disable_loopback(loop);
}

struct usb_function *loopback_alloc(struct usb_function_instance *fi)
{
	struct f_loopback	*loop;
	struct f_lb_opts	*lb_opts;

	loop = kzalloc(sizeof *loop, GFP_KERNEL);
	if (!loop)
		return ERR_PTR(-ENOMEM);

	lb_opts = container_of(fi, struct f_lb_opts, func_inst);

	mutex_lock(&lb_opts->lock);
	lb_opts->refcnt++;
	mutex_unlock(&lb_opts->lock);

	buflen = lb_opts->bulk_buflen;
	qlen = lb_opts->qlen;
	if (!qlen)
		qlen = 32;

	loop->function.name = "loopback";
	loop->function.bind = loopback_bind;
	loop->function.set_alt = loopback_set_alt;
	loop->function.disable = loopback_disable;
	loop->function.strings = loopback_strings;

	loop->function.free_func = lb_free_func;

	return &loop->function;
}

static inline struct f_lb_opts *to_f_lb_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_lb_opts,
			    func_inst.group);
}

CONFIGFS_ATTR_STRUCT(f_lb_opts);
CONFIGFS_ATTR_OPS(f_lb_opts);

static void lb_attr_release(struct config_item *item)
{
	struct f_lb_opts *lb_opts = to_f_lb_opts(item);

	usb_put_function_instance(&lb_opts->func_inst);
}

static struct configfs_item_operations lb_item_ops = {
	.release		= lb_attr_release,
	.show_attribute		= f_lb_opts_attr_show,
	.store_attribute	= f_lb_opts_attr_store,
};

static ssize_t f_lb_opts_qlen_show(struct f_lb_opts *opts, char *page)
{
	int result;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%d", opts->qlen);
	mutex_unlock(&opts->lock);

	return result;
}

static ssize_t f_lb_opts_qlen_store(struct f_lb_opts *opts,
				    const char *page, size_t len)
{
	int ret;
	u32 num;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = kstrtou32(page, 0, &num);
	if (ret)
		goto end;

	opts->qlen = num;
	ret = len;
end:
	mutex_unlock(&opts->lock);
	return ret;
}

static struct f_lb_opts_attribute f_lb_opts_qlen =
	__CONFIGFS_ATTR(qlen, S_IRUGO | S_IWUSR,
			f_lb_opts_qlen_show,
			f_lb_opts_qlen_store);

static ssize_t f_lb_opts_bulk_buflen_show(struct f_lb_opts *opts, char *page)
{
	int result;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%d", opts->bulk_buflen);
	mutex_unlock(&opts->lock);

	return result;
}

static ssize_t f_lb_opts_bulk_buflen_store(struct f_lb_opts *opts,
				    const char *page, size_t len)
{
	int ret;
	u32 num;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = kstrtou32(page, 0, &num);
	if (ret)
		goto end;

	opts->bulk_buflen = num;
	ret = len;
end:
	mutex_unlock(&opts->lock);
	return ret;
}

static struct f_lb_opts_attribute f_lb_opts_bulk_buflen =
	__CONFIGFS_ATTR(buflen, S_IRUGO | S_IWUSR,
			f_lb_opts_bulk_buflen_show,
			f_lb_opts_bulk_buflen_store);

static struct configfs_attribute *lb_attrs[] = {
	&f_lb_opts_qlen.attr,
	&f_lb_opts_bulk_buflen.attr,
	NULL,
};

static struct config_item_type lb_func_type = {
	.ct_item_ops    = &lb_item_ops,
	.ct_attrs	= lb_attrs,
	.ct_owner       = THIS_MODULE,
};

static void lb_free_instance(struct usb_function_instance *fi)
{
	struct f_lb_opts *lb_opts;

	lb_opts = container_of(fi, struct f_lb_opts, func_inst);
	kfree(lb_opts);
}

struct usb_function_instance *loopback_alloc_instance(void)
{
	struct f_lb_opts *lb_opts;

	lb_opts = kzalloc(sizeof(*lb_opts), GFP_KERNEL);
	if (!lb_opts)
		return ERR_PTR(-ENOMEM);
	mutex_init(&lb_opts->lock);
	lb_opts->func_inst.free_func_inst = lb_free_instance;
	lb_opts->bulk_buflen = GZERO_BULK_BUFLEN;
	lb_opts->qlen = GZERO_QLEN;

	config_group_init_type_name(&lb_opts->func_inst.group, "",
				    &lb_func_type);

	return  &lb_opts->func_inst;
}
DECLARE_USB_FUNCTION(Loopback, loopback_alloc_instance, loopback_alloc);

int __init lb_modinit(void)
{
	int ret;

	ret = usb_function_register(&Loopbackusb_func);
	if (ret)
		return ret;
	return ret;
}
void __exit lb_modexit(void)
{
	usb_function_unregister(&Loopbackusb_func);
}

MODULE_LICENSE("GPL");