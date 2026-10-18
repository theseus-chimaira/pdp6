/* Headless PDP-6: command interpreter, no panel or display transport. */
#include "pdp6.h"

/* These hooks belong to the graphical console, not the CPU. */
void
updatepanel(Apr *apr)
{
    (void)apr;
}

void
writeconsreg(Apr *apr, u32 addr, u32 data)
{
    (void)apr;
    (void)addr;
    (void)data;
}

u32
readconsreg(Apr *apr, u32 addr)
{
    (void)apr;
    (void)addr;
    return 0;
}

int
threadmain(int argc, char *argv[])
{
    Channel *cmdchans[2];
    Task task;
    int cli_id;

    (void)argc;
    (void)argv;
    debugfp = fopen("/dev/null", "w");
    if (debugfp == nil)
        err("cannot open null output");

    rtcchan = chancreate(sizeof(RtcMsg), 20);
    /* The graphical init.ini mounts host-specific panel and tape devices.
     * Start from the self-contained CPU/I/O configuration instead; users
     * may mount media with the normal command interpreter afterwards. */
    defaultconfig();
    if (getdevice("apr") == nil)
        err("need APR device");

    cmdchans[0] = chancreate(sizeof(char *), 1);
    cmdchans[1] = chancreate(sizeof(void *), 1);
    task = (Task){ nil, readcmdchan, cmdchans, 10, 0 };
    addtask(task);

    threadcreate(simthread, nil);
    threadcreate(rtcthread, nil);
    cli_id = threadcreate(cmdthread, cmdchans);
    threadwait(cli_id);
    return 0;
}
