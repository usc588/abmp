#include "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        KASSERT(NULL != dir);
        KASSERT(NULL != name);
        
        if (dir->vn_ops->lookup == NULL||(!S_ISDIR(dir->vn_mode)))
        {
                dbg(DBG_ERROR | DBG_VFS,"ERROR: lookup(): dir has no lookup()\n");
                return -ENOTDIR;
        }
        if(strcmp(name,".")==0||len == 0)
        {
		dbg(DBG_VFS,"name (%s) points to current directory\n",name);
                vref(dir);
                *result=dir;
                return 0;
        }
        if (len > NAME_LEN)
        {
                dbg(DBG_ERROR | DBG_VFS, "ERROR: lookup(): name is too long.\n");
                return -ENAMETOOLONG;
        }
        int i = dir->vn_ops->lookup(dir,name,len,result);       /*Calling lookup for the vnode dir*/
	 if(i<0)                                                 /* Return ENOTDIR if lookup fails*/
        {
                dbg(DBG_VFS,"ERROR: lookup(): dir has no entry with the given name:\n");
                return i;
        }
       /* vref(*result);  Increment the refcount */
        KASSERT(NULL != result);
       /*NOT_YET_IMPLEMENTED("VFS: lookup");*/
        return 0;                                               /* return 0 if succesful */

}
/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,vnode_t *base, vnode_t **res_vnode)
{
	KASSERT(NULL != pathname);
	KASSERT(NULL!=res_vnode); 
	vnode_t *dir_vnode;
        vnode_t *ret_result;
        char *file_name=(char *)pathname;
        char *file_pass=(char *)pathname;
        char *end=(char *)pathname + strlen(pathname);
        dbg(DBG_VFS, "INFO: The Path is (%s)\n",  pathname);
        if (strlen(pathname) > MAXPATHLEN)
        {
                dbg(DBG_ERROR | DBG_VFS, "ERROR: dir_namev(): The Path (%s) is too long.\n",  pathname);
                return -ENAMETOOLONG;
        }
        if (strlen(pathname) == 0)
        {
                dbg(DBG_ERROR | DBG_VFS, "ERROR: dir_namev(): The name is invalid\n");
                return -EINVAL;
        }
        if(pathname[0]=='/')
        {
                dir_vnode=vfs_root_vn;
                vref(dir_vnode);
                file_name++;    
        }
        else if(base == NULL)
        {
                dir_vnode = curproc->p_cwd;
                vref(dir_vnode);
        }
        else
        {
                dir_vnode = base;
                vref(dir_vnode);
        }
       /* if (file_name == end)
        {
                vput(dir_vnode);
                return -ENOENT;
        }*/     
        while(file_pass != end)
        { 
                file_pass = strchr(file_name,'/');
                if(NULL==dir_vnode)
                {
                
                        dbg(DBG_ERROR | DBG_VFS, "ERROR: dir_namev(): lookup failed. a path element does not exist.\n");
                        return -ENOENT;
                }

                if (!S_ISDIR(dir_vnode->vn_mode))
                {
                        dbg(DBG_ERROR | DBG_VFS, "ERROR: dir_namev(): lookup failed. a path element is not a directory.\n");
                        vput(dir_vnode);
                        return -ENOTDIR;
                 }
                 if(file_pass!=NULL)
                 {
                        int kk= file_pass - file_name ;
                        if (kk > NAME_LEN)
                        {
                               dbg(DBG_ERROR | DBG_VFS, "ERROR: dir_namev(): lookup failed. Path component too long.\n");
                               vput(dir_vnode);
                               return -ENAMETOOLONG;
                        }
                        
                        dbg(DBG_VFS, "The name passing to lookup is: (%s), namelen is (%d)\n", file_pass,*namelen);
                        int ii=lookup(dir_vnode,file_name,file_pass - file_name,&ret_result);
                        if(ii<0)
                        {
                               vput(dir_vnode);    
                               return ii;      
                        }
                        vput(dir_vnode);
                        dir_vnode = ret_result;
                        file_pass++;
                        file_name=file_pass;
                        
                 }
                 else
                        break;
        }
       *namelen = end-file_name;
       *res_vnode = dir_vnode;
       *name = (const char*)file_name;

       dbg(DBG_VFS, "INFO: returning from namev.c and the name is (%s), namelen is (%d)\n", file_name,*namelen);


	      
	KASSERT(NULL!=*res_vnode);
	KASSERT(NULL != namelen);
        KASSERT(NULL != name);

        /*NOT_YET_IMPLEMENTED("VFS: dir_namev");*/
        return 0;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
        KASSERT(pathname != NULL);
        size_t namelen=0;
        vnode_t *res_vnode1;
        const char *name=NULL;
        int i = dir_namev(pathname,&namelen,&name,base,&res_vnode1);
	if(i<0)
        {
                return i;
        }
        if (!S_ISDIR(res_vnode1->vn_mode))
        {
                /* Entry is not a directory */
                vput(res_vnode1);
                return -ENOTDIR;
        }
        dbg(DBG_VFS,"Calling lookup() to check for the file existence\n");
        int j=lookup(res_vnode1,name,namelen,res_vnode);     
        if(j<0)
        {
		if(flag&O_CREAT)
                {
        		KASSERT(res_vnode1->vn_ops->create!=NULL);
        		int k=(res_vnode1->vn_ops->create)(res_vnode1,name,namelen,res_vnode);
        		if(k<0)
        		{
        			vput(res_vnode1);
        			return k;
        		}
        	}
        	else
        	{
        	        vput(res_vnode1);
        	        return j;
        	}		
        }        
       /* NOT_YET_IMPLEMENTED("VFS: open_namev");*/
        vput(res_vnode1);
        return 0;
}




#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
