/*
   group.c - NSS lookup functions for group database

   Copyright (C) 2006 West Consulting
   Copyright (C) 2006, 2007 Arthur de Jong

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

#include <string.h>
#include <nss.h>
#include <errno.h>

#include "prototypes.h"
#include "common.h"
#include "compat/attrs.h"

static enum nss_status read_group(
        FILE *fp,struct group *result,
        char *buffer,size_t buflen,int *errnop)
{
  int32_t tmpint32,tmp2int32,tmp3int32;
  size_t bufptr=0;
  READ_STRING_BUF(fp,result->gr_name);
  READ_STRING_BUF(fp,result->gr_passwd);
  READ_TYPE(fp,result->gr_gid,gid_t);
  READ_STRINGLIST_NULLTERM(fp,result->gr_mem);
  return NSS_STATUS_SUCCESS;
}

#ifdef REENABLE_WHEN_WORKING
/* read all group entries from the stream and add
   gids of these groups to the list */
static enum nss_status read_gids(
        FILE *fp,long int *start,long int *size,
        gid_t **groupsp,long int limit,int *errnop)
{
  int32_t res=NSLCD_RESULT_SUCCESS;
  int32_t tmpint32,tmp2int32,tmp3int32;
  gid_t gid;
  int num=0;
  /* loop over results */
  while (res==NSLCD_RESULT_SUCCESS)
  {
    /* skip group name */
    SKIP_STRING(fp);
    /* skip passwd entry */
    SKIP_STRING(fp);
    /* read gid */
    READ_TYPE(fp,gid,gid_t);
    /* skip members */
    SKIP_STRINGLIST(fp);
    /* check if entry would fit and we have not returned too many */
    if ( ((*start)>=(*size)) || (num>=limit) )
      { ERROR_OUT_BUFERROR(fp); }
    /* add gid to list */
    (*groupsp)[*start++]=gid;
    num++;
    /* read next response code
      (don't bail out on not success since we just want to build
      up a list) */
    READ_TYPE(fp,res,int32_t);
  }
  /* return the proper status code */
  return (res==NSLCD_RESULT_NOTFOUND)?NSS_STATUS_SUCCESS:nslcd2nss(res);
}
#endif /* REENABLE_WHEN_WORKING */

enum nss_status _nss_ldap_getgrnam_r(const char *name,struct group *result,char *buffer,size_t buflen,int *errnop)
{
  NSS_BYNAME(NSLCD_ACTION_GROUP_BYNAME,
             name,
             read_group(fp,result,buffer,buflen,errnop));
}

enum nss_status _nss_ldap_getgrgid_r(gid_t gid,struct group *result,char *buffer,size_t buflen,int *errnop)
{
  NSS_BYTYPE(NSLCD_ACTION_GROUP_BYGID,
             gid,gid_t,
             read_group(fp,result,buffer,buflen,errnop));
}

#ifdef REENABLE_WHEN_WORKING
/* this function returns a list of groups, documentation for the
   interface is scarce (any pointers are welcome) but this is
   what is assumed the parameters mean:

   user     IN      - the user name to find groups for
   group    ingored - an extra gid to add to the list?
   *start   IN/OUT  - where to write in the array, is incremented
   *size    IN      - the size of the supplied array
   *groupsp IN/OUT  - the array of groupids
   limit    IN      - the maximum number of groups to add
   *errnop  OUT     - for returning errno

   This function cannot grow the array if it becomes too large
   (and will return NSS_STATUS_TRYAGAIN on buffer problem)
   because it has no way of free()ing the buffer.
*/
enum nss_status _nss_ldap_initgroups_dyn(
        const char *user,gid_t group,long int *start,
        long int *size,gid_t **groupsp,long int limit,int *errnop)
{
  NSS_BYNAME(NSLCD_ACTION_GROUP_BYMEMBER,
             user,
             read_gids(fp,start,size,groupsp,limit,errnop));
}
#endif /* REENABLE_WHEN_WORKING */

/* thread-local file pointer to an ongoing request */
static __thread FILE *grentfp;

enum nss_status _nss_ldap_setgrent(int UNUSED(stayopen))
{
  NSS_SETENT(grentfp,NSLCD_ACTION_GROUP_ALL);
}

enum nss_status _nss_ldap_getgrent_r(struct group *result,char *buffer,size_t buflen,int *errnop)
{
  NSS_GETENT(grentfp,read_group(grentfp,result,buffer,buflen,errnop));
}

enum nss_status _nss_ldap_endgrent(void)
{
  NSS_ENDENT(grentfp);
}
