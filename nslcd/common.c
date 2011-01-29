/*
   common.c - common server code routines
   This file is part of the nss-pam-ldapd library.

   Copyright (C) 2006 West Consulting
   Copyright (C) 2006, 2007, 2008, 2009 Arthur de Jong

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301 USA
*/

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <limits.h>
#include <netdb.h>
#include <string.h>

#include "nslcd.h"
#include "common.h"
#include "log.h"
#include "attmap.h"

/* simple wrapper around snptintf() to return non-0 in case
   of any failure (but always keep string 0-terminated) */
int mysnprintf(char *buffer,size_t buflen,const char *format, ...)
{
  int res;
  va_list ap;
  /* do snprintf */
  va_start(ap,format);
  res=vsnprintf(buffer,buflen,format,ap);
  /* NULL-terminate the string just to be on the safe side */
  buffer[buflen-1]='\0';
  /* check if the string was completely written */
  return ((res<0)||(((size_t)res)>=buflen));
}

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif /* not HOST_NAME_MAX */

/* return the fully qualified domain name of the current host */
const char *getfqdn(void)
{
  static char *fqdn=NULL;
  char hostname[HOST_NAME_MAX+1];
  int hostnamelen;
  int i;
  struct hostent *host=NULL;
  /* if we already have a fqdn return that */
  if (fqdn!=NULL)
    return fqdn;
  /* get system hostname */
  if (gethostname(hostname,sizeof(hostname))<0)
  {
    log_log(LOG_ERR,"gethostname() failed: %s",strerror(errno));
    return NULL;
  }
  hostnamelen=strlen(hostname);
  /* lookup hostent */
  host=gethostbyname(hostname);
  if (host==NULL)
  {
    log_log(LOG_ERR,"gethostbyname(%s): %s",hostname,hstrerror(h_errno));
    /* fall back to hostname */
    fqdn=strdup(hostname);
    return fqdn;
  }
  /* check h_name for fqdn starting with our hostname */
  if ((strncasecmp(hostname,host->h_name,hostnamelen)==0)&&
      (host->h_name[hostnamelen]=='.')&&
      (host->h_name[hostnamelen+1]!='\0'))
  {
    fqdn=strdup(host->h_name);
    return fqdn;
  }
  /* also check h_aliases */
  for (i=0;host->h_aliases[i]!=NULL;i++)
  {
    if ((strncasecmp(hostname,host->h_aliases[i],hostnamelen)==0)&&
        (host->h_aliases[i][hostnamelen]=='.')&&
        (host->h_aliases[i][hostnamelen+1]!='\0'))
    {
      fqdn=host->h_aliases[i];
      return fqdn;
    }
  }
  /* fall back to h_name if it has a dot in it */
  if (strchr(host->h_name,'.')!=NULL)
  {
    fqdn=strdup(host->h_name);
    return fqdn;
  }
  /* also check h_aliases */
  for (i=0;host->h_aliases[i]!=NULL;i++)
  {
    if (strchr(host->h_aliases[i],'.')!=NULL)
    {
      fqdn=strdup(host->h_aliases[i]);
      return fqdn;
    }
  }
  /* nothing found, fall back to hostname */
  fqdn=strdup(hostname);
  return fqdn;
}

const char *get_userpassword(MYLDAP_ENTRY *entry,const char *attr,char *buffer,size_t buflen)
{
  const char *tmpvalue;
  /* get the value */
  tmpvalue=attmap_get_value(entry,attr,buffer,buflen);
  if (tmpvalue==NULL)
    return NULL;
  /* go over the entries and return the remainder of the value if it
     starts with {crypt} or crypt$ */
  if (strncasecmp(tmpvalue,"{crypt}",7)==0)
    return tmpvalue+7;
  if (strncasecmp(tmpvalue,"crypt$",6)==0)
    return tmpvalue+6;
  /* just return the first value completely */
  return tmpvalue;
  /* TODO: support more password formats e.g. SMD5
    (which is $1$ but in a different format)
    (any code for this is more than welcome) */
}

