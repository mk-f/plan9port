#include <stdio.h>
#include <kvm.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/sysctl.h>

int get_proc(pid_t pid, pid_t *ppid, pid_t *sid)
{
    char errbuf[_POSIX2_LINE_MAX];
    kvm_t *kernel = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
    if(kernel == NULL)
    	return -1;

    int nentries = 0;
    struct kinfo_proc *kinfo = kvm_getprocs(kernel, KERN_PROC_PID, pid, sizeof(struct kinfo_proc), &nentries);

    if(nentries != 1)
    	return -1;

    *ppid = kinfo[0].p_ppid;
	*sid = kinfo[0].p_sid;

    return 0;
}
