/*
 channel-rejoin.c : rejoin to channel if it's "temporarily unavailable"
                    this has nothing to do with autorejoin if kicked

    Copyright (C) 2000 Timo Sirainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "module.h"
#include "signals.h"
#include "misc.h"

#include "irc.h"
#include "irc-channels.h"

#define REJOIN_TIMEOUT (1000*60*5) /* try to rejoin every 5 minutes */

static int rejoin_tag;

#define channel_have_key(chan) \
	((chan) != NULL && (chan)->key != NULL && (chan)->key[0] != '\0')

static void channel_rejoin(IRC_SERVER_REC *server, const char *channel)
{
	IRC_CHANNEL_REC *chanrec;
	char *str;

	chanrec = irc_channel_find(server, channel);
	str = channel_have_key(chanrec) ? g_strdup(channel) :
		g_strdup_printf("%s %s", channel, chanrec->key);

	server->rejoin_channels =
		g_slist_append(server->rejoin_channels, str);
}

static void event_target_unavailable(const char *data, IRC_SERVER_REC *server)
{
	char *params, *channel;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 2, NULL, &channel);
	if (ischannel(*channel)) {
		/* channel is unavailable - try to join again a bit later */
		channel_rejoin(server, channel);
	}

	g_free(params);
}

static void sig_disconnected(IRC_SERVER_REC *server)
{
	if (!IS_IRC_SERVER(server))
		return;

	g_slist_foreach(server->rejoin_channels, (GFunc) g_free, NULL);
	g_slist_free(server->rejoin_channels);
}

static void server_rejoin_channels(IRC_SERVER_REC *server)
{
	while (server->rejoin_channels != NULL) {
		char *channel = server->rejoin_channels->data;

                irc_channels_join(server, channel, TRUE);
		server->rejoin_channels =
			g_slist_remove(server->rejoin_channels, channel);
	}
}

static int sig_rejoin(void)
{
	GSList *tmp;

	for (tmp = servers; tmp != NULL; tmp = tmp->next) {
		IRC_SERVER_REC *rec = tmp->data;

		if (IS_IRC_SERVER(rec))
			server_rejoin_channels(rec);
	}

	return TRUE;
}

void channel_rejoin_init(void)
{
	rejoin_tag = g_timeout_add(REJOIN_TIMEOUT,
				   (GSourceFunc) sig_rejoin, NULL);

	signal_add_first("event 437", (SIGNAL_FUNC) event_target_unavailable);
	signal_add("server disconnected", (SIGNAL_FUNC) sig_disconnected);
}

void channel_rejoin_deinit(void)
{
	g_source_remove(rejoin_tag);

	signal_remove("event 437", (SIGNAL_FUNC) event_target_unavailable);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_disconnected);
}
