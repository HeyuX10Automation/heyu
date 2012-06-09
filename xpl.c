/*
 * xPL interface for Heyu
 *
 * Copyright (C) 2010, Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBXPL

#include <poll.h>
#include <stdio.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_XPL_H
#include <xPL.h>
#endif

#include "x10.h"

static xPL_ObjectPtr xpl_select_msg_handler(String class, String type)
{
	xPL_ObjectPtr handler = NULL;

	/* Schema based message handler selection logic */
	/* End of message handler selection logic */

	return handler;
}

static void xpl_reconf_listeners(xPL_ServicePtr service,
				 xPL_ObjectPtr signal_src)
{
	Bool enabled = xPL_isServiceEnabled(service);
	int count = xPL_getServiceConfigValueCount(service, "listener");

	if (enabled)
		xPL_setServiceEnabled(service, FALSE);

	if (service->listenerCount > 2)
		service->listenerCount = 2;

	while (count > 0) {
		String value = xPL_getServiceConfigValueAt(service, "listener",
							   --count);
		String msg_type, schema_class, schema_type = NULL;
		xPL_MessageType message_type;
		xPL_ObjectPtr handler;

		if (!value)
			continue;

		msg_type = strdup(value);
		if (!msg_type) {
			fprintf(stderr, "xpl_reconf_listeners(): out of memory, skipping listener %s\n",
					value);
			continue;
		}

		schema_class = strchr(msg_type, '.');
		if (schema_class) {
			*(schema_class++) = '\0';
			schema_type = strchr(schema_class, '.');
		}
		if (schema_type)
			*(schema_type++) = '\0';

		if (strcmp(msg_type, "*") == 0) {
			message_type = xPL_MESSAGE_ANY;
		} else if (xPL_strcmpIgnoreCase(msg_type, "xpl-cmnd") == 0) {
		        message_type = xPL_MESSAGE_COMMAND;
		} else if (xPL_strcmpIgnoreCase(msg_type, "xpl-stat") == 0) {
			message_type = xPL_MESSAGE_STATUS;
		} else if (xPL_strcmpIgnoreCase(msg_type, "xpl-trig") == 0) {
			message_type = xPL_MESSAGE_TRIGGER;
		} else {
			fprintf(stderr, "xpl_reconf_listeners(): bad message type %s, skipping listener %s\n",
					msg_type, value);
			goto free;
		}

		handler = xpl_select_msg_handler(schema_class, schema_type);

		if (handler)
			xPL_addServiceListener(service, handler, message_type,
					schema_class, schema_type, signal_src);
		else
			fprintf(stderr, "xpl_reconf_listeners(): schema %s.%s not supported, skipping listener %s\n",
					schema_class, schema_type, value);
free:
		free(msg_type);
	}

	if (enabled)
		xPL_setServiceEnabled(service, TRUE);
}

#define xpl_add_configurable_array(service, config)	\
	xPL_addServiceConfigurable(service, config, xPL_CONFIG_OPTIONAL, 255)

static xPL_ObjectPtr xpl_setup_subservice(xPL_ServicePtr service,
				       xPL_ObjectPtr *user_value)
{
	if (!xPL_isServiceConfigured(service)) {
		xPL_ServiceFilterPtr fp = malloc(sizeof(xPL_ServiceFilter));

		if (fp) {
			service->messageFilterList = fp;
			service->filterAllocCount = 1;
			memset(fp, 0, sizeof(xPL_ServiceFilter));
		}

		xpl_add_configurable_array(service, "listener");
	}

	return xpl_reconf_listeners;
}

