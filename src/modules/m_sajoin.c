/*
 *   IRC - Internet Relay Chat, src/modules/m_sajoin.c
 *   (C) 2004 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_sajoin(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SAJOIN 	"SAJOIN"	

ModuleHeader MOD_HEADER(m_sajoin)
  = {
	"m_sajoin",
	"$Id$",
	"command /sajoin", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_sajoin)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_SAJOIN, m_sajoin, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_sajoin)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_sajoin)(int module_unload)
{
	return MOD_SUCCESS;
}

/* m_sajoin() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
   Coded for Sadmin by Stskeeps
   also Modified by NiQuiL (niquil@programmer.net)
	parv[0] - sender
	parv[1] - nick to make join
	parv[2] - channel(s) to join
*/
DLLFUNC CMD_FUNC(m_sajoin)
{
	aClient *acptr;
	char jbuf[BUFSIZE];
	int did_anything = 0;

	if (!IsSAdmin(sptr) && !IsULine(sptr))
	{
	 sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
	 return 0;
	}

	if (parc < 3)
	{
	 sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SAJOIN");
	 return 0;
	}

	if (!(acptr = find_person(parv[1], NULL)))
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
		return 0;
	}
	if (MyClient(acptr))
	{
		char *name, *p = NULL;
		int i, parted = 0;
	
		*jbuf = 0;

		/* Now works like m_join */
		for (i = 0, name = strtoken(&p, parv[2], ","); name; name = strtoken(&p,
		     NULL, ","))
		{
			aChannel *chptr;
			Membership *lp;

			if (strlen(name) > CHANNELLEN)
				name[CHANNELLEN] = 0;
			clean_channelname(name);
			if (*name == '0' && !atoi(name))
			{
				(void)strcpy(jbuf, "0");
				i = 1;
				parted = 1;
				continue;
			}
			if (*name == '0' || !IsChannelName(name))
			{
				sendto_one(sptr,
				    err_str(ERR_NOSUCHCHANNEL), me.name,
				    parv[0], name);
				continue;
			}

			chptr = get_channel(acptr, name, 0);
			if (!parted && chptr && (lp = find_membership_link(acptr->user->channel, chptr)))
			{
				sendto_one(sptr, err_str(ERR_USERONCHANNEL), me.name, parv[0], 
					   parv[1], name);
				continue;
			}
			if (*jbuf)
				(void)strlcat(jbuf, ",", sizeof jbuf);
			(void)strlncat(jbuf, name, sizeof jbuf, sizeof(jbuf) - i - 1);
			i += strlen(name) + 1;
		}
		if (!*jbuf)
			return -1;
		i = 0;
		strcpy(parv[2], jbuf);
		*jbuf = 0;
		for (name = strtoken(&p, parv[2], ","); name; name = strtoken(&p, NULL, ","))
		{
			int flags;
			aChannel *chptr;
			Membership *lp;
			Hook *h;
			int i = 0;

			if (*name == '0' && !atoi(name))
			{
				did_anything = 1;
				while ((lp = acptr->user->channel))
				{
					chptr = lp->chptr;
					sendto_channel_butserv(chptr, acptr,
					    ":%s PART %s :%s", acptr->name, chptr->chname,
					    "Left all channels");
					if (MyConnect(acptr))
						RunHook4(HOOKTYPE_LOCAL_PART, acptr, acptr, chptr,
							 "Left all channels");
					remove_user_from_channel(acptr, chptr);
				}
				sendto_server(acptr, 0, 0, ":%s JOIN 0", acptr->name);
				strcpy(jbuf, "0");
				continue;
			}
			flags = (ChannelExists(name)) ? CHFL_DEOPPED : CHFL_CHANOP;
			chptr = get_channel(acptr, name, CREATE);
			if (chptr && (lp = find_membership_link(acptr->user->channel, chptr)))
				continue;

			i = HOOK_CONTINUE;
			for (h = Hooks[HOOKTYPE_CAN_SAJOIN]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(acptr,chptr,sptr);
				if (i != HOOK_CONTINUE)
					break;
			}

			if (i == HOOK_DENY)
				continue; /* process next channel */

			join_channel(chptr, acptr, acptr, flags);
			did_anything = 1;
			if (*jbuf)
				strlcat(jbuf, ",", sizeof jbuf);
			strlcat(jbuf, name, sizeof jbuf);
		}
		
		if (did_anything)
		{
			sendnotice(acptr, "*** You were forced to join %s", jbuf);
			sendto_realops("%s used SAJOIN to make %s join %s", sptr->name, acptr->name,
				       jbuf);
			sendto_server(&me, 0, 0, ":%s GLOBOPS :%s used SAJOIN to make %s join %s",
					   me.name, sptr->name, acptr->name, jbuf);
			/* Logging function added by XeRXeS */
			ircd_log(LOG_SACMDS,"SAJOIN: %s used SAJOIN to make %s join %s",
				sptr->name, parv[1], jbuf);
		}
	}
	else
	{
		sendto_one(acptr, ":%s SAJOIN %s %s", parv[0],
		    parv[1], parv[2]);

		/* Logging function added by XeRXeS */
		ircd_log(LOG_SACMDS,"SAJOIN: %s used SAJOIN to make %s join %s",
			sptr->name, parv[1], parv[2]);
	}

	return 0;
}