/*
   Checks to see if the specified name seems to be a valid user or group name.

   This test is based on the definition from POSIX (IEEE Std 1003.1, 2004,
   3.426 User Name, 3.189 Group Name and 3.276 Portable Filename Character Set):
   http://www.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap03.html#tag_03_426
   http://www.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap03.html#tag_03_189
   http://www.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap03.html#tag_03_276

   The standard defines user names valid if they contain characters from
   the set [A-Za-z0-9._-] where the hyphen should not be used as first
   character. As an extension this test allows some more characters.
*/
int isvalidname(const char *name)
{
  int i;
  if ((name==NULL)||(name[0]=='\0'))
    return 0;
  /* check characters */
  for (i=0;name[i]!='\0';i++)
  {
#ifdef LOGIN_NAME_MAX
    if (i>=LOGIN_NAME_MAX)
      return 0;
#endif /* LOGIN_NAME_MAX */
    /* characters supported everywhere in the name */
    if ( (name[i]>='@' && name[i] <= 'Z') ||
         (name[i]>='a' && name[i] <= 'z') ||
         (name[i]>='0' && name[i] <= '9') ||
         name[i]=='.' || name[i]=='_'  || name[i]=='$' )
      continue;
    /* characters that may be anywhere except as first character */
    if ( i>0 && ( name[i]=='-' || name[i]=='~' ) )
      continue;
    /* characters that may not be the first or last character */
    if ( ( i>0 && name[i+1]!='\0' ) && ( name[i]=='\\' || name[i]==' ') )
      continue;
    /* anything else is bad */
    return 0;
  }
  /* no test failed so it must be good */
  return -1;
}

/* this writes a single address to the stream */
int write_address(TFILE *fp,const char *addr)
{
  int32_t tmpint32;
  struct in_addr ipv4addr;
  struct in6_addr ipv6addr;
  /* try to parse the address as IPv4 first, fall back to IPv6 */
  if (inet_pton(AF_INET,addr,&ipv4addr)>0)
  {
    /* write address type */
    WRITE_INT32(fp,AF_INET);
    /* write the address length */
    WRITE_INT32(fp,sizeof(struct in_addr));
    /* write the address itself (in network byte order) */
    WRITE_TYPE(fp,ipv4addr,struct in_addr);
  }
  else if (inet_pton(AF_INET6,addr,&ipv6addr)>0)
  {
    /* write address type */
    WRITE_INT32(fp,AF_INET6);
    /* write the address length */
    WRITE_INT32(fp,sizeof(struct in6_addr));
    /* write the address itself (in network byte order) */
    WRITE_TYPE(fp,ipv6addr,struct in6_addr);
  }
  else
  {
    /* failure, log but write simple invalid address
       (otherwise the address list is messed up) */
    /* TODO: have error message in correct format */
    log_log(LOG_WARNING,"unparseble address: %s",addr);
    /* write an illegal address type */
    WRITE_INT32(fp,-1);
    /* write an emtpy address */
    WRITE_INT32(fp,0);
  }
  /* we're done */
  return 0;
}

int read_address(TFILE *fp,char *addr,int *addrlen,int *af)
{
  int32_t tmpint32;
  int len;
  /* read address family */
  READ_INT32(fp,*af);
  if ((*af!=AF_INET)&&(*af!=AF_INET6))
  {
    log_log(LOG_WARNING,"incorrect address family specified: %d",*af);
    return -1;
  }
  /* read address length */
  READ_INT32(fp,len);
  if ((len>*addrlen)||(len<=0))
  {
    log_log(LOG_WARNING,"address length incorrect: %d",len);
    return -1;
  }
  *addrlen=len;
  /* read address */
  READ(fp,addr,len);
  /* we're done */
  return 0;
}