#define xpl_setup_service(name, instance, type, user_value) \
		_xpl_setup_service(name, instance, xpl_setup_##type, user_value)

/*
 * _xpl_setup_service() - a helper function for setting up xPL services.
 * name		the service name,
 * configure	optional initial configuration routine for the service;
 *		should accept a pointer to the service structure, and a pointer
 *		to an optional extra argument intended to be passed to the
 *		service reconfigure handler; may return a pointer to the
 *		service reconfiguration handler, if applicable,
 * user_value	optional extra argument passed to the service reconfiguration
 *		handler; can be modified by the configure routine;
 * return value	TRUE - success, FALSE - failure.
 */
static Bool _xpl_setup_service(String name, String instance,
		xPL_ObjectPtr (*configure)(xPL_ServicePtr, xPL_ObjectPtr *),
		xPL_ObjectPtr user_value)
{
	xPL_ServicePtr service;
	int instance_len = strlen(instance);
	String config_file = malloc(strlen(XPLCONFDIR) + strlen("/") +
			strlen(name) + instance_len + strlen(".xpl") + 1);

	if (!config_file) {
		fprintf(stderr, "xpl_setup_service(%s): out of memory\n", name);
		return FALSE;
	}

	sprintf(config_file, "%s/%s%s.xpl", XPLCONFDIR, name, instance);
	service = xPL_createConfigurableService("heyu", name, config_file);

	free(config_file);

	if (!service) {
		fprintf(stderr, "xpl_setup_service(%s): create failed\n", name);
		return FALSE;
	}

	if (!xPL_isServiceConfigured(service) && instance_len) {
		/*
		 * For XML Plugin Files for xPL enabled Heyu to work as
		 * expected, we have to use a closed set of xPL device IDs,
		 * even if running several Heyu or xPL subservice instances.
		 * Then, the only way to distinguish among them after created
		 * and before configured for the first time is adjusting their
		 * default instance ID to match the instance argument provided.
		 */
		String instance_id = xPL_getServiceInstanceID(service);
		int id_len = strlen(instance_id);

		if (instance_len > id_len)
			instance_len = id_len;

		strncpy(instance_id + (id_len - instance_len), instance,
				instance_len);
		xPL_setServiceInstanceID(service, instance_id);
	}

	xPL_setServiceVersion(service, VERSION);

	if (configure) {
		/* Do custom startup configuration */
		void (*config_handler)(xPL_ServicePtr, xPL_ObjectPtr) =
				configure(service, &user_value);

		if (config_handler) {
			/* Set up custom runtime reconfiguration */
			xPL_addServiceConfigChangedListener(service,
					config_handler, user_value);

			config_handler(service, user_value);
		}
	}

	xPL_setServiceEnabled(service, TRUE);

	return TRUE;
}

static xPL_ObjectPtr xpl_signal_src(String name)
{
	int i;

	for (i = D_CMDLINE; i <= D_AUXRCV; i++) {
		if (xPL_strcmpIgnoreCase(name, (String) source_name[i]) != 0)
			continue;

		switch (i) {
		default:
			return NULL;
		}

		return (xPL_ObjectPtr) &source_type[i];
	}

	return NULL;
}

static void xpl_reconf_subservices(xPL_ServicePtr service,
				   xPL_ObjectPtr user_value)
{
	int count = xPL_getServiceCount();

	while (count > 1) {
		xPL_ServicePtr subservice = xPL_getServiceAt(--count);

		if (subservice)
			xPL_releaseService(subservice);
	}

	count = xPL_getServiceConfigValueCount(service, "subService");
	while (count > 0) {
		String instance = xPL_getServiceConfigValueAt(service,
				"subService", --count);
		xPL_ObjectPtr signal_src;
		static char name[5];

		if (!instance)
			continue;

		/*
		 * Split the user defined subservice name
		 * into a 4 character Heyu signal source name
		 * and an optional instance modifier part.
		 */
		strncpy(name, instance, 4);
		instance += strlen(name);

		/* Use Heyu instance as a last resort */
		if (!strlen(instance))
			instance = optptr->dispsub;

		signal_src = xpl_signal_src(name);

		if (signal_src == NULL)
			fprintf(stderr, "xpl_reconf_subservices(): service name %s not supported, skipping\n",
					name);
		else
			xpl_setup_service(name, instance, subservice,
					  signal_src);
	}
}

static xPL_ObjectPtr xpl_setup_metaservice(xPL_ServicePtr service,
					   xPL_ObjectPtr *user_value)
{
	void (*setup_io)(xPL_ServicePtr) = *user_value;

	if (setup_io)
		setup_io(service);

	if (!xPL_isServiceConfigured(service)) {
		xpl_add_configurable_array(service, "subService");
	}

	return xpl_reconf_subservices;
}

/*
 * xpl_init(): initialize the xPL layer and set up xPL service(s)
 *	       based on the Heyu daemon specific i_am_* flags.
 */
Bool xpl_init(void)
{
	String name = NULL;
	xPL_ObjectPtr setup_io = NULL;

	/* Primary xPL service name and io_setup selection logic goes here */
	/* End of primary xPL service name and io_setup selection logic */

	if (!name) {
		fprintf(stderr, "xpl_init(): unsupported Heyu daemon\n");
		return FALSE;
	}

	if (!xPL_initialize(xPL_getParsedConnectionType())) {
		fprintf(stderr, "xpl_init(): unable to initialize xPL\n");
		return FALSE;
	}

	/*
	 * Take an optional xPL service instance ID
	 * modifier from the Heyu daemon instace number
	 */
	if (xpl_setup_service(name, optptr->dispsub, metaservice, setup_io))
		return TRUE;

	xPL_shutdown();
	return FALSE;
}

#endif
